/* test_has_c_attribute.c
 * Task 7.43 — B2: __has_c_attribute predicate (C23 §6.10.9)
 *
 * Verifies that __has_c_attribute(X) returns a nonzero date constant for
 * standard C23 attributes supported by RCC, and 0 for unknown attributes.
 *
 * Expected: compile → exit 0 (all preprocessor #if checks pass)
 */
#include <stdio.h>

int main(void) {
    int fails = 0;

    /* Known attributes must return nonzero */
#if !__has_c_attribute(nodiscard)
    printf("FAIL: __has_c_attribute(nodiscard) should be nonzero\n"); fails++;
#endif

#if !__has_c_attribute(deprecated)
    printf("FAIL: __has_c_attribute(deprecated) should be nonzero\n"); fails++;
#endif

#if !__has_c_attribute(fallthrough)
    printf("FAIL: __has_c_attribute(fallthrough) should be nonzero\n"); fails++;
#endif

#if !__has_c_attribute(maybe_unused)
    printf("FAIL: __has_c_attribute(maybe_unused) should be nonzero\n"); fails++;
#endif

#if !__has_c_attribute(noreturn)
    printf("FAIL: __has_c_attribute(noreturn) should be nonzero\n"); fails++;
#endif

    /* Unknown attribute must return 0 */
#if __has_c_attribute(xyzzy_totally_unknown_attr)
    printf("FAIL: __has_c_attribute(xyzzy_totally_unknown_attr) should be 0\n"); fails++;
#endif

    /* Standard usage pattern: guard code on attribute support */
#if __has_c_attribute(nodiscard) >= 202003L
    /* nodiscard was introduced in C23 — this branch should be taken */
#else
    printf("FAIL: __has_c_attribute(nodiscard) should be >= 202003L\n"); fails++;
#endif

    if (fails == 0) printf("ALL __has_c_attribute TESTS PASSED\n");
    else printf("%d __has_c_attribute TEST(S) FAILED\n", fails);
    return fails;
}
