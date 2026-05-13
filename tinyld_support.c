#include "tinyld.h"

#undef free
#undef malloc
#undef realloc

TCCState *tcc_state;

PUB_FUNC void tcc_free(void *ptr)
{
    free(ptr);
}

PUB_FUNC void *tcc_malloc(unsigned long size)
{
    void *ptr = malloc(size ? size : 1);

    if (!ptr) {
        fputs("tinyld: out of memory\n", stderr);
        exit(1);
    }
    return ptr;
}

PUB_FUNC void *tcc_realloc(void *ptr, unsigned long size)
{
    ptr = realloc(ptr, size ? size : 1);
    if (!ptr) {
        fputs("tinyld: out of memory\n", stderr);
        exit(1);
    }
    return ptr;
}

PUB_FUNC void *tcc_mallocz(unsigned long size)
{
    void *ptr = tcc_malloc(size);
    memset(ptr, 0, size);
    return ptr;
}

PUB_FUNC char *tcc_strdup(const char *str)
{
    char *ptr = tcc_malloc(strlen(str) + 1);
    strcpy(ptr, str);
    return ptr;
}

PUB_FUNC void tcc_enter_state(TCCState *s1)
{
    tcc_state = s1;
}

PUB_FUNC void tcc_exit_state(TCCState *s1)
{
    (void)s1;
    tcc_state = NULL;
}

static void tinyld_vmessage(int is_error, const char *fmt, va_list ap)
{
    char msg[2048];
    TCCState *s1 = tcc_state;
    const char *tool = s1 && s1->tool_name ? s1->tool_name : "tinyld";

    vsnprintf(msg, sizeof(msg), fmt, ap);
    if (s1 && s1->current_filename)
        fprintf(stderr, "%s: %s: %s\n", tool, s1->current_filename, msg);
    else
        fprintf(stderr, "%s: %s\n", tool, msg);
    if (is_error && s1)
        s1->nb_errors++;
}

PUB_FUNC int _tcc_error_noabort(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    tinyld_vmessage(1, fmt, ap);
    va_end(ap);
    return -1;
}

#undef _tcc_error
PUB_FUNC void _tcc_error(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    tinyld_vmessage(1, fmt, ap);
    va_end(ap);
    if (tcc_state && tcc_state->error_set_jmp_enabled)
        longjmp(tcc_state->error_jmp_buf, 1);
    exit(1);
}
#define _tcc_error use_tcc_error_noabort

PUB_FUNC void _tcc_warning(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    tinyld_vmessage(0, fmt, ap);
    va_end(ap);
}

ST_FUNC char *pstrcpy(char *buf, size_t buf_size, const char *s)
{
    char *q = buf;
    char *end = buf + buf_size;

    if (buf_size) {
        while (q + 1 < end && *s)
            *q++ = *s++;
        *q = '\0';
    }
    return buf;
}

ST_FUNC char *pstrcat(char *buf, size_t buf_size, const char *s)
{
    size_t len = strlen(buf);

    if (len < buf_size)
        pstrcpy(buf + len, buf_size - len, s);
    return buf;
}

PUB_FUNC char *tiny_basename(const char *name)
{
    const char *p = name + strlen(name);

    while (p > name && !IS_DIRSEP(p[-1]))
        p--;
    return (char *)p;
}

PUB_FUNC char *tiny_fileextension(const char *name)
{
    char *base = tiny_basename(name);
    char *ext = strrchr(base, '.');

    return ext ? ext : base + strlen(base);
}

ST_FUNC void dynarray_add(void *ptab, int *nb_ptr, void *data)
{
    int nb = *nb_ptr;
    void **items = *(void ***)ptab;

    if ((nb & (nb - 1)) == 0) {
        int alloc = nb ? nb * 2 : 1;
        items = tcc_realloc(items, alloc * sizeof(*items));
        *(void ***)ptab = items;
    }
    items[nb++] = data;
    *nb_ptr = nb;
}

ST_FUNC void dynarray_reset(void *pp, int *n)
{
    void **items = *(void ***)pp;

    while (*n) {
        --*n;
        tcc_free(items[*n]);
    }
    tcc_free(items);
    *(void **)pp = NULL;
}

