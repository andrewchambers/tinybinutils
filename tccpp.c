/*
 *  TCC - Tiny C Compiler
 * 
 *  Copyright (c) 2001-2004 Fabrice Bellard
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#define USING_GLOBALS
#include "tcc.h"

/* #define to 1 to enable (see parse_pp_string()) */
#define ACCEPT_LF_IN_STRINGS 0

/********************************************************/
/* global variables */

ST_DATA int tok_flags;
ST_DATA int parse_flags;

ST_DATA struct BufferedFile *file;
ST_DATA int tok;
ST_DATA CValue tokc;
ST_DATA const int *token_stream_ptr;
ST_DATA CString tokcstr; /* current parsed string, if any */

/* display benchmark infos */
ST_DATA int tok_ident;
ST_DATA TokenSym **table_ident;

/* ------------------------------------------------------------------------- */

static TokenSym *hash_ident[TOK_HASH_SIZE];
static char token_buf[STRING_MAX_SIZE + 1];
static CString cstr_buf;
static TokenString unget_buf;
static unsigned char isidnum_table[256 - CH_EOF];
static void next_nomacro(void);
static void parse_string(const char *p, int len);

static struct TinyAlloc *toksym_alloc;
static struct TinyAlloc *tokstr_alloc;

static TokenString *token_stream_stack;

static const char tcc_keywords[] = 
#define DEF(id, str) str "\0"
#include "tcctok.h"
#undef DEF
;

/* WARNING: the content of this string encodes token numbers */
static const unsigned char tok_two_chars[] =
/* outdated -- gr
    "<=\236>=\235!=\225&&\240||\241++\244--\242==\224<<\1>>\2+=\253"
    "-=\255*=\252/=\257%=\245&=\246^=\336|=\374->\313..\250##\266";
*/{
    '<','=', TOK_LE,
    '>','=', TOK_GE,
    '!','=', TOK_NE,
    '&','&', TOK_LAND,
    '|','|', TOK_LOR,
    '+','+', TOK_INC,
    '-','-', TOK_DEC,
    '=','=', TOK_EQ,
    '<','<', TOK_SHL,
    '>','>', TOK_SAR,
    '+','=', TOK_A_ADD,
    '-','=', TOK_A_SUB,
    '*','=', TOK_A_MUL,
    '/','=', TOK_A_DIV,
    '%','=', TOK_A_MOD,
    '&','=', TOK_A_AND,
    '^','=', TOK_A_XOR,
    '|','=', TOK_A_OR,
    '-','>', TOK_ARROW,
    '.','.', TOK_TWODOTS,
    '#','#', TOK_TWOSHARPS,
    0
};

ST_FUNC void skip(int c)
{
    if (tok != c) {
        char tmp[40];
        pstrcpy(tmp, sizeof tmp, get_tok_str(c, &tokc));
        tcc_error("'%s' expected (got '%s')", tmp, get_tok_str(tok, &tokc));
	}
    next();
}

ST_FUNC void expect(const char *msg)
{
    tcc_error("%s expected", msg);
}

/* ------------------------------------------------------------------------- */
/* Custom allocator for tiny objects */

#define USE_TAL

#ifndef USE_TAL /* may cause memory leaks after errors */
#define tal_free(al, p) tcc_free(p)
#define tal_realloc(al, p, size) tcc_realloc(p, size)
#define tal_new(a,b)
#define tal_delete(a)
#else
#if !defined(MEM_DEBUG)
#define tal_free(al, p) tal_free_impl(al, p)
#define tal_realloc(al, p, size) tal_realloc_impl(al, p, size)
#define TAL_DEBUG_PARAMS
#else
#define TAL_DEBUG MEM_DEBUG
//#define TAL_INFO 1 /* collect and dump allocators stats */
#define tal_free(al, p) tal_free_impl(al, p, __FILE__, __LINE__)
#define tal_realloc(al, p, size) tal_realloc_impl(al, p, size, __FILE__, __LINE__)
#define TAL_DEBUG_PARAMS , const char *sfile, int sline
#endif

#define TOKSYM_TAL_SIZE (256 * 1024) /* allocator for TokenSym in table_ident */
#define TOKSTR_TAL_SIZE (256 * 1024) /* allocator for TokenString instances */

typedef struct TinyAlloc {
    uint8_t *p;
    uint8_t *bufend;
    struct TinyAlloc *next;
    unsigned nb_allocs;
    unsigned size;
#if TAL_INFO
    unsigned nb_peak;
    unsigned nb_total;
    uint8_t *peak_p;
#endif
    union {
        uint8_t buffer[1];
        size_t _aligner_;
    };
} TinyAlloc;

typedef struct tal_header_t {
    size_t  size; /* word align */
#if TAL_DEBUG
    int     line_num; /* negative line_num used for double free check */
    char    file_name[40];
#endif
} tal_header_t;

#define TAL_ALIGN(size) \
    (((size) + (sizeof (size_t) - 1)) & ~(sizeof (size_t) - 1))

/* ------------------------------------------------------------------------- */

static TinyAlloc *tal_new(TinyAlloc **pal, unsigned size)
{
    TinyAlloc *al = tcc_malloc(sizeof(TinyAlloc) - sizeof (size_t) + size);
    al->p = al->buffer;
    al->bufend = al->buffer + size;
    al->nb_allocs = 0;
    al->next = *pal, *pal = al;
    al->size = al->next ? al->next->size : size;
#if TAL_INFO
    al->nb_peak = 0;
    al->nb_total = 0;
    al->peak_p = al->p;
#endif
    return al;
}

