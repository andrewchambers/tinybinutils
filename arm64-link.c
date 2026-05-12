#ifdef TARGET_DEFS_ONLY

#define EM_TCC_TARGET EM_AARCH64

#define R_DATA_32  R_AARCH64_ABS32
#define R_DATA_PTR R_AARCH64_ABS64
#define R_JMP_SLOT R_AARCH64_JUMP_SLOT
#define R_GLOB_DAT R_AARCH64_GLOB_DAT
#define R_COPY     R_AARCH64_COPY
#define R_RELATIVE R_AARCH64_RELATIVE

#define R_NUM      R_AARCH64_NUM

#define ELF_START_ADDR 0x00400000
#define ELF_PAGE_SIZE  0x10000

#define PCRELATIVE_DLLPLT 1
#define RELOCATE_DLLPLT 1

#else

#include "tcc.h"

#define ARM64_ADD_IMM   0x11000000U
#define ARM64_ADRP      0x90000000U
#define ARM64_BR        0xD61F0000U
#define ARM64_LDR_X     0xF9400000U
#define ARM64_NOP       0xD503201FU
#define ARM64_STP_X_PRE 0xA9800000U
#define ARM64_RD(r)     ((uint32_t)(r) & 0x1FU)
#define ARM64_RN(r)     (((uint32_t)(r) & 0x1FU) << 5)
#define ARM64_RT(r)     ((uint32_t)(r) & 0x1FU)
#define ARM64_RT2(r)    (((uint32_t)(r) & 0x1FU) << 10)
#define ARM64_IMM7(v)   (((uint32_t)(v) & 0x7FU) << 15)
#define ARM64_SF(s)     (((uint32_t)(s) & 1) << 31)

ST_FUNC int code_reloc(int reloc_type)
{
    switch (reloc_type) {
    case R_AARCH64_ABS32:
    case R_AARCH64_ABS64:
    case R_AARCH64_PREL32:
    case R_AARCH64_MOVW_UABS_G0_NC:
    case R_AARCH64_MOVW_UABS_G1_NC:
    case R_AARCH64_MOVW_UABS_G2_NC:
    case R_AARCH64_MOVW_UABS_G3:
    case R_AARCH64_ADR_PREL_PG_HI21:
    case R_AARCH64_ADD_ABS_LO12_NC:
    case R_AARCH64_ADR_GOT_PAGE:
    case R_AARCH64_LD64_GOT_LO12_NC:
    case R_AARCH64_LDST128_ABS_LO12_NC:
    case R_AARCH64_LDST64_ABS_LO12_NC:
    case R_AARCH64_LDST32_ABS_LO12_NC:
    case R_AARCH64_LDST16_ABS_LO12_NC:
    case R_AARCH64_LDST8_ABS_LO12_NC:
    case R_AARCH64_GLOB_DAT:
    case R_AARCH64_COPY:
        return 0;

    case R_AARCH64_JUMP26:
    case R_AARCH64_CALL26:
    case R_AARCH64_JUMP_SLOT:
    case R_AARCH64_CONDBR19:
    case R_AARCH64_TSTBR14:
        return 1;
    }
    return -1;
}

ST_FUNC int gotplt_entry_type(int reloc_type)
{
    switch (reloc_type) {
    case R_AARCH64_PREL32:
    case R_AARCH64_MOVW_UABS_G0_NC:
    case R_AARCH64_MOVW_UABS_G1_NC:
    case R_AARCH64_MOVW_UABS_G2_NC:
    case R_AARCH64_MOVW_UABS_G3:
    case R_AARCH64_ADR_PREL_PG_HI21:
    case R_AARCH64_ADD_ABS_LO12_NC:
    case R_AARCH64_LDST128_ABS_LO12_NC:
    case R_AARCH64_LDST64_ABS_LO12_NC:
    case R_AARCH64_LDST32_ABS_LO12_NC:
    case R_AARCH64_LDST16_ABS_LO12_NC:
    case R_AARCH64_LDST8_ABS_LO12_NC:
    case R_AARCH64_GLOB_DAT:
    case R_AARCH64_JUMP_SLOT:
    case R_AARCH64_COPY:
    case R_AARCH64_CONDBR19:
    case R_AARCH64_TSTBR14:
        return NO_GOTPLT_ENTRY;

    case R_AARCH64_ABS32:
    case R_AARCH64_ABS64:
    case R_AARCH64_JUMP26:
    case R_AARCH64_CALL26:
        return AUTO_GOTPLT_ENTRY;

    case R_AARCH64_ADR_GOT_PAGE:
    case R_AARCH64_LD64_GOT_LO12_NC:
        return ALWAYS_GOTPLT_ENTRY;
    }
    return -1;
}

