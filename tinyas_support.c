#define USING_GLOBALS
#include "tinyld.h"

#undef free

int ind;
int nocode_wanted;

static Sym *sym_free_first;
static void **sym_pools;
static int nb_sym_pools;

void libc_free(void *ptr)
{
    free(ptr);
}

ST_FUNC void tcc_open_bf(TCCState *s1, const char *filename, int initlen)
{
    BufferedFile *bf;
    int buflen = initlen ? initlen : IO_BUF_SIZE;

    bf = tcc_mallocz(sizeof(*bf) + buflen);
    bf->buf_ptr = bf->buffer;
    bf->buf_end = bf->buffer + initlen;
    bf->buf_end[0] = CH_EOB;
    pstrcpy(bf->filename, sizeof(bf->filename), filename);
    bf->line_num = 1;
    bf->fd = -1;
    bf->prev = file;
    file = bf;
    tok_flags = TOK_FLAG_BOL;
}

ST_FUNC void tcc_close(void)
{
    BufferedFile *bf = file;

    if (bf->fd > 0) {
        close(bf->fd);
        total_lines += bf->line_num - 1;
    }
    file = bf->prev;
    tcc_free(bf);
}

ST_FUNC int tcc_open(TCCState *s1, const char *filename)
{
    int fd;

    if (!strcmp(filename, "-")) {
        fd = 0;
        filename = "<stdin>";
    } else {
        fd = open(filename, O_RDONLY | O_BINARY);
    }
    if (fd < 0)
        return -1;
    tcc_open_bf(s1, filename, 0);
    file->fd = fd;
    return 0;
}

static Sym *__sym_malloc(void)
{
    Sym *pool, *sym;
    int i;

    pool = tcc_malloc(SYM_POOL_NB * sizeof(*pool));
    dynarray_add(&sym_pools, &nb_sym_pools, pool);
    for (i = 0; i < SYM_POOL_NB; i++) {
        sym = pool + i;
        sym->next = sym_free_first;
        sym_free_first = sym;
    }
    return sym_free_first;
}

static Sym *sym_malloc(void)
{
    Sym *sym = sym_free_first;

    if (!sym)
        sym = __sym_malloc();
    sym_free_first = sym->next;
    return sym;
}

static Sym *sym_new(int v, int t, int c)
{
    Sym *s = sym_malloc();

    memset(s, 0, sizeof(*s));
    s->v = v;
    s->type.t = t;
    s->c = c;
    return s;
}

ST_FUNC Sym *sym_find(int v)
{
    v -= TOK_IDENT;
    if ((unsigned)v >= (unsigned)(tok_ident - TOK_IDENT))
        return NULL;
    return table_ident[v]->sym_identifier;
}

ST_FUNC Sym *global_identifier_push(int v, int t, int c)
{
    Sym *s = sym_new(v, t, c);
    Sym **ps;

    if (v < SYM_FIRST_ANOM) {
        ps = &table_ident[v - TOK_IDENT]->sym_identifier;
        s->prev_tok = *ps;
        *ps = s;
    }
    return s;
}

ST_FUNC ElfSym *elfsym(Sym *s)
{
    if (!s || !s->c)
        return NULL;
    return &((ElfSym *)symtab_section->data)[s->c];
}

ST_FUNC void update_storage(Sym *sym)
{
    ElfSym *esym = elfsym(sym);
    int bind;

    if (!esym)
        return;
    if (sym->visibility)
        esym->st_other = (esym->st_other & ~ELFW(ST_VISIBILITY)(-1))
            | sym->visibility;
    if (esym->st_shndx == SHN_UNDEF)
        bind = sym->weak ? STB_WEAK : STB_GLOBAL;
    else if (sym->type.t & VT_STATIC)
        bind = STB_LOCAL;
    else if (sym->weak)
        bind = STB_WEAK;
    else
        bind = STB_GLOBAL;
    esym->st_info = ELFW(ST_INFO)(bind, ELFW(ST_TYPE)(esym->st_info));
}

