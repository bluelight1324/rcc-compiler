/* test_dead_code_elim.c
 * Task 7.47 — P3-C: Dead-code elimination for constant if/while conditions
 *
 * When the condition of if() or while() is a compile-time constant, the
 * compiler should emit only the live branch (no dead code).
 * Expected: compile with exit 0.
 */
#include <stdio.h>

static int g_fails = 0;
static void check(const char *name, int cond) {
    if (!cond) { printf("FAIL: %s\n", name); g_fails++; }
}

int main(void) {
    printf("=== Dead-Code Elimination Test ===\n");

    /* Constant-true if: only then-branch should be reached */
    int x = 0;
    if (1) x = 1;
    else   x = 99;  /* dead branch */
    check("constant-true if: x == 1", x == 1);

    /* Constant-false if: only else-branch should be reached */
    int y = 0;
    if (0) y = 99;  /* dead branch */
    else   y = 2;
    check("constant-false if: y == 2", y == 2);

    /* Constant-false if with no else: body should be skipped */
    int z = 7;
    if (0) z = 99;  /* dead branch */
    check("constant-false if no-else: z == 7", z == 7);

    /* while (0): body never executed */
    int w = 5;
    while (0) { w = 99; }
    check("while(0) body never runs: w == 5", w == 5);

    /* Constant expression from constexpr/enum */
    enum { ZERO = 0, ONE = 1 };
    int a = 0;
    if (ONE) a = 3;
    check("constant enum true: a == 3", a == 3);

    int b = 0;
    if (ZERO) b = 99;
    else      b = 4;
    check("constant enum false: b == 4", b == 4);

    if (g_fails == 0) printf("ALL DEAD-CODE ELIM TESTS PASSED\n");
    else printf("%d DEAD-CODE ELIM TEST(S) FAILED\n", g_fails);
    return g_fails;
}