static void tal_delete(TinyAlloc **pal)
{
    TinyAlloc *al = *pal, *next;

#if TAL_INFO
    fprintf(stderr, "tal_delete (&tok%s_alloc):\n", pal == &toksym_alloc ? "sym" : "str");
#endif
tail_call:
#if TAL_DEBUG && TAL_DEBUG != 3 /* do not check TAL leaks with -DMEM_DEBUG=3 */
#if TAL_INFO
    fprintf(stderr, "  size %7d  nb_peak %5d  nb_total %6d  usage %5.1f%%\n",
            al->bufend - al->buffer, al->nb_peak, al->nb_total,
            (al->peak_p - al->buffer) * 100.0 / (al->bufend - al->buffer));
#endif
    if (al->nb_allocs > 0) {
        uint8_t *p;
        fprintf(stderr, "TAL_DEBUG: memory leak %d chunk(s)\n", al->nb_allocs);
        p = al->buffer;
        while (p < al->p) {
            tal_header_t *header = (tal_header_t *)p;
            if (header->line_num > 0) {
                fprintf(stderr, "%s:%d: chunk of %d bytes leaked\n",
                        header->file_name, header->line_num, (int)header->size);
            }
            p += header->size + sizeof(tal_header_t);
        }
#if TAL_DEBUG == 2
        exit(2);
#endif
    }
#endif
    next = al->next;
    tcc_free(al);
    al = next;
    if (al)
        goto tail_call;
    *pal = al;
}

static void tal_free_impl(TinyAlloc **pal, void *p TAL_DEBUG_PARAMS)
{
    TinyAlloc *al, **top = pal;
    tal_header_t *header;

    if (!p)
        return;
    header = (tal_header_t *)p - 1;
#if TAL_DEBUG
    if (header->line_num < 0) {
        fprintf(stderr, "%s:%d: TAL_DEBUG: double frees chunk from\n",
                sfile, sline);
        fprintf(stderr, "%s:%d: %d bytes\n",
                header->file_name, (int)-header->line_num, (int)header->size);
    } else
        header->line_num = -header->line_num;
#endif
    al = *pal;
    while ((uint8_t*)p < al->buffer || (uint8_t*)p > al->bufend)
        al = *(pal = &al->next);
    if (0 == --al->nb_allocs) {
        *pal = al->next;
        if ((al->bufend - al->buffer) > al->size) {
            //fprintf(stderr, "free big tal: %u\n", header->size);
            tcc_free(al);
        } else {
            /* reset and move to front */
            al->p = al->buffer;
            al->next = *top, *top = al;
        }
    } else if ((uint8_t*)p + header->size == al->p) {
        al->p = (uint8_t*)header;
    }
}

static void *tal_realloc_impl(TinyAlloc **pal, void *p, unsigned size TAL_DEBUG_PARAMS)
{
    tal_header_t *header;
    void *ret;
    unsigned adj_size = TAL_ALIGN(size) + sizeof(tal_header_t);
    TinyAlloc *al = *pal;

    if (p) {
        /* reallpc case */
        while ((uint8_t*)p < al->buffer || (uint8_t*)p > al->bufend)
            al = al->next;
        header = (tal_header_t *)p - 1;
        if ((uint8_t*)p + header->size == al->p)
            al->p = (uint8_t*)header; /* maybe reuse */
        if (al->p + adj_size > al->bufend) {
            ret = tal_realloc(pal, 0, size);
            memcpy(ret, p, header->size);
            tal_free(pal, p);
            return ret;
        } else if (al->p != (uint8_t*)header) {
            memcpy((tal_header_t*)al->p + 1, p, header->size);
#if TAL_DEBUG
            header->line_num = -header->line_num;
#endif
        }
    } else {
        /* new alloc case */
        while (al->p + adj_size > al->bufend) {
            al = al->next;
            if (!al) {
                unsigned new_size = (*pal)->size;
                if (adj_size > new_size) {
                    new_size = adj_size;
                    //fprintf(stderr, "%s:%d: alloc big tal: %u\n", file->filename, file->line_num, adj_size - sizeof(tal_header_t));
                }
                al = tal_new(pal, new_size);
                break;
            }
        }
        al->nb_allocs++;
    }
    header = (tal_header_t *)al->p;
    header->size = adj_size - sizeof(tal_header_t);
    al->p += adj_size;
    ret = header + 1;
#if  TAL_DEBUG
    {
        int ofs = strlen(sfile) + 1 - sizeof header->file_name;
        strcpy(header->file_name, sfile + (ofs > 0 ? ofs : 0));
        header->line_num = sline;
#if TAL_INFO
        if (al->nb_peak < al->nb_allocs)
            al->nb_peak = al->nb_allocs;
        if (al->peak_p < al->p)
            al->peak_p = al->p;
        al->nb_total++;
#endif
    }
#endif
    return ret;
}

#endif /* USE_TAL */

/* ------------------------------------------------------------------------- */
/* CString handling */
static void cstr_realloc(CString *cstr, int new_size)
{
    int size;

    size = cstr->size_allocated;
    if (size < 8)
        size = 8; /* no need to allocate a too small first string */
    while (size < new_size)
        size = size * 2;
    cstr->data = tcc_realloc(cstr->data, size);
    cstr->size_allocated = size;
}

/* add a byte */
ST_INLN void cstr_ccat(CString *cstr, int ch)
{
    int size;
    size = cstr->size + 1;
    if (size > cstr->size_allocated)
        cstr_realloc(cstr, size);
    cstr->data[size - 1] = ch;
    cstr->size = size;
}

ST_INLN char *unicode_to_utf8 (char *b, uint32_t Uc)
{
    if (Uc<0x80) *b++=Uc;
    else if (Uc<0x800) *b++=192+Uc/64, *b++=128+Uc%64;
    else if (Uc-0xd800u<0x800) goto error;
    else if (Uc<0x10000) *b++=224+Uc/4096, *b++=128+Uc/64%64, *b++=128+Uc%64;
    else if (Uc<0x110000) *b++=240+Uc/262144, *b++=128+Uc/4096%64, *b++=128+Uc/64%64, *b++=128+Uc%64;
    else error: tcc_error("0x%x is not a valid universal character", Uc);
    return b;
}

/* add a unicode character expanded into utf8 */
ST_INLN void cstr_u8cat(CString *cstr, int ch)
{
    char buf[4], *e;
    e = unicode_to_utf8(buf, (uint32_t)ch);
    cstr_cat(cstr, buf, e - buf);
}

/* add string of 'len', or of its len/len+1 when 'len' == -1/0 */
ST_FUNC void cstr_cat(CString *cstr, const char *str, int len)
{
    int size;
    if (len <= 0)
        len = strlen(str) + 1 + len;
    size = cstr->size + len;
    if (size > cstr->size_allocated)
        cstr_realloc(cstr, size);
    memmove(cstr->data + cstr->size, str, len);
    cstr->size = size;
}

