/* test_c11_math.c
 * RCC Task 7.38 — C11 <math.h> runtime test.
 *
 * Tests:
 *   M1: Basic double arithmetic functions (fabs, sqrt, pow, floor, ceil)
 *   M2: Classification macros (isnan, isinf, isfinite, signbit)
 *   M3: fmin / fmax
 *   M4: INFINITY and NAN macros defined (compile-check via use)
 *
 * Expected: RCC compile + run → exit 0.
 */
#include <stdio.h>
#include <math.h>

static int g_failures = 0;
static void check(const char* name, int cond) {
    if (!cond) { printf("FAIL: %s\n", name); g_failures++; }
}

/* Double comparison with small tolerance */
static int deq(double a, double b) {
    double diff = a - b;
    if (diff < 0.0) diff = -diff;
    return diff < 1e-9;
}

int main(void) {
    printf("=== C11 Math Library Test ===\n");

    /* ── M1: Basic math functions ─────────────────────────────────────────── */
    check("fabs(-2.5)==2.5",     deq(fabs(-2.5), 2.5));
    check("fabs(0.0)==0.0",      deq(fabs(0.0), 0.0));
    check("sqrt(4.0)==2.0",      deq(sqrt(4.0), 2.0));
    check("sqrt(9.0)==3.0",      deq(sqrt(9.0), 3.0));
    check("pow(2.0,10.0)==1024", deq(pow(2.0, 10.0), 1024.0));
    check("pow(3.0,2.0)==9.0",   deq(pow(3.0, 2.0), 9.0));
    check("floor(2.9)==2.0",     deq(floor(2.9), 2.0));
    check("floor(-2.1)==-3.0",   deq(floor(-2.1), -3.0));
    check("ceil(2.1)==3.0",      deq(ceil(2.1), 3.0));
    check("ceil(-2.9)==-2.0",    deq(ceil(-2.9), -2.0));
    check("round(2.5)==3.0",     deq(round(2.5), 3.0));
    check("trunc(2.9)==2.0",     deq(trunc(2.9), 2.0));

    /* ── M2: fmin / fmax ─────────────────────────────────────────────────── */
    check("fmin(1.0,2.0)==1.0",  deq(fmin(1.0, 2.0), 1.0));
    check("fmax(1.0,2.0)==2.0",  deq(fmax(1.0, 2.0), 2.0));
    check("fmin(-1.0,1.0)==-1.0",deq(fmin(-1.0, 1.0), -1.0));

    /* ── M3: Classification macros ───────────────────────────────────────── */
    /* Use non-constant divisors to force runtime FP division */
    float fzero = 0.0f;
    float fone  = 1.0f;
    float my_nan = fzero / fzero;   /* IEEE 754: NaN */
    float my_inf = fone  / fzero;   /* IEEE 754: +Infinity */

    check("isnan(NaN)",       isnan(my_nan));
    check("!isnan(1.0)",      !isnan(1.0));
    check("isinf(+Inf)",      isinf(my_inf));
    check("!isinf(1.0)",      !isinf(1.0));
    check("isfinite(1.0)",    isfinite(1.0));
    check("!isfinite(+Inf)",  !isfinite(my_inf));
    check("signbit(-1.0)",    signbit(-1.0));
    check("!signbit(1.0)",    !signbit(1.0));
    check("signbit(-0.0)",    signbit(-0.0));

    /* ── M4: INFINITY and NAN macros usable in expressions ───────────────── */
    /* Just verify they compile and produce the right type of value */
    double inf_val = HUGE_VAL;
    double nan_val = NAN;
    check("HUGE_VAL is inf", isinf(inf_val));
    check("NAN is nan",      isnan(nan_val));

    if (g_failures == 0) printf("ALL MATH CHECKS PASSED\n");
    else printf("%d MATH CHECK(S) FAILED\n", g_failures);
    return g_failures;
}
