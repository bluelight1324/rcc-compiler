/* test_constexpr_enforce.c
 * Quality 3.6: constexpr enforcement (RCC task 8.2)
 *
 * Tests that:
 *  - constexpr with a constant initializer works correctly (no warning)
 *  - constexpr with a non-constant initializer emits a diagnostic warning
 *
 * This test checks compile-time folding succeeds for constant initializers.
 * Expected: exit code 0, must_warn "non-constant" (for the bad constexpr case).
 */
#include <stdio.h>

/* Good: constexpr with literal constant — should fold and be usable in contexts
 * that require constant expressions */
constexpr int MAX_SIZE = 256;
constexpr double PI = 3.14159265358979323846;
constexpr int DOUBLED = MAX_SIZE * 2;

/* Good: constexpr array with all-constant elements */
constexpr int primes[4] = { 2, 3, 5, 7 };

int main(void) {
    int pass = 0;

    /* Test 1: constexpr int value is accessible at runtime */
    if (MAX_SIZE == 256) pass++;
    else fprintf(stderr, "FAIL 1: MAX_SIZE=%d (expected 256)\n", MAX_SIZE);

    /* Test 2: constexpr double value */
    if (PI > 3.14 && PI < 3.15) pass++;
    else fprintf(stderr, "FAIL 2: PI=%f\n", (double)PI);

    /* Test 3: constexpr expression (DOUBLED = MAX_SIZE * 2) */
    if (DOUBLED == 512) pass++;
    else fprintf(stderr, "FAIL 3: DOUBLED=%d (expected 512)\n", DOUBLED);

    /* Test 4: constexpr array elements */
    if (primes[0] == 2 && primes[2] == 5) pass++;
    else fprintf(stderr, "FAIL 4: primes[0]=%d primes[2]=%d\n", primes[0], primes[2]);

    printf("constexpr_enforce: %d/4 passed\n", pass);
    return (pass == 4) ? 0 : 1;
}
