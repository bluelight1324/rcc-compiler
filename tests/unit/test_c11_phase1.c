/* test_c11_phase1.c
 * RCC Task 72.22 — C11 Phase 1 compliance (compile-only test).
 *
 * Tests the following can be compiled without errors:
 *   G1: _Alignof(type) / alignof(type) via stdalign.h
 *   G2: __STDC_NO_ATOMICS__ and __STDC_NO_THREADS__ predefined macros
 *   G4: quick_exit / at_quick_exit / strtoll / strtoull declarations
 *   G5: char16_t / char32_t types from uchar.h
 *
 * Expected: RCC exit 0 (no errors).
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdalign.h>
#include <uchar.h>

/* G1: _Alignof evaluates at preprocessor level to an integer constant.
 * Use the result as a global initialiser — forces constant evaluation. */
int align_char       = _Alignof(char);
int align_short      = _Alignof(short);
int align_int        = _Alignof(int);
int align_long       = _Alignof(long);
int align_float      = _Alignof(float);
int align_longlong   = _Alignof(long long);
int align_double     = _Alignof(double);

/* G1: alignof macro from stdalign.h */
int align_int2       = alignof(int);
int align_double2    = alignof(double);

/* G2: __STDC_NO_ATOMICS__ and __STDC_NO_THREADS__ must be defined
 * when _Atomic / <threads.h> are not supported.  Use #error to fail
 * the compilation if they are missing. */
#ifndef __STDC_NO_ATOMICS__
#error "C11 requires __STDC_NO_ATOMICS__==1 when _Atomic is not supported"
#endif
#ifndef __STDC_NO_THREADS__
#error "C11 requires __STDC_NO_THREADS__==1 when <threads.h> is not supported"
#endif

/* G5: char16_t / char32_t as global variables */
char16_t global_c16 = 65;       /* 'A' in UTF-16 */
char32_t global_c32 = 128512;   /* U+1F600 GRINNING FACE (fits in 32 bits) */

int main(void) {
    /* G1: _Alignof inside a function (expression context) */
    int a = _Alignof(int);
    int b = alignof(double);
    printf("alignof(int)=%d alignof(double)=%d\n", a, b);

    /* G5: char16_t / char32_t as locals */
    char16_t c16 = 0x0041;    /* U+0041 LATIN CAPITAL LETTER A */
    char32_t c32 = 0x10FFFF;  /* U+10FFFF highest Unicode code point */
    printf("c16=%d c32=%u\n", (int)c16, (unsigned int)c32);

    /* G4: strtoll / strtoull (declared in stdlib.h) */
    long long  ll  = strtoll("9223372036854775807", 0, 10);
    unsigned long long ull = strtoull("18446744073709551615", 0, 10);
    printf("strtoll=%lld strtoull=%llu\n", ll, ull);

    /* G4: at_quick_exit — register NULL handler (no-op); test declaration. */
    at_quick_exit(0);

    return 0;
}
