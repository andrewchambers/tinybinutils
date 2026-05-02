#include <stdio.h>

/* P0.4 + P1.4: riscv64 asm pseudo-instructions test.
   Exercises neg/negw, sext.w, fmv.s/d, fneg.s/d.  */

#ifdef __riscv

int test_neg(int x)
{
    int r;
    asm("neg %0, %1" : "=r"(r) : "r"(x));
    return r;
}

long long test_negw(long long x)
{
    int r;
    asm("negw %0, %1" : "=r"(r) : "r"((int)x));
    return (long long)r;
}

long long test_sextw(int x)
{
    long long r;
    asm("sext.w %0, %1" : "=r"(r) : "r"(x));
    return r;
}

float test_fmv_s(float a)
{
    float r;
    asm("fmv.s %0, %1" : "=f"(r) : "f"(a));
    return r;
}

float test_fneg_s(float a)
{
    float r;
    asm("fneg.s %0, %1" : "=f"(r) : "f"(a));
    return r;
}

double test_fmv_d(double a)
{
    double r;
    asm("fmv.d %0, %1" : "=f"(r) : "f"(a));
    return r;
}

double test_fneg_d(double a)
{
    double r;
    asm("fneg.d %0, %1" : "=f"(r) : "f"(a));
    return r;
}

int main(void)
{
    int ok = 1;

    if (test_neg(42) != -42) {
        printf("FAIL: neg\n");
        ok = 0;
    }
    if (test_negw(100) != -100) {
        printf("FAIL: negw\n");
        ok = 0;
    }
    if (test_sextw(0x80000000) != 0xffffffff80000000LL) {
        printf("FAIL: sext.w\n");
        ok = 0;
    }
    if (test_fmv_s(3.14f) != 3.14f) {
        printf("FAIL: fmv.s\n");
        ok = 0;
    }
    if (test_fneg_s(3.14f) != -3.14f) {
        printf("FAIL: fneg.s\n");
        ok = 0;
    }
    if (test_fmv_d(2.718281828) != 2.718281828) {
        printf("FAIL: fmv.d\n");
        ok = 0;
    }
    if (test_fneg_d(2.718281828) != -2.718281828) {
        printf("FAIL: fneg.d\n");
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
