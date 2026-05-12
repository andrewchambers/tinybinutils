#define USING_GLOBALS
#include "tinyld.h"

#undef free

Sym *global_stack;
Sym *local_stack;
Sym *local_label_stack;
Sym *global_label_stack;
Sym *define_stack;
CType int_type, func_old_type, char_pointer_type;
SValue *vtop;
int rsym, anon_sym, ind, loc;
char debug_modes;
int nocode_wanted;
int global_expr;
CType func_vt;
int func_var;
int func_vc;
int func_ind;
const char *funcname;

#if defined TCC_TARGET_X86_64
const char * const target_machine_defs =
    "__x86_64__\0"
    "__x86_64\0"
    "__amd64__\0";
#elif defined TCC_TARGET_ARM64
const char * const target_machine_defs =
    "__aarch64__\0";
#elif defined TCC_TARGET_RISCV64
const char * const target_machine_defs =
    "__riscv\0"
    "__riscv_xlen 64\0";
#endif

static Sym *sym_free_first;
static void **sym_pools;
static int nb_sym_pools;
static int local_scope;

void libc_free(void *ptr)
{
    free(ptr);
}

ST_FUNC int normalized_PATHCMP(const char *f1, const char *f2)
{
    char *p1, *p2;
    int ret = 1;

    p1 = realpath(f1, NULL);
    if (p1) {
        p2 = realpath(f2, NULL);
        if (p2) {
            ret = PATHCMP(p1, p2);
            free(p2);
        }
        free(p1);
    }
    return ret;
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
    bf->true_filename = bf->filename;
    bf->line_num = 1;
    bf->ifdef_stack_ptr = s1->ifdef_stack_ptr;
    bf->fd = -1;
    bf->prev = file;
    bf->prev_tok_flags = tok_flags;
    file = bf;
    tok_flags = TOK_FLAG_BOL | TOK_FLAG_BOF;
}

