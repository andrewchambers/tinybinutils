#include <stdio.h>

#ifdef __riscv

int main(void)
{
    /* F/D comparison (use raw regs to avoid inline asm float→int bug) */
    asm volatile("feq.s a0, fa0, fa1");
    asm volatile("feq.d a0, fa0, fa1");
    asm volatile("flt.s a0, fa0, fa1");
    asm volatile("flt.d a0, fa0, fa1");
    asm volatile("fle.s a0, fa0, fa1");
    asm volatile("fle.d a0, fa0, fa1");

    /* fcvt conversions */
    asm volatile("fcvt.w.s a0, fa0");
    asm volatile("fcvt.wu.s a0, fa0");
    asm volatile("fcvt.s.w fa0, a0");
    asm volatile("fcvt.w.d a0, fa0");
    asm volatile("fcvt.d.w fa0, a0");
    asm volatile("fcvt.d.s fa0, fa0");
    asm volatile("fcvt.s.d fa0, fa0");

    /* fclass */
    asm volatile("fclass.s a0, fa0");
    asm volatile("fclass.d a0, fa0");

    printf("PASS\n");
    return 0;
}

#else
int main(void) { printf("SKIP\n"); return 0; }
#endif
