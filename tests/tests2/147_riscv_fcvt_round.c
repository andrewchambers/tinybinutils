#include <stdio.h>

#ifdef __riscv

int main(void)
{
    /* fcvt with optional rounding mode operand (GNU as syntax) */
    asm volatile("fcvt.w.s a0, fa0, rne");
    asm volatile("fcvt.w.s a0, fa0, rtz");
    asm volatile("fcvt.w.s a0, fa0, rup");
    asm volatile("fcvt.w.d a0, fa0, rne");
    asm volatile("fcvt.w.d a0, fa0, rtz");

    printf("PASS\n");
    return 0;
}

#else
int main(void) { printf("SKIP\n"); return 0; }
#endif
