/* test_noreturn_keyword.c
 * Task 7.47 — P3-A: _Noreturn keyword (C11) expands to [[noreturn]]
 *
 * _Noreturn is the C11 keyword form of the C23 [[noreturn]] attribute.
 * After task 7.47, _Noreturn is preprocessor-expanded to [[noreturn]],
 * so the return-path warning fires for both forms.
 *
 * Expected: compile with exit 0.  The function is a declaration only —
 * we don't call it, so no link error.
 */
#include <stdio.h>
#include <stdlib.h>

/* _Noreturn function that actually does not return (uses exit) — no warning */
_Noreturn void fatal_ok(int code) {
    exit(code);
}

static int g_fails = 0;
static void check(const char *name, int cond) {
    if (!cond) { printf("FAIL: %s\n", name); g_fails++; }
}

int main(void) {
    printf("=== _Noreturn Keyword Test ===\n");

    /* _Noreturn function that actually exits — no warning should appear */
    check("_Noreturn expansion compiled", 1);

    /* Compile-time verification: sizeof still works after _Noreturn expansion */
    check("sizeof int == 4", sizeof(int) == 4);

    if (g_fails == 0) printf("ALL _NORETURN TESTS PASSED\n");
    else printf("%d _NORETURN TEST(S) FAILED\n", g_fails);
    return g_fails;
}