static void tinyld_split_path(TCCState *s, void *ary, int *nb_ary, const char *path)
{
    const char *p = path;
    const char *start;

    (void)s;
    while (*p) {
        size_t len;

        start = p;
        while (*p && *p != PATHSEP[0])
            p++;
        len = (size_t)(p - start);
        if (len) {
            char *entry = tcc_malloc(len + 1);
            memcpy(entry, start, len);
            entry[len] = '\0';
            dynarray_add(ary, nb_ary, entry);
        }
        if (*p)
            p++;
    }
}

TinyLDState *tinyld_new(void)
{
    TCCState *s = tcc_mallocz(sizeof(*s));

    s->tool_name = "tinyld";
    s->output_type = TINY_OUTPUT_EXE;
    elf_state_new(s);
    return s;
}

void tinyld_delete(TinyLDState *s)
{
    if (!s)
        return;
    elf_state_delete(s);
    dynarray_reset(&s->library_paths, &s->nb_library_paths);
    tcc_free(s->elf_entryname);
    tcc_free(s);
}

int tinyld_add_library_path(TinyLDState *s, const char *path)
{
    tinyld_split_path(s, &s->library_paths, &s->nb_library_paths, path);
    return 0;
}

static int tinyld_add_binary(TCCState *s1, int flags, const char *filename, int fd)
{
    ElfW(Ehdr) ehdr;
    const char *saved_filename = s1->current_filename;
    int obj_type;
    int ret;

    s1->current_filename = filename;
    obj_type = tinyld_object_type(fd, &ehdr);
    lseek(fd, 0, SEEK_SET);
    switch (obj_type) {
    case AFF_BINTYPE_REL:
        ret = tinyld_load_object_file(s1, fd, 0);
        break;
    case AFF_BINTYPE_AR:
        ret = tinyld_load_archive(s1, fd, !(flags & AFF_WHOLE_ARCHIVE));
        break;
    case AFF_BINTYPE_DYN:
        ret = tcc_error_noabort("%s: dynamic libraries are not supported", filename);
        break;
    default:
        ret = tcc_error_noabort("%s: unrecognized file type", filename);
        break;
    }
    close(fd);
    s1->current_filename = saved_filename;
    return ret;
}

ST_FUNC int tinyld_add_file_internal(TCCState *s1, const char *filename, int flags)
{
    int fd = open(filename, O_RDONLY | O_BINARY);

    if (fd < 0) {
        if (flags & AFF_PRINT_ERROR)
            return tcc_error_noabort("file '%s' not found", filename);
        return FILE_NOT_FOUND;
    }
    return tinyld_add_binary(s1, flags, filename, fd);
}

int tinyld_add_file(TinyLDState *s, const char *filename)
{
    int flags = AFF_PRINT_ERROR;

    if (s->whole_archive)
        flags |= AFF_WHOLE_ARCHIVE;
    return tinyld_add_file_internal(s, filename, flags);
}

static int tinyld_add_library_internal(TCCState *s1, const char *fmt,
                                       const char *name, int flags)
{
    char path[1024];
    int i;

    for (i = 0; i < s1->nb_library_paths; i++) {
        snprintf(path, sizeof(path), fmt, s1->library_paths[i], name);
        if (tinyld_add_file_internal(s1, path, flags & ~AFF_PRINT_ERROR) != FILE_NOT_FOUND)
            return s1->nb_errors ? -1 : 0;
    }
    if (flags & AFF_PRINT_ERROR)
        return tcc_error_noabort("library '%s' not found", name);
    return FILE_NOT_FOUND;
}

int tinyld_add_library(TinyLDState *s, const char *library_name)
{
    int flags = s->whole_archive ? AFF_WHOLE_ARCHIVE : 0;

    if (library_name[0] == ':')
        return tinyld_add_library_internal(s, "%s/%s", library_name + 1,
                                           flags | AFF_PRINT_ERROR);
    return tinyld_add_library_internal(s, "%s/lib%s.a", library_name,
                                       flags | AFF_PRINT_ERROR);
}