/* add a wide char */
ST_FUNC void cstr_wccat(CString *cstr, int ch)
{
    int size;
    size = cstr->size + sizeof(nwchar_t);
    if (size > cstr->size_allocated)
        cstr_realloc(cstr, size);
    *(nwchar_t *)(cstr->data + size - sizeof(nwchar_t)) = ch;
    cstr->size = size;
}

ST_FUNC void cstr_new(CString *cstr)
{
    memset(cstr, 0, sizeof(CString));
}

/* free string and reset it to NULL */
ST_FUNC void cstr_free(CString *cstr)
{
    tcc_free(cstr->data);
}

/* reset string to empty */
ST_FUNC void cstr_reset(CString *cstr)
{
    cstr->size = 0;
}

ST_FUNC int cstr_vprintf(CString *cstr, const char *fmt, va_list ap)
{
    va_list v;
    int len, size = 80;
    for (;;) {
        size += cstr->size;
        if (size > cstr->size_allocated)
            cstr_realloc(cstr, size);
        size = cstr->size_allocated - cstr->size;
        va_copy(v, ap);
        len = vsnprintf(cstr->data + cstr->size, size, fmt, v);
        va_end(v);
        if (len >= 0 && len < size)
            break;
        size *= 2;
    }
    cstr->size += len;
    return len;
}

ST_FUNC int cstr_printf(CString *cstr, const char *fmt, ...)
{
    va_list ap; int len;
    va_start(ap, fmt);
    len = cstr_vprintf(cstr, fmt, ap);
    va_end(ap);
    return len;
}

/* XXX: unicode ? */
static void add_char(CString *cstr, int c)
{
    if (c == '\'' || c == '\"' || c == '\\') {
        /* XXX: could be more precise if char or string */
        cstr_ccat(cstr, '\\');
    }
    if (c >= 32 && c <= 126) {
        cstr_ccat(cstr, c);
    } else {
        cstr_ccat(cstr, '\\');
        if (c == '\n') {
            cstr_ccat(cstr, 'n');
        } else {
            cstr_ccat(cstr, '0' + ((c >> 6) & 7));
            cstr_ccat(cstr, '0' + ((c >> 3) & 7));
            cstr_ccat(cstr, '0' + (c & 7));
        }
    }
}

/* ------------------------------------------------------------------------- */
/* allocate a new token */
static TokenSym *tok_alloc_new(TokenSym **pts, const char *str, int len)
{
    TokenSym *ts, **ptable;
    int i;

    if (tok_ident >= SYM_FIRST_ANOM) 
        tcc_error("memory full (symbols)");

    /* expand token table if needed */
    i = tok_ident - TOK_IDENT;
    if ((i % TOK_ALLOC_INCR) == 0) {
        ptable = tcc_realloc(table_ident, (i + TOK_ALLOC_INCR) * sizeof(TokenSym *));
        table_ident = ptable;
    }

    ts = tal_realloc(&toksym_alloc, 0, sizeof(TokenSym) + len);
    table_ident[i] = ts;
    ts->tok = tok_ident++;
    ts->sym_identifier = NULL;
    ts->len = len;
    ts->hash_next = NULL;
    memcpy(ts->str, str, len);
    ts->str[len] = '\0';
    *pts = ts;
    return ts;
}

#define TOK_HASH_INIT 1
#define TOK_HASH_FUNC(h, c) ((h) + ((h) << 5) + ((h) >> 27) + (c))


/* find a token and add it if not found */
ST_FUNC TokenSym *tok_alloc(const char *str, int len)
{
    TokenSym *ts, **pts;
    int i;
    unsigned int h;
    
    h = TOK_HASH_INIT;
    for(i=0;i<len;i++)
        h = TOK_HASH_FUNC(h, ((unsigned char *)str)[i]);
    h &= (TOK_HASH_SIZE - 1);

    pts = &hash_ident[h];
    for(;;) {
        ts = *pts;
        if (!ts)
            break;
        if (ts->len == len && !memcmp(ts->str, str, len))
            return ts;
        pts = &(ts->hash_next);
    }
    return tok_alloc_new(pts, str, len);
}

ST_FUNC int tok_alloc_const(const char *str)
{
    return tok_alloc(str, strlen(str))->tok;
}


/* XXX: buffer overflow */
ST_FUNC const char *get_tok_str(int v, CValue *cv)
{
    char *p;
    int i, len;

    cstr_reset(&cstr_buf);
    p = cstr_buf.data;

    switch(v) {
    case TOK_LCHAR:
        cstr_ccat(&cstr_buf, 'L');
    case TOK_CCHAR:
        cstr_ccat(&cstr_buf, '\'');
        add_char(&cstr_buf, cv->i);
        cstr_ccat(&cstr_buf, '\'');
        cstr_ccat(&cstr_buf, '\0');
        break;
    case TOK_PPNUM:
    case TOK_PPSTR:
        return (char*)cv->str.data;
    case TOK_LSTR:
        cstr_ccat(&cstr_buf, 'L');
    case TOK_STR:
        cstr_ccat(&cstr_buf, '\"');
        if (v == TOK_STR) {
            len = cv->str.size - 1;
            for(i=0;i<len;i++)
                add_char(&cstr_buf, ((unsigned char *)cv->str.data)[i]);
        } else {
            len = (cv->str.size / sizeof(nwchar_t)) - 1;
            for(i=0;i<len;i++)
                add_char(&cstr_buf, ((nwchar_t *)cv->str.data)[i]);
        }
        cstr_ccat(&cstr_buf, '\"');
        cstr_ccat(&cstr_buf, '\0');
        break;

    case TOK_LINENUM:
        return strcpy(p, "<linenumber>");

    /* above tokens have value, the ones below don't */
    case TOK_LT:
        v = '<';
        goto addv;
    case TOK_GT:
        v = '>';
        goto addv;
    case TOK_DOTS:
        return strcpy(p, "...");
    case TOK_A_SHL:
        return strcpy(p, "<<=");
    case TOK_A_SAR:
        return strcpy(p, ">>=");
    case TOK_EOF:
        return strcpy(p, "<eof>");
    case 0: /* anonymous nameless symbols */
        return strcpy(p, "<no name>");
    default:
        if (v < TOK_IDENT) {
            /* search in two bytes table */
            const unsigned char *q = tok_two_chars;
            while (*q) {
                if (q[2] == v) {
                    *p++ = q[0];
                    *p++ = q[1];
                    *p = '\0';
                    return cstr_buf.data;
                }
                q += 3;
            }
            if (v >= 127 || (v < 32 && !is_space(v) && v != '\n')) {
                sprintf(p, "<\\x%02x>", v);
                break;
            }
    addv:
            *p++ = v;
            *p = '\0';
        } else if (v < tok_ident) {
            return table_ident[v - TOK_IDENT]->str;
        } else if (v >= SYM_FIRST_ANOM) {
            /* special name for anonymous symbol */
            sprintf(p, "L.%u", v - SYM_FIRST_ANOM);
        } else {
            /* should never happen */
            return NULL;
        }
        break;
    }
    return cstr_buf.data;
}

