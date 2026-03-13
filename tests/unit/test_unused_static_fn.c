/* test_unused_static_fn.c — 4.2: Unused static function warning
 * RCC should emit a warning "defined but not used" for static functions
 * that are never called.
 */

/* This static function is never called — should trigger warning */
static int helper_never_called(int x) {
    return x * 2;
}

int main(void) {
    return 0;
}
