/* test_noreturn_deprecated.c
 * Optional 3.5: _Noreturn deprecation warning (RCC task 8.2)
 *
 * When compiled with -std=c23, RCC should warn that _Noreturn is deprecated.
 * Without -std=c23 it should compile silently (C11 behavior preserved).
 *
 * Expected: exit code 0, and (when -std=c23) must_warn "deprecated".
 * The run_tests.ps1 entry uses must_warn to verify the warning is emitted.
 */
#include <stdio.h>
#include <stdlib.h>

/* C11: _Noreturn maps to [[noreturn]] in RCC.
 * C23: additionally warns that _Noreturn is deprecated. */
_Noreturn void fatal_error(const char *msg) {
    fprintf(stderr, "fatal: %s\n", msg);
    exit(1);
}

/* Normal [[noreturn]] — should have no warning */
[[noreturn]] void fatal2(const char *msg) {
    fprintf(stderr, "fatal2: %s\n", msg);
    exit(1);
}

int main(void) {
    printf("noreturn_deprecated: ok\n");
    return 0;
    (void)fatal_error; /* suppress unused warning */
    (void)fatal2;
}
