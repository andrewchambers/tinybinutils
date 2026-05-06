#include <stdio.h>

#ifdef __riscv

int main(void)
{
    /* AMO base (all funct5 now match GNU as) */
    asm volatile("amoadd.w a0, a1, (sp)");
    asm volatile("amoswap.w a0, a1, (sp)");
    asm volatile("amoand.w a0, a1, (sp)");
    asm volatile("amoor.d a0, a1, (sp)");
    asm volatile("amoxor.w a0, a1, (sp)");
    asm volatile("amomax.w a0, a1, (sp)");
    asm volatile("amomaxu.d a0, a1, (sp)");
    asm volatile("amomin.w a0, a1, (sp)");
    asm volatile("amominu.d a0, a1, (sp)");

    /* AMO aq/rl ordering suffixes */
    asm volatile("amoadd.w.aq a0, a1, (sp)");
    asm volatile("amoadd.w.rl a0, a1, (sp)");
    asm volatile("amoadd.d.aqrl a0, a1, (sp)");

    printf("PASS\n");
    return 0;
}

#else
int main(void) { printf("SKIP\n"); return 0; }
#endif
