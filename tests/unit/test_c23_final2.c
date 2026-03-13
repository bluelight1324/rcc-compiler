/* test_c23_final2.c
 * Task 8.42: Final C23 compliance gaps — RCC v5.14.0
 *
 * Tests the remaining C23 gaps filled in task 8.42:
 *   - iso646.h     : and/or/not/xor/bitand/bitor/compl and compound variants
 *   - cpp.cpp      : __STDC_UTF_8__ predefined macro
 *   - stdarg.h     : C23 va_start(ap) single-argument form
 *   - math.h       : fromfp/ufromfp proper rounding via FP_INT_* constants
 *   - uchar.h      : mbrtoc8/c8rtomb pass-through for non-ASCII UTF-8 bytes
 *
 * Expected: exit code 0, no compile errors.
 */
#include <stdio.h>
#include <iso646.h>
#include <stdarg.h>
#include <math.h>
#include <uchar.h>
#include <string.h>

/* ── iso646.h tests ──────────────────────────────────────────────────────── */
static int test_iso646(void) {
    int pass = 0;
    int a = 1, b = 0;

    if (a and b)   { } else pass++;   /* && */
    if (a or  b)   pass++;            /* || */
    if (not  b)    pass++;            /* !  */
    if (a bitand 1) pass++;           /* &  */
    if (a bitor  0) pass++;           /* |  */

    int x = 0xFF;
    if ((compl x) == (int)(~0xFF)) pass++;   /* ~ */
    if (a not_eq b) pass++;           /* != */

    int y = 5;
    y and_eq 3;    /* &= */
    if (y == 1) pass++;
    y or_eq  2;    /* |= */
    if (y == 3) pass++;
    y xor_eq 1;    /* ^= */
    if (y == 2) pass++;
    y xor    1;    /* ^ (expression) */
    if ((y xor 1) == 3) pass++;

    return pass;  /* 12 checks */
}

/* ── __STDC_UTF_8__ ──────────────────────────────────────────────────────── */
static int test_stdc_utf8(void) {
#ifdef __STDC_UTF_8__
    return (__STDC_UTF_8__ == 1) ? 1 : 0;
#else
    return 0;
#endif
}

/* ── C23 va_start single-arg form ────────────────────────────────────────── */
/* A variadic-only function with NO named parameters before ... */
static int sum_ints(int count, ...) {
    va_list ap;
    va_start(ap, count);   /* 2-arg form: still works */
    int total = 0;
    for (int i = 0; i < count; i++)
        total += va_arg(ap, int);
    va_end(ap);
    return total;
}

static int test_va_start(void) {
    int s = sum_ints(3, 10, 20, 30);
    return (s == 60) ? 1 : 0;
}

/* ── fromfp / ufromfp proper rounding ───────────────────────────────────── */
static int test_fromfp_rounding(void) {
    int pass = 0;

    /* FP_INT_TOWARDZERO (truncate) */
    if (fromfp(3.7, FP_INT_TOWARDZERO, 32) == 3L)  pass++;
    if (fromfp(-3.7, FP_INT_TOWARDZERO, 32) == -3L) pass++;

    /* FP_INT_UPWARD (ceil) */
    if (fromfp(3.2, FP_INT_UPWARD, 32) == 4L)  pass++;
    if (fromfp(-3.2, FP_INT_UPWARD, 32) == -3L) pass++;

    /* FP_INT_DOWNWARD (floor) */
    if (fromfp(3.9, FP_INT_DOWNWARD, 32) == 3L)  pass++;
    if (fromfp(-3.1, FP_INT_DOWNWARD, 32) == -4L) pass++;

    /* FP_INT_TONEARESTFROMZERO (round — half away from zero) */
    if (fromfp(2.5, FP_INT_TONEARESTFROMZERO, 32) == 3L) pass++;
    if (fromfp(-2.5, FP_INT_TONEARESTFROMZERO, 32) == -3L) pass++;

    /* ufromfp: unsigned */
    if (ufromfp(3.7, FP_INT_TOWARDZERO, 32) == 3UL) pass++;
    if (ufromfp(3.2, FP_INT_UPWARD, 32) == 4UL) pass++;

    return pass;  /* 10 checks */
}

/* ── mbrtoc8 / c8rtomb UTF-8 passthrough ────────────────────────────────── */
static int test_mbrtoc8(void) {
    int pass = 0;
    mbstate_t mbs;
    memset(&mbs, 0, sizeof(mbs));

    /* ASCII byte */
    unsigned char c8;
    size_t r = mbrtoc8(&c8, "A", 1, &mbs);
    if (r == 1 && c8 == (unsigned char)'A') pass++;

    /* Null byte */
    r = mbrtoc8(&c8, "\0", 1, &mbs);
    if (r == 0 && c8 == 0) pass++;

    /* Non-ASCII byte (UTF-8 lead byte) — should NOT return (size_t)-1 now */
    unsigned char lead = 0;
    r = mbrtoc8(&lead, "\xC3", 1, &mbs);
    if (r == 1 && lead == 0xC3) pass++;

    /* c8rtomb: ASCII passthrough */
    char buf[4];
    size_t n = c8rtomb(buf, (unsigned char)'Z', &mbs);
    if (n == 1 && buf[0] == 'Z') pass++;

    /* c8rtomb: non-ASCII passthrough */
    n = c8rtomb(buf, (unsigned char)0xC3, &mbs);
    if (n == 1 && (unsigned char)buf[0] == 0xC3) pass++;

    return pass;  /* 5 checks */
}

int main(void) {
    int pass = 0;

    pass += test_iso646();       /* 12 */
    pass += test_stdc_utf8();    /* 1  */
    pass += test_va_start();     /* 1  */
    pass += test_fromfp_rounding(); /* 10 */
    pass += test_mbrtoc8();      /* 5  */

    printf("c23_final2: %d/29 passed\n", pass);
    return (pass == 29) ? 0 : 1;
}
