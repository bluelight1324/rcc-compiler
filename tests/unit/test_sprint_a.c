/* test_sprint_a.c
 * Task 8.5 Sprint A — Correctness fixes for RCC v5.15.0
 *
 * Tests:
 *   1.4 Struct pointer stride   — p++ on struct pointer uses sizeof(struct) not 1
 *   1.3 Unsigned comparisons    — setb/seta/setbe/setae for unsigned operands
 *   1.3 Unsigned right shift    — shr (logical) for unsigned, sar (arithmetic) for signed
 *   1.3 Unsigned division       — div/xor rdx for unsigned, idiv/cqo for signed
 *   5.1 GNU extension stubs     — __attribute__, __builtin_expect, __builtin_unreachable, __extension__
 *
 * Expected: exit code 0, no compile errors.
 */
#include <stdio.h>
#include <stddef.h>
#include <stdbit.h>

/* ── 1.4: Struct pointer stride ─────────────────────────────────────────────*/
typedef struct { int x; int y; } Point2;   /* sizeof = 8 */
typedef struct { int a; int b; int c; int d; } Quad;  /* sizeof = 16 */

static int test_ptr_stride(void) {
    int pass = 0;
    Point2 arr[4];
    arr[0].x = 10; arr[0].y = 20;
    arr[1].x = 30; arr[1].y = 40;
    arr[2].x = 50; arr[2].y = 60;
    arr[3].x = 70; arr[3].y = 80;

    /* ++ should advance by sizeof(Point2) = 8, not by 1 */
    Point2 *p = arr;
    p++;
    if (p->x == 30 && p->y == 40) pass++; /* 1 */

    /* -- should retreat by sizeof(Point2) */
    p--;
    if (p->x == 10 && p->y == 20) pass++; /* 2 */

    /* pointer + integer: p + 2 should reach arr[2] */
    Point2 *q = arr + 2;
    if (q->x == 50 && q->y == 60) pass++; /* 3 */

    /* pointer subtraction: (arr+3) - arr == 3 elements */
    long diff = (arr + 3) - arr;
    if (diff == 3) pass++; /* 4 */

    /* 16-byte stride: Quad array */
    Quad qa[3];
    qa[0].a = 1; qa[1].a = 2; qa[2].a = 3;
    Quad *qp = qa;
    qp++;
    if (qp->a == 2) pass++; /* 5 */

    /* Subscript on struct array */
    if (arr[2].x == 50) pass++; /* 6 */

    return pass; /* 6 checks */
}

/* ── 1.3: Unsigned comparison ────────────────────────────────────────────── */
static int test_unsigned_cmp(void) {
    int pass = 0;
    unsigned int big = 0xFFFFFFFFU;  /* 4294967295 */
    unsigned int one = 1U;

    /* Without unsigned comparisons, signed interpretation: big = -1, so big < 1 */
    if (big > one) pass++;   /* should be true: 4294967295 > 1 */  /* 1 */
    if (one < big) pass++;   /* should be true */                   /* 2 */
    if (big >= one) pass++;  /* should be true */                   /* 3 */
    if (one <= big) pass++;  /* should be true */                   /* 4 */

    /* boundary: 0 < UINT_MAX */
    unsigned int z = 0U;
    if (big > z) pass++;    /* 5 */
    if (z < big) pass++;    /* 6 */

    return pass; /* 6 checks */
}

/* ── 1.3: Unsigned right shift — logical vs arithmetic ──────────────────── */
static int test_unsigned_shift(void) {
    int pass = 0;
    unsigned int u = 0x80000000U;  /* top bit set */
    /* Logical right shift: fills with 0, not sign-extended 1s */
    unsigned int us = u >> 1;
    if (us == 0x40000000U) pass++;  /* 1: should be 0x40000000 */

    signed int s = (signed int)0x80000000;  /* negative if treated as signed */
    signed int ss = s >> 1;
    /* Arithmetic: fills with 1s, so result is negative */
    if (ss < 0) pass++;  /* 2 */

    return pass; /* 2 checks */
}

/* ── 1.3: Unsigned division ─────────────────────────────────────────────── */
static int test_unsigned_div(void) {
    int pass = 0;
    unsigned int n = 0xFFFFFFFEU;  /* large positive unsigned = 4294967294 */
    unsigned int d = 2U;
    unsigned int q = n / d;
    if (q == 0x7FFFFFFFU) pass++;  /* 1: 4294967294 / 2 = 2147483647 */

    unsigned int r = n % d;
    if (r == 0U) pass++;  /* 2: no remainder */

    return pass; /* 2 checks */
}

/* ── 5.1: GNU extension stubs ───────────────────────────────────────────── */
/* __attribute__((packed)) — should be silently ignored by RCC */
typedef struct __attribute__((packed)) { char c; int i; } PackedStruct;

/* __builtin_expect — should expand to the expression */
static int always_true_flag = 1;
static int test_gnu_exts(void) {
    int pass = 0;

    /* __builtin_expect: the hint is discarded, expression value preserved */
    int val = __builtin_expect(always_true_flag, 1);
    if (val == 1) pass++;  /* 1 */

    /* __extension__: no-op */
    __extension__ int ext_var = 42;
    if (ext_var == 42) pass++;  /* 2 */

    /* __builtin_popcount: mapped to stdc_count_ones */
    int pop = __builtin_popcount(0xFF);
    if (pop == 8) pass++;  /* 3 */

    /* __builtin_offsetof: mapped to offsetof */
    typedef struct { int a; double b; } S2;
    size_t off = __builtin_offsetof(S2, b);
    if (off == 8) pass++;  /* 4 */

    return pass; /* 4 checks */
}

int main(void) {
    int pass = 0;

    pass += test_ptr_stride();      /*  6 */
    pass += test_unsigned_cmp();    /*  6 */
    pass += test_unsigned_shift();  /*  2 */
    pass += test_unsigned_div();    /*  2 */
    pass += test_gnu_exts();        /*  4 */

    printf("sprint_a: %d/20 passed\n", pass);
    return (pass == 20) ? 0 : 1;
}
