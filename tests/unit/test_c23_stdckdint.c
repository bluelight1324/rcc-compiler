/* test_c23_stdckdint.c
 * Task 7.44 — C23 §7.20: Checked integer arithmetic (stdckdint.h)
 *
 * Verifies that ckd_add, ckd_sub, ckd_mul compile and detect overflow.
 *
 * Expected: compile with exit 0
 */
#include <stdio.h>
#include <stdckdint.h>

static int g_fails = 0;
static void check(const char *name, int cond) {
    if (!cond) { printf("FAIL: %s\n", name); g_fails++; }
}

int main(void) {
    printf("=== stdckdint.h Test ===\n");
    int r;
    int ov;

    /* ckd_add: no overflow */
    ov = ckd_add(&r, 10, 20);
    check("ckd_add(10+20) result == 30",    r  == 30);
    check("ckd_add(10+20) no overflow",     ov == 0);

    /* ckd_add: overflow */
    ov = ckd_add(&r, 2000000000, 2000000000);
    check("ckd_add overflow detected",      ov != 0);

    /* ckd_sub: no overflow */
    ov = ckd_sub(&r, 100, 40);
    check("ckd_sub(100-40) result == 60",   r  == 60);
    check("ckd_sub(100-40) no overflow",    ov == 0);

    /* ckd_sub: underflow */
    ov = ckd_sub(&r, -2000000000, 2000000000);
    check("ckd_sub underflow detected",     ov != 0);

    /* ckd_mul: no overflow */
    ov = ckd_mul(&r, 6, 7);
    check("ckd_mul(6*7) result == 42",      r  == 42);
    check("ckd_mul(6*7) no overflow",       ov == 0);

    /* ckd_mul: overflow */
    ov = ckd_mul(&r, 100000, 100000);
    check("ckd_mul overflow detected",      ov != 0);

    if (g_fails == 0) printf("ALL STDCKDINT TESTS PASSED\n");
    else printf("%d STDCKDINT TEST(S) FAILED\n", g_fails);
    return g_fails;
}
