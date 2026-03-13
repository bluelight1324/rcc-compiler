/* test_c11_phase4.c
 * RCC Task 72.26 — C11 Phase 4 compliance (compile-only test).
 *
 * Tests:
 *   R4:  <stdnoreturn.h>   noreturn macro + __noreturn_is_defined
 *   R11: <complex.h>       __STDC_NO_COMPLEX__ + complex macro (expands to empty)
 *   R5:  <stdlib.h>        aligned_alloc mapped to _aligned_malloc (Windows alias)
 *   R6:  <wchar.h>         wchar_t, wint_t, wcslen declaration
 *   R6:  <wctype.h>        iswalpha, towlower declarations
 *   R7:  <stdatomic.h>     atomic_int, atomic_flag, atomic_store/load
 *   R7:  _Atomic(T)        preprocessor strips qualifier -> plain T
 *   R3:  _Generic          cast expression type dispatch
 *
 * Expected: RCC exit 0 (compile success).
 * No warnings required — all features are no-op or silent.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <complex.h>
#include <wchar.h>
#include <wctype.h>
#include <stdatomic.h>

/* ── R4: noreturn function declaration ─────────────────────────────────────
 * noreturn -> _Noreturn -> empty (preprocessor no-op).
 * Declaration is valid; function body not needed for compile-only test. */
noreturn void fatal(const char* msg);

/* ── R11: complex expands to empty, double complex z; => double z; ─────── */
double complex g_real;

/* ── R5: aligned_alloc macro is visible after #include <stdlib.h> ──────── */
#ifdef aligned_alloc
static int g_have_aligned_alloc = 1;
#else
static int g_have_aligned_alloc = 0;
#endif

/* ── R6: wchar_t global array ──────────────────────────────────────────── */
static wchar_t g_wide[8];

/* ── R7: _Atomic(T) qualifier stripped by preprocessor ─────────────────── */
_Atomic(int) g_atom;

/* ── R7: atomic type aliases and atomic_flag ─────────────────────────────  */
static atomic_int   s_counter;
static atomic_flag  s_flag = ATOMIC_FLAG_INIT;

/* ── R3: _Generic with cast expression ───────────────────────────────────── */
#define CLASSIFY(x) _Generic((x),   \
    int:    "int",                   \
    double: "double",                \
    float:  "float",                 \
    long:   "long",                  \
    char*:  "string",                \
    default: "other")

int main(void) {
    /* R4: conformance marker */
    printf("noreturn_is_defined: %d\n", __noreturn_is_defined);

    /* R11: __STDC_NO_COMPLEX__ must be 1 */
#ifndef __STDC_NO_COMPLEX__
    printf("ERROR: __STDC_NO_COMPLEX__ not defined\n");
    return 1;
#endif
    printf("STDC_NO_COMPLEX: %d\n", __STDC_NO_COMPLEX__);

    /* R11: complex expands to empty; g_real is a plain double */
    g_real = 3.14;
    printf("complex (double): %.2f\n", g_real);

    /* R5: aligned_alloc alias present */
    printf("aligned_alloc aliased: %d\n", g_have_aligned_alloc);

    /* R6: wchar_t usage + wctype classification */
    g_wide[0] = (wchar_t)'H';
    g_wide[1] = (wchar_t)'i';
    g_wide[2] = 0;
    wint_t wc = (wint_t)g_wide[0];
    printf("wchar_t value: %d\n", (int)wc);
    /* iswalpha / towlower are declared — call to verify codegen */
    int alpha = iswalpha(wc);
    wint_t lower = towlower((wint_t)'A');
    printf("iswalpha(%d): %d\n", (int)wc, alpha);
    printf("towlower('A'): %d\n", (int)lower);

    /* R7: _Atomic(T) -> plain int */
    g_atom = 99;
    printf("_Atomic(int): %d\n", g_atom);

    /* R7: atomic operations (non-atomic stubs) */
    atomic_store(&s_counter, 42);
    int val = atomic_load(&s_counter);
    printf("atomic store/load: %d\n", val);

    atomic_flag_test_and_set(&s_flag);
    printf("atomic_flag set: %d\n", s_flag._val);

    /* R7: atomic_fetch_add */
    atomic_store(&s_counter, 10);
    atomic_fetch_add(&s_counter, 5);
    printf("atomic_fetch_add: %d\n", atomic_load(&s_counter));

    /* R3: _Generic with integer literal (existing) */
    printf("_Generic(42): %s\n",   CLASSIFY(42));
    printf("_Generic(3.14): %s\n", CLASSIFY(3.14));
    printf("_Generic(2.0f): %s\n", CLASSIFY(2.0f));
    printf("_Generic(1L): %s\n",   CLASSIFY(1L));

    /* R3: _Generic with cast expression */
    int x = 7;
    printf("_Generic((double)x): %s\n", CLASSIFY((double)x));
    printf("_Generic((float)x): %s\n",  CLASSIFY((float)x));

    return 0;
}