ST_FUNC void tcc_close(void)
{
    BufferedFile *bf = file;

    if (bf->fd > 0) {
        close(bf->fd);
        total_lines += bf->line_num - 1;
    }
    if (bf->true_filename != bf->filename)
        tcc_free(bf->true_filename);
    file = bf->prev;
    tok_flags = bf->prev_tok_flags;
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

ST_FUNC void sym_free(Sym *sym)
{
    sym->next = sym_free_first;
    sym_free_first = sym;
}

ST_FUNC Sym *sym_push2(Sym **ps, int v, int t, int c)
{
    Sym *s = sym_malloc();

    memset(s, 0, sizeof(*s));
    s->v = v;
    s->type.t = t;
    s->c = c;
    s->prev = *ps;
    *ps = s;
    return s;
}

ST_FUNC Sym *sym_find2(Sym *s, int v)
{
    while (s) {
        if (s->v == v)
            return s;
        s = s->prev;
    }
    return NULL;
}

static void sym_link(Sym *s, int yes)
{
    TokenSym *ts = table_ident[(s->v & ~SYM_STRUCT) - TOK_IDENT];
    Sym **ps = (s->v & SYM_STRUCT) ? &ts->sym_struct : &ts->sym_identifier;

    if (yes) {
        s->prev_tok = *ps;
        *ps = s;
        s->sym_scope = local_scope;
    } else {
        *ps = s->prev_tok;
    }
}

ST_FUNC Sym *sym_push(int v, CType *type, int r, int c)
{
    Sym **ps = local_stack ? &local_stack : &global_stack;
    Sym *s = sym_push2(ps, v, type->t, c);

    s->type.ref = type->ref;
    s->r = r;
    if ((v & ~SYM_STRUCT) < SYM_FIRST_ANOM)
        sym_link(s, 1);
    return s;
}

ST_FUNC void sym_pop(Sym **ptop, Sym *b, int keep)
{
    Sym *s = *ptop;

    while (s != b) {
        Sym *prev = s->prev;
        int v = s->v;

        if ((v & ~SYM_STRUCT) < SYM_FIRST_ANOM)
            sym_link(s, 0);
        if (!keep)
            sym_free(s);
        s = prev;
    }
    if (!keep)
        *ptop = b;
}

ST_FUNC Sym *sym_find(int v)
{
    v -= TOK_IDENT;
    if ((unsigned)v >= (unsigned)(tok_ident - TOK_IDENT))
        return NULL;
    return table_ident[v]->sym_identifier;
}

ST_FUNC Sym *struct_find(int v)
{
    v -= TOK_IDENT;
    if ((unsigned)v >= (unsigned)(tok_ident - TOK_IDENT))
        return NULL;
    return table_ident[v]->sym_struct;
}

ST_FUNC Sym *global_identifier_push(int v, int t, int c)
{
    Sym *s = sym_push2(&global_stack, v, t, c);
    Sym **ps;

    s->r = VT_CONST | VT_SYM;
    if (v < SYM_FIRST_ANOM) {
        ps = &table_ident[v - TOK_IDENT]->sym_identifier;
        while (*ps && (*ps)->sym_scope)
            ps = &(*ps)->prev_tok;
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
    if (sym->a.visibility)
        esym->st_other = (esym->st_other & ~ELFW(ST_VISIBILITY)(-1))
            | sym->a.visibility;
    if (esym->st_shndx == SHN_UNDEF)
        bind = sym->a.weak ? STB_WEAK : STB_GLOBAL;
    else if (sym->type.t & VT_STATIC)
        bind = STB_LOCAL;
    else if (sym->a.weak)
        bind = STB_WEAK;
    else
        bind = STB_GLOBAL;
    esym->st_info = ELFW(ST_INFO)(bind, ELFW(ST_TYPE)(esym->st_info));
}

ST_FUNC void put_extern_sym2(Sym *sym, int sh_num, addr_t value,
                             unsigned long size, int can_add_underscore)
{
    const char *name;
    char buf[256];
    int type, bind, info;
    ElfSym *esym;

    if (!sym->c) {
        name = sym->asm_label ? get_tok_str(sym->asm_label, NULL)
                              : get_tok_str(sym->v, NULL);
        if (tcc_state->leading_underscore && can_add_underscore) {
            buf[0] = '_';
            pstrcpy(buf + 1, sizeof(buf) - 1, name);
            name = buf;
        }
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
    put_extern_sym2(sym, section ? section->sh_num : SHN_UNDEF, value, size, 1);
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

ST_FUNC void tcc_debug_start(TCCState *s1)
{
    (void)s1;
}

ST_FUNC void tcc_debug_line(TCCState *s1)
{
    (void)s1;
}

ST_FUNC void tcc_debug_end(TCCState *s1)
{
    (void)s1;
}

ST_FUNC void tcc_debug_newfile(TCCState *s1)
{
    (void)s1;
}

ST_FUNC void tcc_debug_bincl(TCCState *s1)
{
    (void)s1;
}

ST_FUNC void tcc_debug_eincl(TCCState *s1)
{
    (void)s1;
}

ST_FUNC int tcc_set_options(TCCState *s, const char *str)
{
    (void)s;
    (void)str;
    return 0;
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

static int64_t pp_expr_lor(void);

static int64_t pp_number(void)
{
    int64_t value;
    char *end;

    switch (tok) {
    case TOK_CCHAR:
    case TOK_LCHAR:
    case TOK_CINT:
    case TOK_CUINT:
    case TOK_CLLONG:
    case TOK_CULLONG:
    case TOK_CLONG:
    case TOK_CULONG:
        value = (int64_t)tokc.i;
        next();
        return value;
    case TOK_PPNUM:
        value = (int64_t)strtoll(tokc.str.data, &end, 0);
        if (*end)
            tcc_error("invalid number in preprocessor expression");
        next();
        return value;
    default:
        break;
    }
    if (tok == '(') {
        next();
        value = pp_expr_lor();
        skip(')');
        return value;
    }
    tcc_error("invalid preprocessor expression");
}

static int64_t pp_expr_unary(void)
{
    int op;

    switch (tok) {
    case '+':
        next();
        return pp_expr_unary();
    case '-':
    case '!':
    case '~':
        op = tok;
        next();
        if (op == '-')
            return -pp_expr_unary();
        if (op == '!')
            return !pp_expr_unary();
        return ~pp_expr_unary();
    default:
        return pp_number();
    }
}

static int64_t pp_expr_mul(void)
{
    int64_t value = pp_expr_unary();

    while (tok == '*' || tok == '/' || tok == '%') {
        int op = tok;
        int64_t rhs;

        next();
        rhs = pp_expr_unary();
        if ((op == '/' || op == '%') && rhs == 0)
            tcc_error("division by zero in preprocessor expression");
        if (op == '*')
            value *= rhs;
        else if (op == '/')
            value /= rhs;
        else
            value %= rhs;
    }
    return value;
}

static int64_t pp_expr_add(void)
{
    int64_t value = pp_expr_mul();

    while (tok == '+' || tok == '-') {
        int op = tok;
        int64_t rhs;

        next();
        rhs = pp_expr_mul();
        value = op == '+' ? value + rhs : value - rhs;
    }
    return value;
}

static int64_t pp_expr_shift(void)
{
    int64_t value = pp_expr_add();

    while (tok == TOK_SHL || tok == TOK_SAR || tok == TOK_SHR) {
        int op = tok;
        int64_t rhs;

        next();
        rhs = pp_expr_add();
        if (op == TOK_SHL)
            value <<= rhs;
        else
            value >>= rhs;
    }
    return value;
}

static int64_t pp_expr_rel(void)
{
    int64_t value = pp_expr_shift();

    while (tok == TOK_LT || tok == TOK_LE || tok == TOK_GT || tok == TOK_GE ||
           tok == TOK_ULT || tok == TOK_ULE || tok == TOK_UGT || tok == TOK_UGE) {
        int op = tok;
        int64_t rhs;

        next();
        rhs = pp_expr_shift();
        switch (op) {
        case TOK_LT:
            value = value < rhs;
            break;
        case TOK_LE:
            value = value <= rhs;
            break;
        case TOK_GT:
            value = value > rhs;
            break;
        case TOK_GE:
            value = value >= rhs;
            break;
        case TOK_ULT:
            value = (uint64_t)value < (uint64_t)rhs;
            break;
        case TOK_ULE:
            value = (uint64_t)value <= (uint64_t)rhs;
            break;
        case TOK_UGT:
            value = (uint64_t)value > (uint64_t)rhs;
            break;
        default:
            value = (uint64_t)value >= (uint64_t)rhs;
            break;
        }
    }
    return value;
}

static int64_t pp_expr_eq(void)
{
    int64_t value = pp_expr_rel();

    while (tok == TOK_EQ || tok == TOK_NE) {
        int op = tok;
        int64_t rhs;

        next();
        rhs = pp_expr_rel();
        value = op == TOK_EQ ? value == rhs : value != rhs;
    }
    return value;
}

static int64_t pp_expr_band(void)
{
    int64_t value = pp_expr_eq();

    while (tok == '&') {
        next();
        value &= pp_expr_eq();
    }
    return value;
}

static int64_t pp_expr_xor(void)
{
    int64_t value = pp_expr_band();

    while (tok == '^') {
        next();
        value ^= pp_expr_band();
    }
    return value;
}

static int64_t pp_expr_bor(void)
{
    int64_t value = pp_expr_xor();

    while (tok == '|') {
        next();
        value |= pp_expr_xor();
    }
    return value;
}

static int64_t pp_expr_land(void)
{
    int64_t value = pp_expr_bor();

    while (tok == TOK_LAND) {
        next();
        value = pp_expr_bor() && value;
    }
    return value;
}

static int64_t pp_expr_lor(void)
{
    int64_t value = pp_expr_land();

    while (tok == TOK_LOR) {
        next();
        value = pp_expr_land() || value;
    }
    return value;
}

ST_FUNC int expr_const(void)
{
    return (int)pp_expr_lor();
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
    s->nostdlib = 1;
    s->dollars_in_identifiers = 1;
    s->cversion = 199901;
    s->ppfp = stdout;
    tccelf_new(s);
    return s;
}

void tinyas_delete(TinyASState *s)
{
    if (!s)
        return;
    tccelf_delete(s);
    dynarray_reset(&s->include_paths, &s->nb_include_paths);
    dynarray_reset(&s->sysinclude_paths, &s->nb_sysinclude_paths);
    cstr_free(&s->cmdline_defs);
    cstr_free(&s->cmdline_incl);
    dynarray_reset(&sym_pools, &nb_sym_pools);
    sym_free_first = NULL;
    global_stack = local_stack = NULL;
    global_label_stack = local_label_stack = NULL;
    define_stack = NULL;
    tcc_free(s);
}

int tinyas_add_include_path(TinyASState *s, const char *path)
{
    dynarray_add(&s->include_paths, &s->nb_include_paths, tcc_strdup(path));
    return 0;
}

void tinyas_define_symbol(TinyASState *s, const char *definition)
{
    const char *eq = strchr(definition, '=');

    if (eq)
        cstr_printf(&s->cmdline_defs, "#define %.*s %s\n",
                    (int)(eq - definition), definition, eq + 1);
    else
        cstr_printf(&s->cmdline_defs, "#define %s 1\n", definition);
}

void tinyas_undefine_symbol(TinyASState *s, const char *name)
{
    cstr_printf(&s->cmdline_defs, "#undef %s\n", name);
}

int tinyas_assemble_file(TinyASState *s, const char *filename, int preprocess)
{
    int ret = -1;
    int started = 0;
    int filetype = preprocess ? AFF_TYPE_ASMPP : AFF_TYPE_ASM;

    tcc_enter_state(s);
    s->error_set_jmp_enabled = 1;
    if (setjmp(s->error_jmp_buf) == 0) {
        if (tcc_open(s, filename) < 0) {
            tcc_error_noabort("file '%s' not found", filename);
            goto out;
        }
        preprocess_start(s, filetype);
        started = 1;
        ret = tcc_assemble(s, preprocess);
        if (s->nb_errors)
            ret = -1;
    }
out:
    if (started || file)
        preprocess_end(s);
    s->error_set_jmp_enabled = 0;
    tcc_exit_state(s);
    return ret;
}
