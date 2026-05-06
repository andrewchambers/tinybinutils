#include <stdio.h>

#ifdef __riscv

int main(void)
{
    int ok = 1;
    int old, tmp;

    asm volatile("csrr %0, 0x003" : "=r"(old));

    asm volatile("csrr %0, 0x003" : "=r"(tmp));
    printf("csrr fcsr=%x\n", (unsigned)tmp);

    asm volatile("csrw 0x003, %0" : : "r"(0xE0));
    asm volatile("csrr %0, 0x003" : "=r"(tmp));
    printf("csrw: wrote e0 got %x\n", (unsigned)tmp);
    if (tmp != 0xE0) { printf("FAIL: csrw\n"); ok = 0; }
    asm volatile("csrw 0x003, %0" : : "r"(old));

    asm volatile("csrwi 0x003, 0x10");
    asm volatile("csrr %0, 0x003" : "=r"(tmp));
    printf("csrwi: wrote 0x10 got %x\n", (unsigned)tmp);
    if (tmp != 0x10) { printf("FAIL: csrwi\n"); ok = 0; }
    asm volatile("csrw 0x003, %0" : : "r"(old));

    asm volatile("csrsi 0x003, 0x03");
    asm volatile("csrr %0, 0x003" : "=r"(tmp));
    printf("csrsi: old|3=%x\n", (unsigned)tmp);
    if ((old | 0x03) != tmp) { printf("FAIL: csrsi\n"); ok = 0; }
    asm volatile("csrw 0x003, %0" : : "r"(old));

    asm volatile("csrci 0x003, 0x03");
    asm volatile("csrr %0, 0x003" : "=r"(tmp));
    printf("csrci: old&~3=%x\n", (unsigned)tmp);
    if ((old & ~0x03) != tmp) { printf("FAIL: csrci\n"); ok = 0; }
    asm volatile("csrw 0x003, %0" : : "r"(old));

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