ST_FUNC void put_extern_sym2(Sym *sym, int sh_num, addr_t value,
                             unsigned long size)
{
    const char *name;
    int type, bind, info;
    ElfSym *esym;

    if (!sym->c) {
        name = get_tok_str(sym->v, NULL);
        if (IS_ASM_FUNC(sym->type.t))
            type = STT_FUNC;
        else if ((sym->type.t & VT_BTYPE) == VT_VOID)
            type = STT_NOTYPE;
        else
            type = STT_OBJECT;
        if (sh_num == SHN_UNDEF)
            bind = STB_GLOBAL;
        else
            bind = (sym->type.t & VT_STATIC) ? STB_LOCAL : STB_GLOBAL;
        info = ELFW(ST_INFO)(bind, type);
        sym->c = put_elf_sym(symtab_section, value, size, info, 0, sh_num, name);
    } else {
        esym = elfsym(sym);
        esym->st_value = value;
        esym->st_size = size;
        esym->st_shndx = sh_num;
    }
    update_storage(sym);
}

ST_FUNC void put_extern_sym(Sym *sym, Section *section, addr_t value,
                            unsigned long size)
{
    put_extern_sym2(sym, section ? section->sh_num : SHN_UNDEF, value, size);
}

ST_FUNC void greloca(Section *s, Sym *sym, unsigned long offset, int type,
                     addr_t addend)
{
    int c = 0;

    if (sym) {
        if (!sym->c)
            put_extern_sym(sym, NULL, 0, 0);
        c = sym->c;
    }
    put_elf_reloca(symtab_section, s, offset, type, c, addend);
}

ST_FUNC void gen_fill_nops(int bytes)
{
#if defined TCC_TARGET_X86_64
    while (bytes-- > 0)
        g(0x90);
#elif defined TCC_TARGET_ARM64
    while (bytes >= 4) {
        gen_le32(0xd503201f);
        bytes -= 4;
    }
    while (bytes-- > 0)
        g(0);
#elif defined TCC_TARGET_RISCV64
    while (bytes >= 4) {
        gen_le32(0x00000013);
        bytes -= 4;
    }
    while (bytes-- > 0)
        g(0);
#endif
}

ST_FUNC int code_reloc(int reloc_type)
{
    (void)reloc_type;
    return -1;
}

ST_FUNC int gotplt_entry_type(int reloc_type)
{
    (void)reloc_type;
    return NO_GOTPLT_ENTRY;
}

ST_FUNC unsigned create_plt_entry(TCCState *s1, unsigned got_offset,
                                  struct sym_attr *attr)
{
    (void)s1;
    (void)got_offset;
    (void)attr;
    return 0;
}

ST_FUNC void relocate_plt(TCCState *s1)
{
    (void)s1;
}

ST_FUNC void relocate(TCCState *s1, ElfW_Rel *rel, int type,
                      unsigned char *ptr, addr_t addr, addr_t val)
{
    (void)s1;
    (void)rel;
    (void)type;
    (void)ptr;
    (void)addr;
    (void)val;
}

TinyASState *tinyas_new(void)
{
    TCCState *s = tcc_mallocz(sizeof(*s));

    s->tool_name = "tinyas";
    s->output_type = TCC_OUTPUT_OBJ;
    tccelf_new(s);
    return s;
}

void tinyas_delete(TinyASState *s)
{
    if (!s)
        return;
    tccelf_delete(s);
    dynarray_reset(&sym_pools, &nb_sym_pools);
    sym_free_first = NULL;
    tcc_free(s);
}

int tinyas_assemble_file(TinyASState *s, const char *filename)
{
    int ret = -1;
    int started = 0;

    tcc_enter_state(s);
    s->error_set_jmp_enabled = 1;
    if (setjmp(s->error_jmp_buf) == 0) {
        if (tcc_open(s, filename) < 0) {
            tcc_error_noabort("file '%s' not found", filename);
            goto out;
        }
        tcc_lexer_start(s);
        started = 1;
        ret = tcc_assemble(s);
        if (s->nb_errors)
            ret = -1;
    }
out:
    if (started || file)
        tcc_lexer_end(s);
    s->error_set_jmp_enabled = 0;
    tcc_exit_state(s);
    return ret;
}
