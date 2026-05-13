/*
 *  tinybinutils shared assembler/linker internals
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

#ifndef _TCC_H
#define _TCC_H

#define _GNU_SOURCE
#define _DARWIN_C_SOURCE

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <setjmp.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>

#ifndef O_BINARY
# define O_BINARY 0
#endif

#ifndef offsetof
#ifdef __clang__ // clang -fsanitize compains about: NULL+value
#define offsetof(type, field) __builtin_offsetof(type, field)
#else
#define offsetof(type, field) ((size_t) &((type *)0)->field)
#endif
#endif

#ifndef countof
#define countof(tab) (sizeof(tab) / sizeof((tab)[0]))
#endif

#define NORETURN __attribute__((noreturn))
#define ALIGNED(x) __attribute__((aligned(x)))
#define PRINTF_LIKE(x,y) __attribute__ ((format (printf, (x), (y))))

#define IS_DIRSEP(c) (c == '/')
#define IS_ABSPATH(p) IS_DIRSEP(p[0])
#define PATHCMP strcmp
#define PATHSEP ":"

/* -------------------------------------------- */

/* parser debug */
/* #define PARSE_DEBUG */
/* memory leak debug (only for single threaded usage) */
/* #define MEM_DEBUG 1,2,3 */
/* assembler debug */
/* #define ASM_DEBUG */

/* target selection */
#if (defined(TINY_TARGET_X86_64) + defined(TINY_TARGET_ARM64) + defined(TINY_TARGET_RISCV64)) != 1
# error tinyld requires exactly one target: TINY_TARGET_X86_64, TINY_TARGET_ARM64, or TINY_TARGET_RISCV64
#endif

#include "tinyelf.h"

#ifndef PUB_FUNC
# define PUB_FUNC
#endif

typedef struct TCCState TCCState;

#define TINY_OUTPUT_EXE        2
#define TINY_OUTPUT_OBJ        3

/* -------------------------------------------- */

#define ST_INLN
#define ST_FUNC
#define ST_DATA extern

/* -------------------------------------------- */
/* ELF64 target definitions */

#define PTR_SIZE 8

#define TARGET_DEFS_ONLY
#if defined TINY_TARGET_X86_64
# define TINYLD_TARGET_NAME "x86_64"
# include "x86_64-link.c"
#elif defined TINY_TARGET_ARM64
# define TINYLD_TARGET_NAME "aarch64"
# include "arm64-link.c"
#elif defined TINY_TARGET_RISCV64
# define TINYLD_TARGET_NAME "riscv64"
# include "riscv64-link.c"
#endif
#undef TARGET_DEFS_ONLY

/* -------------------------------------------- */

#define ELFCLASSW ELFCLASS64
#define ElfW(type) Elf##64##_##type
#define ELFW(type) ELF##64##_##type
#define ElfW_Rel ElfW(Rela)
#define SHT_RELX SHT_RELA
#define REL_SECTION_FMT ".rela%s"
/* target address type */
#define addr_t ElfW(Addr)
#define ElfSym ElfW(Sym)

#if defined TINY_TARGET_X86_64
# define NB_ASM_REGS 16
enum {
    TREG_RAX = 0,
    TREG_RCX = 1,
    TREG_RDX = 2,
    TREG_RSP = 4,
    TREG_RSI = 6,
    TREG_RDI = 7,
    TREG_R8  = 8,
    TREG_R9  = 9,
    TREG_R10 = 10,
    TREG_R11 = 11,
    TREG_XMM0 = 16,
    TREG_XMM1 = 17,
    TREG_XMM2 = 18,
    TREG_XMM3 = 19,
    TREG_XMM4 = 20,
    TREG_XMM5 = 21,
    TREG_XMM6 = 22,
    TREG_XMM7 = 23,
    TREG_ST0 = 24,
    TREG_MEM = 0x20
};
#elif defined TINY_TARGET_ARM64 || defined TINY_TARGET_RISCV64
# define NB_ASM_REGS 64
#endif

/* -------------------------------------------- */

#define VSTACK_SIZE         512
#define STRING_MAX_SIZE     1024
#define TOKSTR_MAX_SIZE     256

#define TOK_HASH_SIZE       16384 /* must be a power of two */
#define TOK_ALLOC_INCR      512  /* must be a power of two */
#define TOK_MAX_SIZE        4 /* token max size in int unit when stored in string */