ST_FUNC unsigned create_plt_entry(TCCState *s1, unsigned got_offset,
                                  struct sym_attr *attr)
{
    Section *plt = s1->plt;
    uint8_t *p;
    unsigned plt_offset;

    (void)attr;
    if (plt->data_offset == 0)
        section_ptr_add(plt, 32);
    plt_offset = plt->data_offset;

    p = section_ptr_add(plt, 16);
    write32le(p, got_offset);
    write32le(p + 4, (uint64_t)got_offset >> 32);
    return plt_offset;
}

ST_FUNC void relocate_plt(TCCState *s1)
{
    uint8_t *p, *p_end;

    if (!s1->plt)
        return;

    p = s1->plt->data;
    p_end = p + s1->plt->data_offset;
    if (p < p_end) {
        uint64_t plt = s1->plt->sh_addr;
        uint64_t got = s1->got->sh_addr + 16;
        uint64_t off = (got >> 12) - (plt >> 12);
        if ((off + ((uint32_t)1 << 20)) >> 21)
            tcc_error_noabort("failed relocating PLT");
        write32le(p, ARM64_STP_X_PRE | ARM64_RT(16) | ARM64_RT2(30) |
                     ARM64_RN(31) | ARM64_IMM7(-2));
        write32le(p + 4, ARM64_ADRP | ARM64_RD(16) |
                         (off & 0x1ffffc) << 3 | (off & 3) << 29);
        write32le(p + 8, ARM64_LDR_X | ARM64_RT(17) | ARM64_RN(16) |
                         (got & 0xff8) << 7);
        write32le(p + 12, ARM64_ADD_IMM | ARM64_SF(1) | ARM64_RD(16) |
                          ARM64_RN(16) | (got & 0xfff) << 10);
        write32le(p + 16, ARM64_BR | ARM64_RN(17));
        write32le(p + 20, ARM64_NOP);
        write32le(p + 24, ARM64_NOP);
        write32le(p + 28, ARM64_NOP);
        p += 32;
        got = s1->got->sh_addr;
        while (p < p_end) {
            uint64_t pc = plt + (p - s1->plt->data);
            uint64_t addr = got + read64le(p);
            off = (addr >> 12) - (pc >> 12);
            if ((off + ((uint32_t)1 << 20)) >> 21)
                tcc_error_noabort("failed relocating PLT");
            write32le(p, ARM64_ADRP | ARM64_RD(16) |
                         (off & 0x1ffffc) << 3 | (off & 3) << 29);
            write32le(p + 4, ARM64_LDR_X | ARM64_RT(17) | ARM64_RN(16) |
                             (addr & 0xff8) << 7);
            write32le(p + 8, ARM64_ADD_IMM | ARM64_SF(1) | ARM64_RD(16) |
                             ARM64_RN(16) | (addr & 0xfff) << 10);
            write32le(p + 12, ARM64_BR | ARM64_RN(17));
            p += 16;
        }
    }

    if (s1->plt->reloc) {
        ElfW_Rel *rel;
        p = s1->got->data;
        for_each_elem(s1->plt->reloc, 0, rel, ElfW_Rel)
            write64le(p + rel->r_offset, s1->plt->sh_addr);
    }
}

