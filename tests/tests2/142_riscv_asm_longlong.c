#include <stdio.h>

/* P1.1: riscv64 inline asm with 64-bit immediate (li).
   Tests that long long immediates assemble correctly,
   including the lui+addi sequence for large constants.  */

#ifdef __riscv

long long test_li_small(void)
{
    long long r;
    asm("li %0, 42" : "=r"(r));
    return r;
}

long long test_li_large(void)
{
    long long r;
    asm("li %0, 0x123456789ABCDEF0" : "=r"(r));
    return r;
}

long long test_li_negative(void)
{
    long long r;
    asm("li %0, -1" : "=r"(r));
    return r;
}

int main(void)
{
    int ok = 1;

    if (test_li_small() != 42) {
        printf("FAIL: li small\n");
        ok = 0;
    }
    if (test_li_large() != 0x123456789ABCDEF0LL) {
        printf("FAIL: li large\n");
        ok = 0;
    }
    if (test_li_negative() != -1) {
        printf("FAIL: li negative\n");
        ok = 0;
    }

    printf("%s\n", ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}

#else
int main(void)
{
    printf("SKIP\n");
    return 0;
}
#endif