/* token symbol management */
typedef struct TokenSym {
    struct TokenSym *hash_next;
    struct Sym *sym_identifier; /* direct pointer to identifier */
    int tok; /* token number */
    int len;
    char str[1];
} TokenSym;

typedef int nwchar_t;

typedef struct CString {
    int size; /* size in bytes */
    int size_allocated;
    char *data; /* nwchar_t* in cases */
} CString;

/* type definition */
typedef struct CType {
    int t;
    struct Sym *ref;
} CType;

/* constant value */
typedef union CValue {
    uint64_t i;
    struct {
        char *data;
        int size;
    } str;
} CValue;

/* symbol management */
typedef struct Sym {
    int v; /* symbol token */
    unsigned char weak;
    unsigned char visibility;
    int c; /* Elf symbol index */

    CType type; /* associated type */
    struct Sym *next; /* free-list link */
    struct Sym *prev_tok; /* previous symbol for this token */
} Sym;

/* section definition */
typedef struct Section {
    unsigned long data_offset; /* current data offset */
    unsigned char *data;       /* section data */
    unsigned long data_allocated; /* used for realloc() handling */
    TCCState *s1;
    int sh_name;             /* elf section name (only used during output) */
    int sh_num;              /* elf section number */
    int sh_type;             /* elf section type */
    int sh_flags;            /* elf section flags */
    int sh_info;             /* elf section info */
    int sh_addralign;        /* elf section alignment */
    int sh_entsize;          /* elf entry size */
    unsigned long sh_size;   /* section size (only used during output) */
    addr_t sh_addr;          /* address at which the section is relocated */
    unsigned long sh_offset; /* file offset */
    int nb_hashed_syms;      /* used to resize the hash table */
    struct Section *link;    /* link to another section */
    struct Section *reloc;   /* corresponding section for relocation, if any */
    struct Section *hash;    /* hash table for symbols */
    struct Section *prev;    /* previous section on section stack */
    char name[1];           /* section name */
} Section;

/* -------------------------------------------------- */

#define SYM_FIRST_ANOM 0x10000000 /* first anonymous sym */

#define IO_BUF_SIZE 8192

typedef struct BufferedFile {
    uint8_t *buf_ptr;
    uint8_t *buf_end;
    int fd;
    struct BufferedFile *prev;
    int line_num;    /* current line number - here to simplify code */
    char filename[1024];    /* filename */
    unsigned char unget[4];
    unsigned char buffer[1]; /* extra size for CH_EOB char */
} BufferedFile;

#define CH_EOB   '\\'       /* end of buffer or '\0' char in file */
#define CH_EOF   (-1)   /* end of file */

/* used to record tokens */
typedef struct TokenString {
    int *str;
    int len;
    int allocated_len;
    int last_line_num;
    int save_line_num;
    /* used to chain token streams */
    struct TokenString *prev;
    const int *prev_ptr;
    char alloc;
} TokenString;

typedef struct ExprValue {
    uint64_t v;
    Sym *sym;
    int pcrel;
} ExprValue;

/* extra symbol attributes (not in symbol table) */
struct sym_attr {
    unsigned got_offset;
    unsigned plt_offset;
    int plt_sym;
    int dyn_index;
};

struct TCCState {
    unsigned char whole_archive;

    unsigned char has_text_addr;
    addr_t text_addr; /* address of text section */
    unsigned section_align; /* section alignment */

    char *elf_entryname; /* "_start" unless set */

    /* output type, see TCC_OUTPUT_XXX */
    int output_type;

    /* library paths */
    char **library_paths;
    int nb_library_paths;

    /* error handling */
    const char *tool_name;
    int error_set_jmp_enabled;
    jmp_buf error_jmp_buf;
    int nb_errors;

    /* sections */
    Section **sections;
    int nb_sections; /* number of sections, including first dummy section */

    Section **priv_sections;
    int nb_priv_sections; /* number of private sections */

    /* predefined sections */
    Section *text_section, *data_section, *rodata_section, *bss_section;
    Section *common_section;
    Section *cur_text_section; /* current section where function code is generated */
    /* symbol section */
    union { Section *symtab_section, *symtab; }; /* historical alias */
    /* got & plt handling */
    Section *got, *plt;

