/* test_c23_noreturn_warn.c
 * Task 7.44 — C23 §6.7.12.1: [[noreturn]] attribute return-path warning
 *
 * A function marked [[noreturn]] that has a path reaching the closing brace
 * should trigger a "may return" warning from RCC.
 *
 * Expected:
 *   compile exit 0  (it is a warning, not an error)
 *   stderr contains "may return"
 */
#include <stdio.h>

/* This function is [[noreturn]] but its body does not call exit/abort —
 * RCC should warn "[[noreturn]] function 'bad_noreturn' may return". */
[[noreturn]] void bad_noreturn(void) {
    int x = 1;
    (void)x;
    /* falls off the end — triggers the warning */
}

int main(void) {
    printf("noreturn_warn: compile-only test\n");
    return 0;
}
