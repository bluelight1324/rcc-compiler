/* test_c17_version.c
 * RCC Task 7.41 — C17 compliance version macro test.
 *
 * Verifies that:
 *   V1: __STDC_VERSION__ == 201710L by default (C17)
 *   V2: __STDC_VERSION__ >= 201710L  (C17 or later)
 *   V3: __STDC_VERSION__ >= 201112L  (C11 or later — backwards compat)
 *   V4: -std=c11 produces 201112L
 *   V5: -std=c23 produces 202311L
 *
 * V1-V3 are tested in this file (compiled without -std= flag, so default = c17).
 * V4-V5 are tested via separate compilations in run_tests.ps1 using -std= flag.
 *
 * Expected: compile + run → exit 0.
 */
#include <stdio.h>

static int g_failures = 0;
static void check(const char* name, int cond) {
    if (!cond) { printf("FAIL: %s\n", name); g_failures++; }
}

int main(void) {
    printf("=== C17 Version Test ===\n");
    printf("__STDC_VERSION__ = %ldL\n", (long)__STDC_VERSION__);

    /* V1: Default mode is C17 */
    check("V1: __STDC_VERSION__ == 201710L",
          __STDC_VERSION__ == 201710L);

    /* V2: At least C17 */
    check("V2: __STDC_VERSION__ >= 201710L",
          __STDC_VERSION__ >= 201710L);

    /* V3: Backwards compat — at least C11 */
    check("V3: __STDC_VERSION__ >= 201112L",
          __STDC_VERSION__ >= 201112L);

    /* V4: Compile-time check: C17 is strictly newer than C11 */
#if __STDC_VERSION__ >= 201710L
    check("V4: #if __STDC_VERSION__ >= 201710L works", 1);
#else
    check("V4: #if __STDC_VERSION__ >= 201710L works", 0);
#endif

    /* V5: C17 header guard pattern (common in real code) */
#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201710L)
    check("V5: C17 header guard pattern", 1);
#else
    check("V5: C17 header guard pattern", 0);
#endif

    if (g_failures == 0) printf("ALL C17 VERSION CHECKS PASSED\n");
    else printf("%d C17 VERSION CHECK(S) FAILED\n", g_failures);
    return g_failures;
}