    /* extra attributes (eg. GOT/PLT value) for symtab symbols */
    struct sym_attr *sym_attrs;
    int nb_sym_attrs;
    /* ptr to next reloc entry reused */
    ElfW_Rel *qrel;
    #define qrel s1->qrel

#ifdef TINY_TARGET_RISCV64
    struct pcrel_hi { addr_t addr, val; } **pcrel_hi_entries;
    int nb_pcrel_hi_entries;
#endif

    /* benchmark info */
    int total_idents;
    int total_lines;
    unsigned int total_bytes;

    /* for warnings/errors for object files */
    const char *current_filename;

};

#define VT_SYM       0x0200  /* a symbol value is added */
#define VT_BTYPE       0x000f
#define VT_VOID             0
#define VT_FUNC             6
#define VT_EXTERN  0x00001000  /* extern definition */
#define VT_STATIC  0x00002000  /* static variable */

#define VT_STRUCT_SHIFT 20
#define VT_STRUCT_MASK (((1U << 12) - 1) << VT_STRUCT_SHIFT | 0x0080)
#define VT_ASM (VT_VOID | 4 << VT_STRUCT_SHIFT)
#define VT_ASM_FUNC (VT_VOID | 5 << VT_STRUCT_SHIFT)
#define IS_ASM_SYM(sym) (((sym)->type.t & ((VT_BTYPE|VT_STRUCT_MASK) & ~(1<<VT_STRUCT_SHIFT))) == VT_ASM)
#define IS_ASM_FUNC(t) ((t & (VT_BTYPE|VT_STRUCT_MASK)) == VT_ASM_FUNC)

/* token values */

/* conditional ops */
/* warning: the following compare tokens depend on i386 asm code */
#define TOK_EQ  0x94
#define TOK_NE  0x95
#define TOK_LT  0x9c
#define TOK_GE  0x9d
#define TOK_LE  0x9e
#define TOK_GT  0x9f

#define TOK_SHL     '<' /* shift left */
#define TOK_SAR     '>' /* signed shift right */

/* tokens that carry values (in additional token string space / tokc) --> */
#define TOK_CCHAR   0xc0 /* char constant in tokc */
#define TOK_STR     0xc2 /* pointer to string in tokc */
#define TOK_PPNUM   0xc4 /* raw number token */
#define TOK_PPSTR   0xc5 /* raw string token */
#define TOK_LINENUM 0xc6 /* line number info */

#define TOK_HAS_VALUE(t) (t >= TOK_CCHAR && t <= TOK_LINENUM)

#define TOK_EOF       (-1)  /* end of file */
#define TOK_LINEFEED  10    /* line feed */

/* all identifiers and strings have token above that */
#define TOK_IDENT 256

enum tcc_token {
    TOK_LAST = TOK_IDENT - 1
#define DEF(id, str) ,id
#include "tcctok.h"
#undef DEF
};

/* ------------ tinyld_support.c ------------ */

ST_DATA struct TCCState *tcc_state;

/* string and path helpers */
ST_FUNC char *pstrcpy(char *buf, size_t buf_size, const char *s);
ST_FUNC char *pstrcat(char *buf, size_t buf_size, const char *s);
PUB_FUNC char *tiny_basename(const char *name);
PUB_FUNC char *tiny_fileextension(const char *name);

/* all allocations - even MEM_DEBUG - use these */
PUB_FUNC void tcc_free(void *ptr);
PUB_FUNC void *tcc_malloc(unsigned long size);
PUB_FUNC void *tcc_mallocz(unsigned long size);
PUB_FUNC void *tcc_realloc(void *ptr, unsigned long size);
PUB_FUNC char *tcc_strdup(const char *str);

#ifdef MEM_DEBUG
#define tcc_free(ptr)           tcc_free_debug(ptr)
#define tcc_malloc(size)        tcc_malloc_debug(size, __FILE__, __LINE__)
#define tcc_mallocz(size)       tcc_mallocz_debug(size, __FILE__, __LINE__)
#define tcc_realloc(ptr,size)   tcc_realloc_debug(ptr, size, __FILE__, __LINE__)
#define tcc_strdup(str)         tcc_strdup_debug(str, __FILE__, __LINE__)
PUB_FUNC void tcc_free_debug(void *ptr);
PUB_FUNC void *tcc_malloc_debug(unsigned long size, const char *file, int line);
PUB_FUNC void *tcc_mallocz_debug(unsigned long size, const char *file, int line);
PUB_FUNC void *tcc_realloc_debug(void *ptr, unsigned long size, const char *file, int line);
PUB_FUNC char *tcc_strdup_debug(const char *str, const char *file, int line);
#endif

