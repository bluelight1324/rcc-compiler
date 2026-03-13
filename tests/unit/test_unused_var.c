/* test_unused_var.c - W6: Unused-variable warning + [[maybe_unused]]
 * RCC should emit a warning for `unused_val` but NOT for `suppressed`.
 * Compiler still exits 0 (warnings don't abort compilation).
 */
#include <stdio.h>

int main() {
    int used = 42;
    int unused_val = 99;        /* W6: expect warning: unused variable 'unused_val' */
    [[maybe_unused]] int suppressed = 7;  /* W6: no warning (suppressed) */

    printf("%d\n", used);
    return 0;
}