/* return the current character, handling end of block if necessary
   (but not stray) */
static int handle_eob(void)
{
    BufferedFile *bf = file;
    int len;

    /* only tries to read if really end of buffer */
    if (bf->buf_ptr >= bf->buf_end) {
        if (bf->fd >= 0) {
#if defined(PARSE_DEBUG)
            len = 1;
#else
            len = IO_BUF_SIZE;
#endif
            len = read(bf->fd, bf->buffer, len);
            if (len < 0)
                len = 0;
        } else {
            len = 0;
        }
        total_bytes += len;
        bf->buf_ptr = bf->buffer;
        bf->buf_end = bf->buffer + len;
        *bf->buf_end = CH_EOB;
    }
    if (bf->buf_ptr < bf->buf_end) {
        return bf->buf_ptr[0];
    } else {
        bf->buf_ptr = bf->buf_end;
        return CH_EOF;
    }
}

/* read next char from current input file and handle end of input buffer */
static int next_c(void)
{
    int ch = *++file->buf_ptr;
    /* end of buffer/file handling */
    if (ch == CH_EOB && file->buf_ptr >= file->buf_end)
        ch = handle_eob();
    return ch;
}

/* input with '\[\r]\n' handling. */
static int handle_stray_noerror(int err)
{
    int ch;
    while ((ch = next_c()) == '\\') {
        ch = next_c();
        if (ch == '\n') {
    newl:
            file->line_num++;
        } else {
            if (ch == '\r') {
                ch = next_c();
                if (ch == '\n')
                    goto newl;
                *--file->buf_ptr = '\r';
            }
            if (err)
                tcc_error("stray '\\' in program");
            /* may take advantage of 'BufferedFile.unget[4}' */
            return *--file->buf_ptr = '\\';
        }
    }
    return ch;
}

#define ninp() handle_stray_noerror(0)

/* handle '\\' in strings, comments and skipped regions */
static int handle_bs(uint8_t **p)
{
    int c;
    file->buf_ptr = *p - 1;
    c = ninp();
    *p = file->buf_ptr;
    return c;
}

/* skip the stray and handle the \\n case. Output an error if
   incorrect char after the stray */
static int handle_stray(uint8_t **p)
{
    int c;
    file->buf_ptr = *p - 1;
    c = handle_stray_noerror(1);
    *p = file->buf_ptr;
    return c;
}

/* handle the complicated stray case */
#define PEEKC(c, p)\
{\
    c = *++p;\
    if (c == '\\')\
        c = handle_stray(&p); \
}

/* single line C++ comments */
static uint8_t *parse_line_comment(uint8_t *p)
{
    int c;
    for(;;) {
        for (;;) {
            c = *++p;
    redo:
            if (c == '\n' || c == '\\')
                break;
            c = *++p;
            if (c == '\n' || c == '\\')
                break;
        }
        if (c == '\n')
            break;
        c = handle_bs(&p);
        if (c == CH_EOF)
            break;
        if (c != '\\')
            goto redo;
    }
    return p;
}

/* C comments */
static uint8_t *parse_comment(uint8_t *p)
{
    int c;
    for(;;) {
        /* fast skip loop */
        for(;;) {
            c = *++p;
        redo:
            if (c == '\n' || c == '*' || c == '\\')
                break;
            c = *++p;
            if (c == '\n' || c == '*' || c == '\\')
                break;
        }
        /* now we can handle all the cases */
        if (c == '\n') {
            file->line_num++;
        } else if (c == '*') {
            do {
                c = *++p;
            } while (c == '*');
            if (c == '\\')
                c = handle_bs(&p);
            if (c == '/')
                break;
            goto check_eof;
        } else {
            c = handle_bs(&p);
        check_eof:
            if (c == CH_EOF)
                tcc_error("unexpected end of file in comment");
            if (c != '\\')
                goto redo;
        }
    }
    return p + 1;
}

/* parse a string without interpreting escapes */
static uint8_t *parse_pp_string(uint8_t *p, int sep, CString *str)
{
    int c;
    for(;;) {
        c = *++p;
    redo:
        if (c == sep) {
            break;
        } else if (c == '\\') {
            c = handle_bs(&p);
            if (c == CH_EOF) {
        unterminated_string:
                /* XXX: indicate line number of start of string */
                tok_flags &= ~TOK_FLAG_BOL;
                tcc_error("missing terminating %c character", sep);
            } else if (c == '\\') {
                if (str)
                    cstr_ccat(str, c);
                c = *++p;
                /* add char after '\\' unconditionally */
                if (c == '\\') {
                    c = handle_bs(&p);
                    if (c == CH_EOF)
                        goto unterminated_string;
                }
                goto add_char;
            } else {
                goto redo;
            }
        } else if (c == '\n') {
        add_lf:
            if (ACCEPT_LF_IN_STRINGS) {
                file->line_num++;
                goto add_char;
            } else if (str) { /* not skipping */
                goto unterminated_string;
            } else {
                //tcc_warning("missing terminating %c character", sep);
                return p;
            }
        } else if (c == '\r') {
            c = *++p;
            if (c == '\\')
                c = handle_bs(&p);
            if (c == '\n')
                goto add_lf;
            if (c == CH_EOF)
                goto unterminated_string;
            if (str)
                cstr_ccat(str, '\r');
            goto redo;
        } else {
        add_char:
            if (str)
                cstr_ccat(str, c);
        }
    }
    p++;
    return p;
}