/* defined to be not used */
#define free(p) use_tcc_free(p)
#define malloc(s) use_tcc_malloc(s)
#define realloc(p, s) use_tcc_realloc(p, s)
#undef strdup
#define strdup(s) use_tcc_strdup(s)
PUB_FUNC int _tcc_error_noabort(const char *fmt, ...) PRINTF_LIKE(1,2);
PUB_FUNC NORETURN void _tcc_error(const char *fmt, ...) PRINTF_LIKE(1,2);
PUB_FUNC void _tcc_warning(const char *fmt, ...) PRINTF_LIKE(1,2);

/* other utilities */
ST_FUNC void dynarray_add(void *ptab, int *nb_ptr, void *data);
ST_FUNC void dynarray_reset(void *pp, int *n);
ST_INLN void cstr_ccat(CString *cstr, int ch);
ST_FUNC void cstr_cat(CString *cstr, const char *str, int len);
ST_FUNC void cstr_wccat(CString *cstr, int ch);
ST_FUNC void cstr_new(CString *cstr);
ST_FUNC void cstr_free(CString *cstr);
ST_FUNC void cstr_reset(CString *cstr);
ST_FUNC void tinyas_open_bf(TCCState *s1, const char *filename, int initlen);
ST_FUNC int tinyas_open(TCCState *s1, const char *filename);
ST_FUNC void tinyas_close(void);

ST_FUNC int tinyld_add_file_internal(TCCState *s1, const char *filename, int flags);
/* flags: */
#define AFF_PRINT_ERROR     0x10 /* print error if file not found */
#define AFF_WHOLE_ARCHIVE   0x80 /* load all objects from archive */
/* values from tinyld_object_type(...) */
#define AFF_BINTYPE_REL 1
#define AFF_BINTYPE_DYN 2
#define AFF_BINTYPE_AR  3

/* return value of tinyld_add_file_internal(): 0, -1, or FILE_NOT_FOUND */
#define FILE_NOT_FOUND -2

/* ------------ lexer ------------ */

ST_DATA struct BufferedFile *file;
ST_DATA int tok;
ST_DATA CValue tokc;
ST_DATA const int *token_stream_ptr;
ST_DATA int parse_flags;
ST_DATA int tok_flags;
ST_DATA CString tokcstr; /* current parsed string, if any */

/* display benchmark infos */
ST_DATA int tok_ident;
ST_DATA TokenSym **table_ident;

#define TOK_FLAG_BOL   0x0001 /* beginning of line before */

#define PARSE_FLAG_LINEFEED   0x0002 /* line feed is returned as a
                                        token. line feed is also
                                        returned at eof */
#define PARSE_FLAG_ASM_FILE   0x0004 /* '#' can be used for line comments, etc. */
#define PARSE_FLAG_TOK_STR    0x0008 /* return parsed strings instead of TOK_PPSTR */

/* isidnum_table flags: */
#define IS_SPC 1
#define IS_ID  2
#define IS_NUM 4

ST_FUNC TokenSym *tok_alloc(const char *str, int len);
ST_FUNC int tok_alloc_const(const char *str);
ST_FUNC const char *get_tok_str(int v, CValue *cv);
ST_FUNC void begin_token_stream(TokenString *str, int alloc);
ST_FUNC void end_token_stream(void);
ST_FUNC int set_idnum(int c, int val);
ST_INLN void tok_str_new(TokenString *s);
ST_FUNC TokenString *tok_str_alloc(void);
ST_FUNC void tok_str_free(TokenString *s);
ST_FUNC void tok_str_free_str(int *str);
ST_FUNC void tok_str_add(TokenString *s, int t);
ST_FUNC void tok_str_add_tok(TokenString *s);
ST_FUNC void next(void);
ST_INLN void unget_tok(int last_tok);
ST_FUNC void tinyas_lexer_start(TCCState *s1);
ST_FUNC void tinyas_lexer_end(TCCState *s1);
ST_FUNC void skip(int c);
ST_FUNC NORETURN void expect(const char *msg);


/* space excluding newline */
static inline int is_space(int ch) {
    return ch == ' ' || ch == '\t' || ch == '\v' || ch == '\f' || ch == '\r';
}
static inline int isid(int c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}
static inline int isnum(int c) {
    return c >= '0' && c <= '9';
}
static inline int isoct(int c) {
    return c >= '0' && c <= '7';
}
static inline int toup(int c) {
    return (c >= 'a' && c <= 'z') ? c - 'a' + 'A' : c;
}

