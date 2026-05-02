#include <stdio.h>

/* gen_cvt_csti test: verify narrow-type conversions in expressions.
   Without the fix, TCC's riscv64 backend could miss the conversion
   step when promoting a narrow result back to int, producing wrong
   values (e.g., treating a char as still 32-bit).  */

int main(void)
{
    int ok = 1;
    int x = 0x12345678;

    /* Cast to char then add 1 — result must be 8-bit.  */
    char c = (char)x + 1;
    unsigned char uc = (unsigned char)x + 1;
    short s = (short)x + 1;
    unsigned short us = (unsigned short)x + 1;

    printf("c=%x uc=%x s=%x us=%x\n",
           (unsigned char)c, (unsigned)uc,
           (unsigned short)s, (unsigned)us);

    if (c != (char)0x78 + 1) {
        printf("FAIL: char conversion\n");
        ok = 0;
    }
    if (uc != (unsigned char)0x78 + 1) {
        printf("FAIL: unsigned char conversion\n");
        ok = 0;
    }
    if (s != (short)0x5678 + 1) {
        printf("FAIL: short conversion\n");
        ok = 0;
    }
    if (us != (unsigned short)0x5678 + 1) {
        printf("FAIL: unsigned short conversion\n");
        ok = 0;
    }

    printf("%s\n", ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}
