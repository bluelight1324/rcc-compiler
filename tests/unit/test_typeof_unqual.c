/* test_typeof_unqual.c
 * Task 7.43 — B1: typeof_unqual keyword (C23 §6.7.2)
 *
 * Verifies that typeof_unqual is recognized as a keyword.
 * In RCC, typeof/typeof_unqual are handled in expression contexts
 * (sizeof(typeof(x))). typeof_unqual has the same semantics as typeof.
 *
 * Expected: compile + run → exit 0
 */
#include <stdio.h>

static int g_fails = 0;
static void check(const char* name, int cond) {
    if (!cond) { printf("FAIL: %s\n", name); g_fails++; }
}

int main(void) {
    printf("=== typeof_unqual Test ===\n");

    int x = 42;
    long y = 100L;
    char c = 'A';

    /* T1: sizeof(typeof_unqual(x)) == sizeof(int) */
    int sz_x = sizeof(typeof_unqual(x));
    check("T1: sizeof(typeof_unqual(int var)) == 4", sz_x == 4);

    /* T2: sizeof(typeof_unqual(y)) == sizeof(long) */
    int sz_y = sizeof(typeof_unqual(y));
    check("T2: sizeof(typeof_unqual(long var)) == 8", sz_y == 8);

    /* T3: sizeof(typeof_unqual(c)) == sizeof(char) */
    int sz_c = sizeof(typeof_unqual(c));
    check("T3: sizeof(typeof_unqual(char var)) == 1", sz_c == 1);

    /* T4: typeof_unqual and typeof give same result for plain int */
    int sz_typeof = sizeof(typeof(x));
    int sz_unqual = sizeof(typeof_unqual(x));
    check("T4: sizeof(typeof(x)) == sizeof(typeof_unqual(x))", sz_typeof == sz_unqual);

    if (g_fails == 0) printf("ALL TYPEOF_UNQUAL TESTS PASSED\n");
    else printf("%d TYPEOF_UNQUAL TEST(S) FAILED\n", g_fails);
    return g_fails;
}