/* ------------ assembler symbol/code output support ------------ */

#define SYM_POOL_NB (8192 / sizeof(Sym))

ST_DATA int ind;
ST_DATA int nocode_wanted; /* true if no code generation wanted for an expression */

ST_FUNC ElfSym *elfsym(Sym *);
ST_FUNC void update_storage(Sym *sym);
ST_FUNC void put_extern_sym2(Sym *sym, int sh_num, addr_t value, unsigned long size);
ST_FUNC void put_extern_sym(Sym *sym, Section *section, addr_t value, unsigned long size);
ST_FUNC void greloca(Section *s, Sym *sym, unsigned long offset, int type, addr_t addend);

ST_INLN Sym *sym_find(int v);

ST_FUNC Sym *global_identifier_push(int v, int t, int c);

/* ------------ tccasm.c ------------ */
ST_FUNC int tinyas_assemble(TCCState *s1);
ST_FUNC Sym *get_asm_sym(int name, Sym *csym);
ST_FUNC void asm_expr(TCCState *s1, ExprValue *pe);
ST_FUNC int asm_int_expr(TCCState *s1);
ST_FUNC void gen_expr64(ExprValue *pe);
ST_FUNC void gen_expr32(ExprValue *pe);
ST_FUNC void asm_opcode(TCCState *s1, int opcode);
ST_FUNC int asm_parse_regvar(int t);

/* ------------ tinyas support ------------ */
ST_FUNC void g(int c);
ST_FUNC void gen_le16(int c);
ST_FUNC void gen_le32(int c);
ST_FUNC void gen_fill_nops(int bytes);
#ifdef TINY_TARGET_X86_64
ST_FUNC void gen_addr32(int r, Sym *sym, int c);
ST_FUNC void gen_addrpc32(int r, Sym *sym, int c);
#endif

/* ------------ tccelf.c ------------ */

#define ARMAG  "!<arch>\n"    /* For COFF and a.out archives */

ST_FUNC void elf_state_new(TCCState *s);
ST_FUNC void elf_state_delete(TCCState *s);
ST_FUNC Section *new_section(TCCState *s1, const char *name, int sh_type, int sh_flags);
ST_FUNC void section_realloc(Section *sec, unsigned long new_size);
ST_FUNC size_t section_add(Section *sec, addr_t size, int align);
ST_FUNC void *section_ptr_add(Section *sec, addr_t size);
ST_FUNC Section *find_section(TCCState *s1, const char *name);
ST_FUNC void free_section(Section *s);
ST_FUNC Section *new_symtab(TCCState *s1, const char *symtab_name, int sh_type, int sh_flags, const char *strtab_name, const char *hash_name, int hash_sh_flags);
ST_FUNC void init_symtab(Section *s);

ST_FUNC int put_elf_str(Section *s, const char *sym);
ST_FUNC int put_elf_sym(Section *s, addr_t value, unsigned long size, int info, int other, int shndx, const char *name);
ST_FUNC int set_elf_sym(Section *s, addr_t value, unsigned long size, int info, int other, int shndx, const char *name);
ST_FUNC int find_elf_sym(Section *s, const char *name);
ST_FUNC void put_elf_reloc(Section *symtab, Section *s, unsigned long offset, int type, int symbol);
ST_FUNC void put_elf_reloca(Section *symtab, Section *s, unsigned long offset, int type, int symbol, addr_t addend);

ST_FUNC void resolve_common_syms(TCCState *s1);
ST_FUNC void relocate_syms(TCCState *s1, Section *symtab, int do_resolve);
ST_FUNC void relocate_sections(TCCState *s1);

ST_FUNC ssize_t full_read(int fd, void *buf, size_t count);
ST_FUNC void *load_data(int fd, unsigned long file_offset, unsigned long size);
ST_FUNC int tinyld_object_type(int fd, ElfW(Ehdr) *h);
ST_FUNC int tinyld_load_object_file(TCCState *s1, int fd, unsigned long file_offset);
ST_FUNC int tinyld_load_archive(TCCState *s1, int fd, int alacarte);
ST_FUNC struct sym_attr *get_sym_attr(TCCState *s1, int index, int alloc);
ST_FUNC addr_t get_sym_addr(TCCState *s, const char *name, int err);
ST_FUNC int set_global_sym(TCCState *s1, const char *name, Section *sec, addr_t offs);