/* token string handling */
ST_INLN void tok_str_new(TokenString *s)
{
    s->str = NULL;
    s->len = 0;
    s->allocated_len = 0;
    s->last_line_num = -1;
}

ST_FUNC TokenString *tok_str_alloc(void)
{
    TokenString *str = tal_realloc(&tokstr_alloc, 0, sizeof *str);
    tok_str_new(str);
    return str;
}

ST_FUNC void tok_str_free_str(int *str)
{
    tal_free(&tokstr_alloc, str);
}

ST_FUNC void tok_str_free(TokenString *str)
{
    tok_str_free_str(str->str);
    tal_free(&tokstr_alloc, str);
}

ST_FUNC int *tok_str_realloc(TokenString *s, int new_size)
{
    int *str, size;

    size = s->allocated_len;
    if (size < 16)
        size = 16;
    while (size < new_size)
        size = size * 2;
    if (size > s->allocated_len) {
        str = tal_realloc(&tokstr_alloc, s->str, size * sizeof(int));
        s->allocated_len = size;
        s->str = str;
    }
    return s->str;
}

ST_FUNC void tok_str_add(TokenString *s, int t)
{
    int len, *str;

    len = s->len;
    str = s->str;
    if (len >= s->allocated_len)
        str = tok_str_realloc(s, len + 1);
    str[len++] = t;
    s->len = len;
}

ST_FUNC void begin_token_stream(TokenString *str, int alloc)
{
    str->alloc = alloc;
    str->prev = token_stream_stack;
    str->prev_ptr = token_stream_ptr;
    str->save_line_num = file->line_num;
    token_stream_ptr = str->str;
    token_stream_stack = str;
}

ST_FUNC void end_token_stream(void)
{
    TokenString *str = token_stream_stack;
    token_stream_stack = str->prev;
    token_stream_ptr = str->prev_ptr;
    file->line_num = str->save_line_num;
    if (str->alloc == 0) {
        str->len = 0;
    } else {
        if (str->alloc == 2)
            str->str = NULL; /* don't free */
        tok_str_free(str);
    }
}

static void tok_str_add2(TokenString *s, int t, CValue *cv)
{
    int len, *str;

    len = s->len;
    str = s->str;

    /* allocate space for worst case */
    if (len + TOK_MAX_SIZE >= s->allocated_len)
        str = tok_str_realloc(s, len + TOK_MAX_SIZE + 1);
    str[len++] = t;
    switch(t) {
    case TOK_CCHAR:
    case TOK_LCHAR:
    case TOK_LINENUM:
        str[len++] = cv->i;
        break;
    case TOK_PPNUM:
    case TOK_PPSTR:
    case TOK_STR:
    case TOK_LSTR:
        {
            /* Insert the string into the int array. */
            size_t nb_words =
                1 + (cv->str.size + sizeof(int) - 1) / sizeof(int);
            if (len + nb_words >= s->allocated_len)
                str = tok_str_realloc(s, len + nb_words + 1);
            str[len] = cv->str.size;
            memcpy(&str[len + 1], cv->str.data, cv->str.size);
            len += nb_words;
        }
        break;
    default:
        break;
    }
    s->len = len;
}

/* add the current parse token in token string 's' */
ST_FUNC void tok_str_add_tok(TokenString *s)
{
    CValue cval;

    /* save line number info */
    if (file->line_num != s->last_line_num) {
        s->last_line_num = file->line_num;
        cval.i = s->last_line_num;
        tok_str_add2(s, TOK_LINENUM, &cval);
    }
    tok_str_add2(s, tok, &tokc);
}

/* get a token from an integer array and increment pointer. */
static inline void tok_get(int *t, const int **pp, CValue *cv)
{
    const int *p = *pp;

    switch(*t = *p++) {
    case TOK_CCHAR:
    case TOK_LCHAR:
    case TOK_LINENUM:
        cv->i = *p++;
        break;
    case TOK_STR:
    case TOK_LSTR:
    case TOK_PPNUM:
    case TOK_PPSTR:
        cv->str.size = *p++;
        cv->str.data = (char*)p;
        p += (cv->str.size + sizeof(int) - 1) / sizeof(int);
        break;
    default:
        break;
    }
    *pp = p;
}


