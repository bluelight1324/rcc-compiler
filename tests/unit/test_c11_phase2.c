/* test_c11_phase2.c
 * RCC Task 72.24 — C11 Phase 2 compliance (compile-only test).
 *
 * Tests:
 *   G6: u"..." / U"..." / L"..." wide string literals
 *   G7: _Alignas(n) — parse without error; warning for n > 16
 *   G8: _Generic type dispatch heuristic
 *
 * Expected: RCC exit 0 (compile success).
 * G7 with n=32 emits a warning containing "cannot be enforced" to stderr.
 */
#include <stdio.h>
#include <uchar.h>   /* char16_t, char32_t */
#include <stdalign.h> /* _Alignas, alignas */

/* G7: _Alignas at global scope — natural alignment, no warning */
_Alignas(8) int g_aligned8 = 42;

/* G8: _Generic macros — selecting branch by controlling expression type */
#define TYPE_OF_ZERO _Generic(0,    int: "int",    long: "long",  default: "other")
#define TYPE_OF_DBL  _Generic(0.0,  double: "dbl", float: "flt",  default: "other")
#define TYPE_OF_FLT  _Generic(0.0f, float: "flt",  double: "dbl", default: "other")
#define TYPE_OF_LL   _Generic(0LL,  long long: "ll", int: "int",  default: "other")

int main(void) {
    /* G6: u"..." UTF-16LE string literal (char16_t[]) */
    const char16_t* s16 = u"Hello UTF-16";
    (void)s16;

    /* G6: U"..." UTF-32LE string literal (char32_t[]) */
    const char32_t* s32 = U"Hello UTF-32";
    (void)s32;

    /* G6: L"..." wide string literal (wchar_t[]) */
    const char16_t* sw = L"Wide string";
    (void)sw;

    /* G6: u8"..." byte string — already supported */
    const char* s8 = u8"UTF-8 string";
    (void)s8;

    /* G7: _Alignas inside a function body — no warning for n <= 8 */
    _Alignas(4) int local4 = 1;
    (void)local4;

    /* G7: _Alignas(32) — rbp limit is 16, so >16 emits "cannot be enforced" */
    _Alignas(32) int local32 = 2;
    (void)local32;

    /* G7: alignas() macro from stdalign.h */
    alignas(8) double d_aligned = 3.14;
    (void)d_aligned;

    /* G8: _Generic type selection — verify strings compile */
    const char* t0 = TYPE_OF_ZERO;
    const char* t1 = TYPE_OF_DBL;
    const char* t2 = TYPE_OF_FLT;
    const char* t3 = TYPE_OF_LL;
    printf("Generic: %s %s %s %s\n", t0, t1, t2, t3);

    return 0;
}
