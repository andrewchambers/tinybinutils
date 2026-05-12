#define USING_GLOBALS
#include "tinyld.h"

ST_FUNC void g(int c)
{
    int next;

    if (nocode_wanted)
        return;
    next = ind + 1;
    if ((unsigned long)next > cur_text_section->data_allocated)
        section_realloc(cur_text_section, next);
    cur_text_section->data[ind] = c;
    ind = next;
}

ST_FUNC void gen_le16(int c)
{
    g(c);
    g(c >> 8);
}

ST_FUNC void gen_le32(int c)
{
    g(c);
    g(c >> 8);
    g(c >> 16);
    g(c >> 24);
}

ST_FUNC void gen_addr32(int r, Sym *sym, int c)
{
    if (r & VT_SYM) {
        greloca(cur_text_section, sym, ind, R_X86_64_32S, c);
        c = 0;
    }
    gen_le32(c);
}

ST_FUNC void gen_addrpc32(int r, Sym *sym, int c)
{
    if (r & VT_SYM) {
        greloca(cur_text_section, sym, ind, R_X86_64_PC32, c - 4);
        c = 4;
    }
    gen_le32(c - 4);
}