/* evaluate escape codes in a string. */
static void parse_escape_string(CString *outstr, const uint8_t *buf, int is_long)
{
    int c, n, i;
    const uint8_t *p;

    p = buf;
    for(;;) {
        c = *p;
        if (c == '\0')
            break;
        if (c == '\\') {
            p++;
            /* escape */
            c = *p;
            switch(c) {
            case '0': case '1': case '2': case '3':
            case '4': case '5': case '6': case '7':
                /* at most three octal digits */
                n = c - '0';
                p++;
                c = *p;
                if (isoct(c)) {
                    n = n * 8 + c - '0';
                    p++;
                    c = *p;
                    if (isoct(c)) {
                        n = n * 8 + c - '0';
                        p++;
                    }
                }
                c = n;
                goto add_char_nonext;
            case 'x': i = 0; goto parse_hex_or_ucn;
            case 'u': i = 4; goto parse_hex_or_ucn;
            case 'U': i = 8; goto parse_hex_or_ucn;
    parse_hex_or_ucn:
                p++;
                n = 0;
                do {
                    c = *p;
                    if (c >= 'a' && c <= 'f')
                        c = c - 'a' + 10;
                    else if (c >= 'A' && c <= 'F')
                        c = c - 'A' + 10;
                    else if (isnum(c))
                        c = c - '0';
                    else if (i >= 0)
                        expect("more hex digits in universal-character-name");
                    else
                        goto add_hex_or_ucn;
                    n = (unsigned) n * 16 + c;
                    p++;
                } while (--i);
		if (is_long) {
    add_hex_or_ucn:
                    c = n;
		    goto add_char_nonext;
		}
                cstr_u8cat(outstr, n);
                continue;
            case 'a':
                c = '\a';
                break;
            case 'b':
                c = '\b';
                break;
            case 'f':
                c = '\f';
                break;
            case 'n':
                c = '\n';
                break;
            case 'r':
                c = '\r';
                break;
            case 't':
                c = '\t';
                break;
            case 'v':
                c = '\v';
                break;
            case '\'':
            case '\"':
            case '\\': 
            case '?':
                break;
            default:
                if (c >= '!' && c <= '~')
                    tcc_warning("unknown escape sequence: \'\\%c\'", c);
                else
                    tcc_warning("unknown escape sequence: \'\\x%x\'", c);
                break;
            }
        } else if (is_long && c >= 0x80) {
            /* assume we are processing UTF-8 sequence */
            /* reference: The Unicode Standard, Version 10.0, ch3.9 */

            int cont; /* count of continuation bytes */
            int skip; /* how many bytes should skip when error occurred */
            int i;

            /* decode leading byte */
            if (c < 0xC2) {
	            skip = 1; goto invalid_utf8_sequence;
            } else if (c <= 0xDF) {
	            cont = 1; n = c & 0x1f;
            } else if (c <= 0xEF) {
	            cont = 2; n = c & 0xf;
            } else if (c <= 0xF4) {
	            cont = 3; n = c & 0x7;
            } else {
	            skip = 1; goto invalid_utf8_sequence;
            }

            /* decode continuation bytes */
            for (i = 1; i <= cont; i++) {
                int l = 0x80, h = 0xBF;

                /* adjust limit for second byte */
                if (i == 1) {
                    switch (c) {
                    case 0xE0: l = 0xA0; break;
                    case 0xED: h = 0x9F; break;
                    case 0xF0: l = 0x90; break;
                    case 0xF4: h = 0x8F; break;
                    }
                }

                if (p[i] < l || p[i] > h) {
                    skip = i; goto invalid_utf8_sequence;
                }

                n = (n << 6) | (p[i] & 0x3f);
            }

            /* advance pointer */
            p += 1 + cont;
            c = n;
            goto add_char_nonext;

            /* error handling */
        invalid_utf8_sequence:
            tcc_warning("ill-formed UTF-8 subsequence starting with: \'\\x%x\'", c);
            c = 0xFFFD;
            p += skip;
            goto add_char_nonext;

        }
        p++;
    add_char_nonext:
        if (!is_long)
            cstr_ccat(outstr, c);
        else {
            cstr_wccat(outstr, c);
        }
    }
    /* add a trailing '\0' */
    if (!is_long)
        cstr_ccat(outstr, '\0');
    else
        cstr_wccat(outstr, '\0');
}

static void parse_string(const char *s, int len)
{
    uint8_t buf[1000], *p = buf;
    int is_long, sep;

    if ((is_long = *s == 'L'))
        ++s, --len;
    sep = *s++;
    len -= 2;
    if (len >= sizeof buf)
        p = tcc_malloc(len + 1);
    memcpy(p, s, len);
    p[len] = 0;

    cstr_reset(&tokcstr);
    parse_escape_string(&tokcstr, p, is_long);
    if (p != buf)
        tcc_free(p);

    if (sep == '\'') {
        int char_size, i, n, c;
        /* XXX: make it portable */
        if (!is_long)
            tok = TOK_CCHAR, char_size = 1;
        else
            tok = TOK_LCHAR, char_size = sizeof(nwchar_t);
        n = tokcstr.size / char_size - 1;
        if (n < 1)
            tcc_error("empty character constant");
        if (n > 1)
            tcc_warning("multi-character character constant");
        for (c = i = 0; i < n; ++i) {
            if (is_long)
                c = ((nwchar_t *)tokcstr.data)[i];
            else
                c = (c << 8) | ((char *)tokcstr.data)[i];
        }
        tokc.i = c;
    } else {
        tokc.str.size = tokcstr.size;
        tokc.str.data = tokcstr.data;
        if (!is_long)
            tok = TOK_STR;
        else
            tok = TOK_LSTR;
    }
}

#define PARSE2(c1, tok1, c2, tok2)              \
    case c1:                                    \
        PEEKC(c, p);                            \
        if (c == c2) {                          \
            p++;                                \
            tok = tok2;                         \
        } else {                                \
            tok = tok1;                         \
        }                                       \
        break;

