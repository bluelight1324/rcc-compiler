/* test_c11_fenv.c
 * RCC Task 7.38 — C11 <fenv.h> compile test (§7.6).
 *
 * Verifies that all types, macros, and function declarations from
 * <fenv.h> are parseable by RCC. Does NOT call fenv functions at
 * runtime (they require UCRT link support — documented limitation).
 *
 * Expected: RCC compile + run → exit 0.
 */
#include <stdio.h>
#include <fenv.h>

/* Verify all types compile */
static void check_fenv_types(void) {
    fenv_t    env;      /* floating-point environment */
    fexcept_t flags;    /* exception flags */
    (void)env;
    (void)flags;
}

/* Verify all exception macros are defined */
static void check_fenv_macros(void) {
    int exc = FE_INVALID | FE_DIVBYZERO | FE_OVERFLOW | FE_UNDERFLOW | FE_INEXACT;
    int all = FE_ALL_EXCEPT;
    int rnd_near  = FE_TONEAREST;
    int rnd_down  = FE_DOWNWARD;
    int rnd_up    = FE_UPWARD;
    int rnd_zero  = FE_TOWARDZERO;
    (void)exc; (void)all;
    (void)rnd_near; (void)rnd_down; (void)rnd_up; (void)rnd_zero;
}

/* Verify all function declarations are visible (just take their address) */
static void check_fenv_decls(void) {
    /* Exception functions */
    void* p1 = (void*)feclearexcept;
    void* p2 = (void*)feraiseexcept;
    void* p3 = (void*)fetestexcept;
    void* p4 = (void*)fegetexceptflag;
    void* p5 = (void*)fesetexceptflag;
    /* Rounding functions */
    void* p6 = (void*)fegetround;
    void* p7 = (void*)fesetround;
    /* Environment functions */
    void* p8  = (void*)fegetenv;
    void* p9  = (void*)feholdexcept;
    void* p10 = (void*)fesetenv;
    void* p11 = (void*)feupdateenv;
    (void)p1;(void)p2;(void)p3;(void)p4;(void)p5;
    (void)p6;(void)p7;(void)p8;(void)p9;(void)p10;(void)p11;
}

int main(void) {
    printf("=== C11 fenv.h Compile Test ===\n");
    check_fenv_types();
    check_fenv_macros();
    check_fenv_decls();
    printf("ALL FENV DECLARATIONS COMPILE OK\n");
    return 0;
}
