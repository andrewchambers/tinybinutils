#include <stdio.h>

/* P1.3: riscv64 F/D extension arithmetic instructions.
   Tests fadd/fsub/fmul/fdiv for both single and double precision.  */

#ifdef __riscv

float test_fadd_s(float a, float b)
{
    float r;
    asm("fadd.s %0, %1, %2" : "=f"(r) : "f"(a), "f"(b));
    return r;
}

float test_fsub_s(float a, float b)
{
    float r;
    asm("fsub.s %0, %1, %2" : "=f"(r) : "f"(a), "f"(b));
    return r;
}

float test_fmul_s(float a, float b)
{
    float r;
    asm("fmul.s %0, %1, %2" : "=f"(r) : "f"(a), "f"(b));
    return r;
}

float test_fdiv_s(float a, float b)
{
    float r;
    asm("fdiv.s %0, %1, %2" : "=f"(r) : "f"(a), "f"(b));
    return r;
}

double test_fadd_d(double a, double b)
{
    double r;
    asm("fadd.d %0, %1, %2" : "=f"(r) : "f"(a), "f"(b));
    return r;
}

double test_fsub_d(double a, double b)
{
    double r;
    asm("fsub.d %0, %1, %2" : "=f"(r) : "f"(a), "f"(b));
    return r;
}

double test_fmul_d(double a, double b)
{
    double r;
    asm("fmul.d %0, %1, %2" : "=f"(r) : "f"(a), "f"(b));
    return r;
}

double test_fdiv_d(double a, double b)
{
    double r;
    asm("fdiv.d %0, %1, %2" : "=f"(r) : "f"(a), "f"(b));
    return r;
}

int main(void)
{
    int ok = 1;

    if (test_fadd_s(1.5f, 2.5f) != 4.0f) {
        printf("FAIL: fadd.s\n");
        ok = 0;
    }
    if (test_fsub_s(5.0f, 2.0f) != 3.0f) {
        printf("FAIL: fsub.s\n");
        ok = 0;
    }
    if (test_fmul_s(3.0f, 4.0f) != 12.0f) {
        printf("FAIL: fmul.s\n");
        ok = 0;
    }
    if (test_fdiv_s(12.0f, 4.0f) != 3.0f) {
        printf("FAIL: fdiv.s\n");
        ok = 0;
    }

    if (test_fadd_d(1.5, 2.5) != 4.0) {
        printf("FAIL: fadd.d\n");
        ok = 0;
    }
    if (test_fsub_d(5.0, 2.0) != 3.0) {
        printf("FAIL: fsub.d\n");
        ok = 0;
    }
    if (test_fmul_d(3.0, 4.0) != 12.0) {
        printf("FAIL: fmul.d\n");
        ok = 0;
    }
    if (test_fdiv_d(12.0, 4.0) != 3.0) {
        printf("FAIL: fdiv.d\n");
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
