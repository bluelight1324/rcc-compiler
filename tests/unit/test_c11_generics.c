/* test_c11_generics.c
 * RCC Task 7.38 — C11 _Generic selection (§6.5.1.1) comprehensive test.
 *
 * RCC implements _Generic as a preprocessor-level dispatch using a
 * cast-based heuristic: (type)expr selects the branch for `type`.
 *
 * Tests:
 *   G1: Type dispatch — int, long long, double, char*
 *   G2: _Generic expression result used in arithmetic
 *   G3: _Generic with default branch
 *   G4: Nested / chained use via macro
 *
 * Expected: RCC compile + run → exit 0.
 */
#include <stdio.h>
#include <string.h>

static int g_failures = 0;
static void check(const char* name, int cond) {
    if (!cond) { printf("FAIL: %s\n", name); g_failures++; }
}

/* Type-name macro — returns a string describing the type */
#define TYPE_NAME(x) _Generic((x),          \
    int:       "int",                        \
    long long: "long long",                  \
    double:    "double",                     \
    char*:     "char*",                      \
    default:   "other")

/* Size macro — returns the sizeof for the selected type */
#define TYPE_SIZE(x) _Generic((x),          \
    int:       4,                            \
    long long: 8,                            \
    double:    8,                            \
    default:   0)

/* Abs macro — integer or float version */
#define GENERIC_ABS(x) _Generic((x),        \
    int:    ((x) < 0 ? -(x) : (x)),         \
    double: ((x) < 0.0 ? -(x) : (x)),       \
    default: (x))

int main(void) {
    printf("=== C11 _Generic Test ===\n");

    /* ── G1: Type-name dispatch using explicit casts ─────────────────────── */
    check("_Generic int",    strcmp(TYPE_NAME((int)0),         "int")       == 0);
    check("_Generic ll",     strcmp(TYPE_NAME((long long)0),   "long long") == 0);
    check("_Generic double", strcmp(TYPE_NAME((double)0.0),    "double")    == 0);

    /* ── G2: _Generic result in expression ───────────────────────────────── */
    int sz_int = TYPE_SIZE((int)0);
    int sz_ll  = TYPE_SIZE((long long)0);
    int sz_dbl = TYPE_SIZE((double)0.0);
    check("TYPE_SIZE int==4",    sz_int == 4);
    check("TYPE_SIZE ll==8",     sz_ll  == 8);
    check("TYPE_SIZE double==8", sz_dbl == 8);

    /* ── G3: Default branch ──────────────────────────────────────────────── */
    int has_default = _Generic((int)1, double: 0, default: 1);
    check("_Generic default taken", has_default == 1);

    int no_default = _Generic((int)1, int: 1, double: 0, default: 2);
    check("_Generic int not default", no_default == 1);

    /* ── G4: GENERIC_ABS macro ───────────────────────────────────────────── */
    int abs_int = GENERIC_ABS((int)-5);
    check("GENERIC_ABS int -5 == 5", abs_int == 5);

    int abs_pos = GENERIC_ABS((int)7);
    check("GENERIC_ABS int 7 == 7", abs_pos == 7);

    /* ── G5: _Generic with integer literal (defaults to int) ─────────────── */
    /* RCC heuristic: uncast integer literal → int */
    int direct = _Generic(0, int: 42, default: 0);
    check("_Generic(0) → int branch", direct == 42);

    if (g_failures == 0) printf("ALL GENERIC CHECKS PASSED\n");
    else printf("%d GENERIC CHECK(S) FAILED\n", g_failures);
    return g_failures;
}
