#include <stdio.h>

/* PROMOTE_RET test: verify char/short return values are properly
   zero/sign-extended to 64-bit per the RISC-V integer calling convention.
   Without PROMOTE_RET, upper bits of the return register may contain
   garbage, causing incorrect results when assigned to a wider type.  */

unsigned char get_uc(void) { return 0x80; }
signed char get_sc(void) { return 0x80; }
unsigned short get_us(void) { return 0x8000; }
signed short get_ss(void) { return 0x8000; }

/* Prevent inlining to force ABI-compliant calling.  */
unsigned char (* volatile fp_uc)(void) = get_uc;
signed char (* volatile fp_sc)(void) = get_sc;
unsigned short (* volatile fp_us)(void) = get_us;
signed short (* volatile fp_ss)(void) = get_ss;

int main(void)
{
    int ok = 1;
    unsigned long long uc = fp_uc();
    signed long long sc = fp_sc();
    unsigned long long us = fp_us();
    signed long long ss = fp_ss();

    printf("uc=%llx sc=%llx us=%llx ss=%llx\n",
           (unsigned long long)uc,
           (unsigned long long)sc,
           (unsigned long long)us,
           (unsigned long long)ss);

    if (uc != 0x80) {
        printf("FAIL: uc not zero-extended\n");
        ok = 0;
    }
    if (sc != 0xffffffffffffff80LL) {
        printf("FAIL: sc not sign-extended\n");
        ok = 0;
    }
    if (us != 0x8000) {
        printf("FAIL: us not zero-extended\n");
        ok = 0;
    }
    if (ss != 0xffffffffffff8000LL) {
        printf("FAIL: ss not sign-extended\n");
        ok = 0;
    }

    printf("%s\n", ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}