/* return next token without macro substitution */
static void next_nomacro(void)
{
    int t, c, is_long, len;
    TokenSym *ts;
    uint8_t *p, *p1;
    unsigned int h;

    p = file->buf_ptr;
 redo_no_start:
    c = *p;
    switch(c) {
    case ' ':
    case '\t':
        tok = c;
        p++;
 maybe_space:
        while (isidnum_table[*p - CH_EOF] & IS_SPC)
            ++p;
        goto redo_no_start;
    case '\f':
    case '\v':
    case '\r':
        p++;
        goto redo_no_start;
    case '\\':
        /* first look if it is in fact an end of buffer */
        c = handle_stray(&p);
        if (c == '\\')
            goto parse_simple;
        if (c == CH_EOF) {
            if (!(tok_flags & TOK_FLAG_BOL)) {
                /* add implicit newline */
                goto maybe_newline;
            } else {
                tok = TOK_EOF;
            }
        } else {
            goto redo_no_start;
        }
        break;

    case '\n':
        file->line_num++;
        p++;
maybe_newline:
        tok_flags |= TOK_FLAG_BOL;
        if (0 == (parse_flags & PARSE_FLAG_LINEFEED))
            goto redo_no_start;
        tok = TOK_LINEFEED;
        goto keep_tok_flags;

    case '#':
        PEEKC(c, p);
        if (c == '#') {
            p++;
            tok = TOK_TWOSHARPS;
        } else {
#ifndef TCC_TARGET_ARM64
            if (parse_flags & PARSE_FLAG_ASM_FILE) {
                p = parse_line_comment(p - 1);
                goto redo_no_start;
            }
#endif
            tok = '#';
        }
        break;
    
    /* dollar is allowed to start identifiers when not parsing asm */
    case '$':
        if (!(isidnum_table['$' - CH_EOF] & IS_ID)
         || (parse_flags & PARSE_FLAG_ASM_FILE))
            goto parse_simple;

    case 'a': case 'b': case 'c': case 'd':
    case 'e': case 'f': case 'g': case 'h':
    case 'i': case 'j': case 'k': case 'l':
    case 'm': case 'n': case 'o': case 'p':
    case 'q': case 'r': case 's': case 't':
    case 'u': case 'v': case 'w': case 'x':
    case 'y': case 'z': 
    case 'A': case 'B': case 'C': case 'D':
    case 'E': case 'F': case 'G': case 'H':
    case 'I': case 'J': case 'K': 
    case 'M': case 'N': case 'O': case 'P':
    case 'Q': case 'R': case 'S': case 'T':
    case 'U': case 'V': case 'W': case 'X':
    case 'Y': case 'Z': 
    case '_':
    parse_ident_fast:
        p1 = p;
        h = TOK_HASH_INIT;
        h = TOK_HASH_FUNC(h, c);
        while (c = *++p, isidnum_table[c - CH_EOF] & (IS_ID|IS_NUM))
            h = TOK_HASH_FUNC(h, c);
        len = p - p1;
        if (c != '\\') {
            TokenSym **pts;

            /* fast case : no stray found, so we have the full token
               and we have already hashed it */
            h &= (TOK_HASH_SIZE - 1);
            pts = &hash_ident[h];
            for(;;) {
                ts = *pts;
                if (!ts)
                    break;
                if (ts->len == len && !memcmp(ts->str, p1, len))
                    goto token_found;
                pts = &(ts->hash_next);
            }
            ts = tok_alloc_new(pts, (char *) p1, len);
        token_found: ;
        } else {
            /* slower case */
            cstr_reset(&tokcstr);
            cstr_cat(&tokcstr, (char *) p1, len);
            p--;
            PEEKC(c, p);
            while (isidnum_table[c - CH_EOF] & (IS_ID|IS_NUM))
            {
                cstr_ccat(&tokcstr, c);
                PEEKC(c, p);
            }
            ts = tok_alloc(tokcstr.data, tokcstr.size);
        }
        tok = ts->tok;
        break;
    case 'L':
        t = p[1];
        if (t == '\'' || t == '\"' || t == '\\') {
            PEEKC(c, p);
            if (c == '\'' || c == '\"') {
                is_long = 1;
                goto str_const;
            }
            *--p = c = 'L';
        }
        goto parse_ident_fast;

    case '0': case '1': case '2': case '3':
    case '4': case '5': case '6': case '7':
    case '8': case '9':
        t = c;
        PEEKC(c, p);
        /* after the first digit, accept digits, alpha, '.' or sign if
           prefixed by 'eEpP' */
    parse_num:
        cstr_reset(&tokcstr);
        for(;;) {
            cstr_ccat(&tokcstr, t);
            if (!((isidnum_table[c - CH_EOF] & (IS_ID|IS_NUM))
                  || c == '.'
                  || ((c == '+' || c == '-')
                      && (((t == 'e' || t == 'E')
                            && !(parse_flags & PARSE_FLAG_ASM_FILE
                                /* 0xe+1 is 3 tokens in asm */
                                && ((char*)tokcstr.data)[0] == '0'
                                && toup(((char*)tokcstr.data)[1]) == 'X'))
                          || t == 'p' || t == 'P'))))
                break;
            t = c;
            PEEKC(c, p);
        }
        /* We add a trailing '\0' to ease parsing */
        cstr_ccat(&tokcstr, '\0');
        tokc.str.size = tokcstr.size;
        tokc.str.data = tokcstr.data;
        tok = TOK_PPNUM;
        break;

    case '.':
        /* special dot handling because it can also start a number */
        PEEKC(c, p);
        if (isnum(c)) {
            t = '.';
            goto parse_num;
        } else if ((isidnum_table['.' - CH_EOF] & IS_ID)
                   && (isidnum_table[c - CH_EOF] & (IS_ID|IS_NUM))) {
            *--p = c = '.';
            goto parse_ident_fast;
        } else if (c == '.') {
            PEEKC(c, p);
            if (c == '.') {
                p++;
                tok = TOK_DOTS;
            } else {
                *--p = '.'; /* may underflow into file->unget[] */
                tok = '.';
            }
        } else {
            tok = '.';
        }
        break;
    case '\'':
    case '\"':
        is_long = 0;
    str_const:
        cstr_reset(&tokcstr);
        if (is_long)
            cstr_ccat(&tokcstr, 'L');
        cstr_ccat(&tokcstr, c);
        p = parse_pp_string(p, c, &tokcstr);
        cstr_ccat(&tokcstr, c);
        cstr_ccat(&tokcstr, '\0');
        tokc.str.size = tokcstr.size;
        tokc.str.data = tokcstr.data;
        tok = TOK_PPSTR;
        break;

    case '<':
        PEEKC(c, p);
        if (c == '=') {
            p++;
            tok = TOK_LE;
        } else if (c == '<') {
            PEEKC(c, p);
            if (c == '=') {
                p++;
                tok = TOK_A_SHL;
            } else {
                tok = TOK_SHL;
            }
        } else {
            tok = TOK_LT;
        }
        break;
    case '>':
        PEEKC(c, p);
        if (c == '=') {
            p++;
            tok = TOK_GE;
        } else if (c == '>') {
            PEEKC(c, p);
            if (c == '=') {
                p++;
                tok = TOK_A_SAR;
            } else {
                tok = TOK_SAR;
            }
        } else {
            tok = TOK_GT;
        }
        break;
        
    case '&':
        PEEKC(c, p);
        if (c == '&') {
            p++;
            tok = TOK_LAND;
        } else if (c == '=') {
            p++;
            tok = TOK_A_AND;
        } else {
            tok = '&';
        }
        break;
        
    case '|':
        PEEKC(c, p);
        if (c == '|') {
            p++;
            tok = TOK_LOR;
        } else if (c == '=') {
            p++;
            tok = TOK_A_OR;
        } else {
            tok = '|';
        }
        break;

    case '+':
        PEEKC(c, p);
        if (c == '+') {
            p++;
            tok = TOK_INC;
        } else if (c == '=') {
            p++;
            tok = TOK_A_ADD;
        } else {
            tok = '+';
        }
        break;
        
    case '-':
        PEEKC(c, p);
        if (c == '-') {
            p++;
            tok = TOK_DEC;
        } else if (c == '=') {
            p++;
            tok = TOK_A_SUB;
        } else if (c == '>') {
            p++;
            tok = TOK_ARROW;
        } else {
            tok = '-';
        }
        break;

    PARSE2('!', '!', '=', TOK_NE)
    PARSE2('=', '=', '=', TOK_EQ)
    PARSE2('*', '*', '=', TOK_A_MUL)
    PARSE2('%', '%', '=', TOK_A_MOD)
    PARSE2('^', '^', '=', TOK_A_XOR)
        
        /* comments or operator */
    case '/':
        PEEKC(c, p);
        if (c == '*') {
            p = parse_comment(p);
            /* comments replaced by a blank */
            tok = ' ';
            goto maybe_space;
        } else if (c == '/') {
            p = parse_line_comment(p);
            tok = ' ';
            goto maybe_space;
        } else if (c == '=') {
            p++;
            tok = TOK_A_DIV;
        } else {
            tok = '/';
        }
        break;
        
        /* simple tokens */
    case '@': /* only used in assembler */
    case '(':
    case ')':
    case '[':
    case ']':
    case '{':
    case '}':
    case ',':
    case ';':
    case ':':
    case '?':
    case '~':
    parse_simple:
        tok = c;
        p++;
        break;
    case 0xEF: /* UTF8 BOM ? */
        if (p[1] == 0xBB && p[2] == 0xBF && p == file->buffer) {
            p += 3;
            goto redo_no_start;
        }
    default:
        if (c >= 0x80 && c <= 0xFF) /* utf8 identifiers */
	    goto parse_ident_fast;
        if (parse_flags & PARSE_FLAG_ASM_FILE)
            goto parse_simple;
        tcc_error("unrecognized character \\x%02x", c);
        break;
    }
    tok_flags = 0;
keep_tok_flags:
    file->buf_ptr = p;
#if defined(PARSE_DEBUG)
    printf("token = %d %s\n", tok, get_tok_str(tok, &tokc));
#endif
}