/* Browse each elem of type <type> in section <sec> starting at elem <startoff>
   using variable <elem> */
#define for_each_elem(sec, startoff, elem, type) \
    for (elem = (type *) sec->data + startoff; \
         elem < (type *) (sec->data + sec->data_offset); elem++)

/* ------------ xxx-link.c ------------ */

ST_FUNC int code_reloc (int reloc_type);
ST_FUNC int gotplt_entry_type (int reloc_type);
/* Whether to generate a GOT/PLT entry and when. NO_GOTPLT_ENTRY is first so
   that unknown relocation don't create a GOT or PLT entry */
enum gotplt_entry {
    NO_GOTPLT_ENTRY,	/* never generate (eg. GLOB_DAT & JMP_SLOT relocs) */
    BUILD_GOT_ONLY,	/* only build GOT (eg. TPOFF relocs) */
    AUTO_GOTPLT_ENTRY,	/* generate if sym is UNDEF */
    ALWAYS_GOTPLT_ENTRY	/* always generate (eg. PLTOFF relocs) */
};
#define NEED_RELOC_TYPE
ST_FUNC unsigned create_plt_entry(TCCState *s1, unsigned got_offset, struct sym_attr *attr);
ST_FUNC void relocate_plt(TCCState *s1);
ST_FUNC void build_got_entries(TCCState *s1, int got_sym); /* in tccelf.c */
#define NEED_BUILD_GOT

ST_FUNC void relocate(TCCState *s1, ElfW_Rel *rel, int type, unsigned char *ptr, addr_t addr, addr_t val);

static inline uint16_t read16le(unsigned char *p) {
    return p[0] | (uint16_t)p[1] << 8;
}
static inline void write16le(unsigned char *p, uint16_t x) {
    p[0] = x & 255;  p[1] = x >> 8 & 255;
}
static inline uint32_t read32le(unsigned char *p) {
    return read16le(p) | (uint32_t)read16le(p + 2) << 16;
}
static inline void write32le(unsigned char *p, uint32_t x) {
    write16le(p, x);  write16le(p + 2, x >> 16);
}
static inline void add32le(unsigned char *p, int32_t x) {
    write32le(p, read32le(p) + x);
}
static inline uint64_t read64le(unsigned char *p) {
    return read32le(p) | (uint64_t)read32le(p + 4) << 32;
}
static inline void write64le(unsigned char *p, uint64_t x) {
    write32le(p, x);  write32le(p + 4, x >> 32);
}
static inline void add64le(unsigned char *p, int64_t x) {
    write64le(p, read64le(p) + x);
}

#define ST_ASM_SET 0x04

/********************************************************/
#undef ST_DATA
#define ST_DATA
/********************************************************/

#define text_section        TCC_STATE_VAR(text_section)
#define data_section        TCC_STATE_VAR(data_section)
#define rodata_section      TCC_STATE_VAR(rodata_section)
#define bss_section         TCC_STATE_VAR(bss_section)
#define common_section      TCC_STATE_VAR(common_section)
#define cur_text_section    TCC_STATE_VAR(cur_text_section)
#define symtab_section      TCC_STATE_VAR(symtab_section)
#define tcc_error_noabort   TCC_SET_STATE(_tcc_error_noabort)
#define tcc_error           TCC_SET_STATE(_tcc_error)
#define tcc_warning         TCC_SET_STATE(_tcc_warning)

#define total_idents        TCC_STATE_VAR(total_idents)
#define total_lines         TCC_STATE_VAR(total_lines)
#define total_bytes         TCC_STATE_VAR(total_bytes)

PUB_FUNC void tcc_enter_state(TCCState *s1);
PUB_FUNC void tcc_exit_state(TCCState *s1);

/********************************************************/
#endif /* _TCC_H */

#undef TCC_STATE_VAR
#undef TCC_SET_STATE

#ifdef USING_GLOBALS
# define TCC_STATE_VAR(sym) tcc_state->sym
# define TCC_SET_STATE(fn) fn
# undef USING_GLOBALS
# undef _tcc_error
#else
# define TCC_STATE_VAR(sym) s1->sym
# define TCC_SET_STATE(fn) (tcc_enter_state(s1),fn)
# define _tcc_error use_tcc_error_noabort
#endif