ST_FUNC void relocate(TCCState *s1, ElfW_Rel *rel, int type,
                      unsigned char *ptr, addr_t addr, addr_t val)
{
    int sym_index = ELFW(R_SYM)(rel->r_info);

    switch (type) {
    case R_AARCH64_ABS64:
        add64le(ptr, val);
        return;
    case R_AARCH64_ABS32:
        add32le(ptr, val);
        return;
    case R_AARCH64_PREL32:
        add32le(ptr, val - addr);
        return;
    case R_AARCH64_MOVW_UABS_G0_NC:
        write32le(ptr, (read32le(ptr) & 0xffe0001f) |
                       ((val & 0xffff) << 5));
        return;
    case R_AARCH64_MOVW_UABS_G1_NC:
        write32le(ptr, (read32le(ptr) & 0xffe0001f) |
                       (((val >> 16) & 0xffff) << 5));
        return;
    case R_AARCH64_MOVW_UABS_G2_NC:
        write32le(ptr, (read32le(ptr) & 0xffe0001f) |
                       (((val >> 32) & 0xffff) << 5));
        return;
    case R_AARCH64_MOVW_UABS_G3:
        write32le(ptr, (read32le(ptr) & 0xffe0001f) |
                       (((val >> 48) & 0xffff) << 5));
        return;
    case R_AARCH64_ADR_PREL_PG_HI21: {
        uint64_t off = (val >> 12) - (addr >> 12);
        if ((off + ((uint64_t)1 << 20)) >> 21)
            tcc_error_noabort("R_AARCH64_ADR_PREL_PG_HI21 relocation failed");
        write32le(ptr, (read32le(ptr) & 0x9f00001f) |
                       ((off & 0x1ffffc) << 3) | ((off & 3) << 29));
        return;
    }
    case R_AARCH64_ADD_ABS_LO12_NC:
    case R_AARCH64_LDST8_ABS_LO12_NC:
        write32le(ptr, (read32le(ptr) & 0xffc003ff) |
                       ((val & 0xfff) << 10));
        return;
    case R_AARCH64_LDST16_ABS_LO12_NC:
        write32le(ptr, (read32le(ptr) & 0xffc003ff) |
                       ((val & 0xffe) << 9));
        return;
    case R_AARCH64_LDST32_ABS_LO12_NC:
        write32le(ptr, (read32le(ptr) & 0xffc003ff) |
                       ((val & 0xffc) << 8));
        return;
    case R_AARCH64_LDST64_ABS_LO12_NC:
        write32le(ptr, (read32le(ptr) & 0xffc003ff) |
                       ((val & 0xff8) << 7));
        return;
    case R_AARCH64_LDST128_ABS_LO12_NC:
        write32le(ptr, (read32le(ptr) & 0xffc003ff) |
                       ((val & 0xff0) << 6));
        return;
    case R_AARCH64_CONDBR19:
        if (((val - addr) + ((uint64_t)1 << 20)) & ~(uint64_t)0x1ffffc)
            tcc_error_noabort("R_AARCH64_CONDBR19 relocation failed");
        write32le(ptr, (read32le(ptr) & 0xff00001f) |
                       (((val - addr) >> 2 & 0x7ffff) << 5));
        return;
    case R_AARCH64_TSTBR14:
        if (((val - addr) + ((uint64_t)1 << 15)) & ~(uint64_t)0xfffc)
            tcc_error_noabort("R_AARCH64_TSTBR14 relocation failed");
        write32le(ptr, (read32le(ptr) & 0xfff8001f) |
                       (((val - addr) >> 2 & 0x3fff) << 5));
        return;
    case R_AARCH64_JUMP26:
    case R_AARCH64_CALL26:
        if (((val - addr) + ((uint64_t)1 << 27)) & ~(uint64_t)0xffffffc) {
            const char *name = (char *)symtab_section->link->data +
                ((ElfW(Sym) *)symtab_section->data)[sym_index].st_name;
            tcc_error_noabort("R_AARCH64_(JUMP|CALL)26 relocation failed for '%s'",
                              name);
        }
        write32le(ptr, 0x14000000 |
                       ((uint32_t)(type == R_AARCH64_CALL26) << 31) |
                       (((val - addr) >> 2) & 0x3ffffff));
        return;
    case R_AARCH64_ADR_GOT_PAGE: {
        uint64_t got_addr = s1->got->sh_addr +
            get_sym_attr(s1, sym_index, 0)->got_offset;
        uint64_t off = (got_addr >> 12) - (addr >> 12);
        if ((off + ((uint64_t)1 << 20)) >> 21)
            tcc_error_noabort("R_AARCH64_ADR_GOT_PAGE relocation failed");
        write32le(ptr, (read32le(ptr) & 0x9f00001f) |
                       ((off & 0x1ffffc) << 3) | ((off & 3) << 29));
        return;
    }
    case R_AARCH64_LD64_GOT_LO12_NC:
        write32le(ptr, (read32le(ptr) & 0xfff803ff) |
                       (((s1->got->sh_addr +
                          get_sym_attr(s1, sym_index, 0)->got_offset) &
                         0xff8) << 7));
        return;
    case R_AARCH64_COPY:
        return;
    case R_AARCH64_GLOB_DAT:
    case R_AARCH64_JUMP_SLOT:
        write64le(ptr, val - rel->r_addend);
        return;
    case R_AARCH64_RELATIVE:
        return;
    default:
        fprintf(stderr, "FIXME: handle reloc type %x at %x [%p] to %x\n",
                type, (unsigned)addr, ptr, (unsigned)val);
        return;
    }
}

#endif