/* return next token from replay stack or input file */
ST_FUNC void next(void)
{
    int t;

    while (token_stream_ptr) {
redo:
        t = *token_stream_ptr;
        if (TOK_HAS_VALUE(t)) {
            tok_get(&tok, &token_stream_ptr, &tokc);
            if (t == TOK_LINENUM) {
                file->line_num = tokc.i;
                goto redo;
            }
            goto convert;
        } else if (t == 0) {
            end_token_stream();
            continue;
        } else {
            ++token_stream_ptr;
        }
        tok = t;
        return;
    }

    next_nomacro();
    t = tok;

convert:
    if (t == TOK_PPSTR) {
        if (parse_flags & PARSE_FLAG_TOK_STR)
            parse_string(tokc.str.data, tokc.str.size - 1);
    }
}

/* push back current token and set current token to last_tok. */
ST_INLN void unget_tok(int last_tok)
{
    TokenString *str = &unget_buf;
    int alloc = 0;
    if (str->len)
        str = tok_str_alloc(), alloc = 1;
    if (tok != TOK_EOF)
        tok_str_add2(str, tok, &tokc);
    tok_str_add(str, 0);
    begin_token_stream(str, alloc);
    tok = last_tok;
}


/* ------------------------------------------------------------------------- */
/* init lexer */

ST_FUNC void tcc_lexer_start(TCCState *s1)
{
    tccpp_new(s1);
    set_idnum('$', 0);
    set_idnum('.', IS_ID);
    parse_flags = PARSE_FLAG_ASM_FILE;
}

ST_FUNC void tcc_lexer_end(TCCState *s1)
{
    while (token_stream_stack)
        end_token_stream();
    token_stream_ptr = NULL;
    while (file)
        tcc_close();
    tccpp_delete(s1);
}


ST_FUNC int set_idnum(int c, int val)
{
    int prev = isidnum_table[c - CH_EOF];
    isidnum_table[c - CH_EOF] = val;
    return prev;
}

ST_FUNC void tccpp_new(TCCState *s)
{
    int i, c;
    const char *p, *r;

    (void)s;

    /* init isid table */
    for(i = CH_EOF; i<128; i++)
        set_idnum(i,
            is_space(i) ? IS_SPC
            : isid(i) ? IS_ID
            : isnum(i) ? IS_NUM
            : 0);

    for(i = 128; i<256; i++)
        set_idnum(i, IS_ID);

    /* init allocators */
    tal_new(&toksym_alloc, TOKSYM_TAL_SIZE);
    tal_new(&tokstr_alloc, TOKSTR_TAL_SIZE);

    memset(hash_ident, 0, TOK_HASH_SIZE * sizeof(TokenSym *));
    cstr_new(&tokcstr);
    cstr_new(&cstr_buf);
    cstr_realloc(&cstr_buf, STRING_MAX_SIZE);
    tok_str_new(&unget_buf);
    tok_str_realloc(&unget_buf, TOKSTR_MAX_SIZE);

    tok_ident = TOK_IDENT;
    p = tcc_keywords;
    while (*p) {
        r = p;
        for(;;) {
            c = *r++;
            if (c == '\0')
                break;
        }
        tok_alloc(p, r - p - 1);
        p = r;
    }
}

ST_FUNC void tccpp_delete(TCCState *s)
{
    int i, n;

    (void)s;

    /* free tokens */
    n = tok_ident - TOK_IDENT;
    if (n > total_idents)
        total_idents = n;
    for (i = n; --i >= 0;)
        tal_free(&toksym_alloc, table_ident[i]);
    tcc_free(table_ident);
    table_ident = NULL;

    /* free static buffers */
    cstr_free(&tokcstr);
    cstr_free(&cstr_buf);
    tok_str_free_str(unget_buf.str);

    /* free allocators */
    tal_delete(&toksym_alloc);
    tal_delete(&tokstr_alloc);
}
