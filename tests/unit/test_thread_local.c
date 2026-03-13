/* test_thread_local.c
 * C11 §6.7.1 — _Thread_local storage class specifier.
 *
 * Tests that _Thread_local variables:
 *   1. Compile without warnings (no "not supported" message)
 *   2. Are emitted to the _TLS segment in assembly output
 *   3. Are accessible and modifiable from the main (single) thread
 *
 * Expected: RCC exit 0 (compile success), correct output.
 * Note: Per-thread isolation is not tested here (requires <threads.h> runtime).
 *       For single-threaded programs, _TLS segment access by label name is correct.
 */
#include <stdio.h>

/* Global _Thread_local variable */
_Thread_local int g_counter = 0;
_Thread_local long g_sum = 0;

/* Non-TLS global to compare */
int g_regular = 100;

int main(void) {
    /* Access and modify _Thread_local variables */
    g_counter = 42;
    g_sum = 1000L;

    printf("counter=%d sum=%ld regular=%d\n", g_counter, g_sum, g_regular);

    if (g_counter != 42) {
        printf("FAIL: g_counter expected 42, got %d\n", g_counter);
        return 1;
    }
    if (g_sum != 1000) {
        printf("FAIL: g_sum expected 1000, got %ld\n", g_sum);
        return 1;
    }

    /* Arithmetic on TLS variable */
    g_counter += 8;
    if (g_counter != 50) {
        printf("FAIL: g_counter+8 expected 50, got %d\n", g_counter);
        return 1;
    }

    printf("_Thread_local test PASS\n");
    return 0;
}
