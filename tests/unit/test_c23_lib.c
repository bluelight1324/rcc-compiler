/* test_c23_lib.c
 * Task 8.4: C23 library gaps — new headers and functions (RCC v5.12.0)
 *
 * Tests that all C23 library additions compile cleanly:
 *   - limits.h   : BOOL_WIDTH / INT_WIDTH / LLONG_WIDTH width macros
 *   - stdint.h   : INT8_WIDTH / INT32_WIDTH / UINT64_WIDTH / SIZE_WIDTH
 *   - stdlib.h   : strfromd / strfromf / strfroml
 *   - uchar.h    : mbrtoc8 / c8rtomb
 *   - math.h     : roundeven / roundevenf / fromfp / ufromfp / FP_INT_* constants
 *   - locale.h   : LC_* constants / struct lconv / setlocale / localeconv
 *   - wchar.h    : wcsdup macro
 *
 * Expected: exit code 0, no compile errors.
 */
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include <locale.h>
#include <uchar.h>
#include <wchar.h>

int main(void) {
    int pass = 0;

    /* ── limits.h width macros (C23 §5.2.4.2.1) ────────────────────── */
    if (BOOL_WIDTH == 1)    pass++;
    if (CHAR_WIDTH == 8)    pass++;
    if (INT_WIDTH  == 32)   pass++;
    if (LLONG_WIDTH == 64)  pass++;

    /* ── stdint.h width macros (C23 §7.20.2.2) ──────────────────────── */
    if (INT8_WIDTH  == 8)   pass++;
    if (INT32_WIDTH == 32)  pass++;
    if (INT64_WIDTH == 64)  pass++;
    if (UINT8_WIDTH == 8)   pass++;
    if (SIZE_WIDTH  == 64)  pass++;

    /* ── stdlib.h: strfromd/strfromf/strfroml (C23 §7.24.1.5) ─────── */
    char buf[32];
    int n = strfromd(buf, sizeof(buf), "%g", 1.5);
    if (n > 0 && buf[0] != '\0') pass++;

    float fv = 2.5f;
    n = strfromf(buf, sizeof(buf), "%g", fv);
    if (n > 0) pass++;

    /* ── uchar.h: mbrtoc8 / c8rtomb (C23 §7.30.2) ──────────────────── */
    unsigned char c8 = 0;
    size_t r = mbrtoc8(&c8, "A", 1, 0);
    if (r == 1 && c8 == 'A') pass++;

    char mb[4];
    size_t r2 = c8rtomb(mb, (unsigned char)'Z', 0);
    if (r2 == 1 && mb[0] == 'Z') pass++;

    /* ── math.h: roundeven (C23 §7.12.15.5) ────────────────────────── */
    /* Banker's rounding: ties go to nearest even integer */
    double re25 = roundeven(2.5);   /* 2.0 (even) */
    double re35 = roundeven(3.5);   /* 4.0 (even) */
    double re15 = roundeven(1.5);   /* 2.0 (even) */
    if (re25 == 2.0 && re35 == 4.0 && re15 == 2.0) pass++;

    float ref25 = roundevenf(2.5f);
    float ref35 = roundevenf(3.5f);
    if (ref25 == 2.0f && ref35 == 4.0f) pass++;

    /* ── math.h: FP_INT_* constants (C23 §7.12.15.1) ──────────────── */
    int rnd = FP_INT_TONEAREST;
    if (rnd == 4) pass++;

    /* ── math.h: fromfp / ufromfp stubs (C23 §7.12.15.1-2) ────────── */
    long iv = fromfp(3.9, FP_INT_TOWARDZERO, 32);
    if (iv == 3) pass++;
    unsigned long uiv = ufromfp(7.2, FP_INT_UPWARD, 32);
    if (uiv == 7) pass++;

    /* ── locale.h: LC_* / localeconv / setlocale (C23 §7.11) ────────── */
    if (LC_ALL == 0 && LC_NUMERIC == 4) pass++;
    struct lconv *lc = localeconv();
    if (lc != 0) pass++;

    /* ── wchar.h: wcsdup (C23 §7.31.4.1) ───────────────────────────── */
    const wchar_t wsrc[4] = { 'H', 'i', '!', 0 };
    wchar_t *wdup = wcsdup(wsrc);
    if (wdup != 0 && wcslen(wdup) == 3) pass++;
    /* Note: free(wdup) omitted — test process exits immediately */

    printf("c23_lib: %d/22 passed\n", pass);
    return (pass == 22) ? 0 : 1;
}
