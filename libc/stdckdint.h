/* stdckdint.h — C23 §7.20 Checked Integer Arithmetic
 *
 * Provides macros for performing integer arithmetic with overflow detection.
 * Each macro stores the result in *r and returns true (1) if overflow occurred,
 * false (0) if the result is exact.
 *
 * Usage:  int result;
 *         if (ckd_add(&result, a, b)) -- overflow detected --
 *
 * Implementation: portable static helper functions; no compiler extensions.
 */

#ifndef _STDCKDINT_H
#define _STDCKDINT_H

/* ── Signed int helpers ───────────────────────────────────────────────── */

static int __ckd_add_i(int* r, int a, int b) {
    *r = a + b;
    return (b > 0 && *r < a) || (b < 0 && *r > a);
}
static int __ckd_sub_i(int* r, int a, int b) {
    *r = a - b;
    return (b < 0 && *r < a) || (b > 0 && *r > a);
}
static int __ckd_mul_i(int* r, int a, int b) {
    long long s = (long long)a * (long long)b;
    *r = (int)s;
    return (long long)*r != s;
}

/* ── Long long helpers ────────────────────────────────────────────────── */

static int __ckd_add_ll(long long* r, long long a, long long b) {
    *r = a + b;
    return (b > 0 && *r < a) || (b < 0 && *r > a);
}
static int __ckd_sub_ll(long long* r, long long a, long long b) {
    *r = a - b;
    return (b < 0 && *r < a) || (b > 0 && *r > a);
}
static int __ckd_mul_ll(long long* r, long long a, long long b) {
    /* Detect overflow via unsigned widening — works for the common INT range */
    unsigned long long ua = (unsigned long long)(a < 0 ? -a : a);
    unsigned long long ub = (unsigned long long)(b < 0 ? -b : b);
    long long result = a * b;
    *r = result;
    if (ua == 0 || ub == 0) return 0;
    return (unsigned long long)(result < 0 ? -result : result) != ua * ub;
}

/* ── Public macros ────────────────────────────────────────────────────── */

/* ckd_add(r, a, b): *r = a+b; returns 1 on signed overflow */
#define ckd_add(r, a, b) __ckd_add_i((r), (int)(a), (int)(b))

/* ckd_sub(r, a, b): *r = a-b; returns 1 on signed overflow */
#define ckd_sub(r, a, b) __ckd_sub_i((r), (int)(a), (int)(b))

/* ckd_mul(r, a, b): *r = a*b; returns 1 on signed overflow */
#define ckd_mul(r, a, b) __ckd_mul_i((r), (int)(a), (int)(b))

#endif /* _STDCKDINT_H */
