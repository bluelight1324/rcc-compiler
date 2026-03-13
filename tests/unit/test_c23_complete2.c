/* test_c23_complete2.c — Task 7.583: Final C23 compliance (GAP-A through GAP-E)
 *  GAP-A: __STDC_IEC_60559_BFP__ IEEE 754 conformance macro
 *  GAP-B: #embed parameters (limit, prefix, suffix, if_empty) — tested separately via -E
 *  GAP-C: Delimited escape sequences in string literals ("\x{N}", "\o{N}", "\u{N}")
 *  GAP-D: constexpr struct member folding (_Static_assert(p.x == 1))
 *  GAP-E: typeof_unqual strips leading qualifiers (const, volatile, restrict)
 */
#include <stdio.h>
#include <string.h>

static int passed = 0;
static int failed = 0;

#define CHECK(cond, msg) \
    do { if (cond) { passed++; } else { failed++; printf("FAIL: %s\n", msg); } } while(0)

/* ── GAP-A: __STDC_IEC_60559_BFP__ ─────────────────────────────────────────── */
static void test_iec60559(void) {
    /* C23 §6.10.9.2: must be defined to 1 when binary FP is IEEE 754 compliant */
#ifdef __STDC_IEC_60559_BFP__
    CHECK(__STDC_IEC_60559_BFP__ == 1, "__STDC_IEC_60559_BFP__ == 1");
    CHECK(1, "__STDC_IEC_60559_BFP__ is defined");
#else
    failed++; printf("FAIL: __STDC_IEC_60559_BFP__ not defined\n");
#endif
}

/* ── GAP-C: Delimited escapes in string literals ────────────────────────────── */
static void test_string_delimited_escapes(void) {
    /* \x{HHH} in a string literal */
    const char s1[] = "\x{48}\x{65}\x{6C}\x{6C}\x{6F}";  /* "Hello" */
    CHECK(s1[0] == 'H', "string \\x{48} == 'H'");
    CHECK(s1[4] == 'o', "string \\x{6C}\\x{6F} == 'o'");
    CHECK(strlen(s1) == 5, "string delimited hex: length 5");

    /* \o{OOO} in a string literal */
    const char s2[] = "\o{110}\o{145}\o{154}\o{154}\o{157}"; /* "Hello" octal */
    CHECK(s2[0] == 'H', "string \\o{110} == 'H'");
    CHECK(s2[4] == 'o', "string \\o{157} == 'o'");

    /* \u{N} in a string literal (ASCII range) */
    const char s3[] = "\u{41}\u{42}\u{43}";  /* "ABC" */
    CHECK(s3[0] == 'A', "string \\u{41} == 'A'");
    CHECK(s3[1] == 'B', "string \\u{42} == 'B'");
    CHECK(s3[2] == 'C', "string \\u{43} == 'C'");

    /* Mix with regular escapes */
    const char s4[] = "\x{41}\n\x{42}";
    CHECK(s4[0] == 'A', "mixed: \\x{41} first char");
    CHECK(s4[1] == '\n', "mixed: \\n second char");
    CHECK(s4[2] == 'B', "mixed: \\x{42} third char");
}

/* ── GAP-D: constexpr struct member folding ──────────────────────────────────── */
struct Vec2 { int x; int y; };
struct RGB  { int r; int g; int b; };

static void test_constexpr_struct(void) {
    constexpr struct Vec2 origin = {0, 0};
    constexpr struct Vec2 unit   = {1, 2};
    constexpr struct RGB  red    = {255, 0, 0};

    /* Runtime access — basic correctness */
    CHECK(origin.x == 0 && origin.y == 0, "constexpr Vec2 origin == {0,0}");
    CHECK(unit.x == 1 && unit.y == 2,     "constexpr Vec2 unit == {1,2}");
    CHECK(red.r == 255 && red.g == 0,     "constexpr RGB red == {255,0,0}");

    /* Compile-time access via _Static_assert */
    _Static_assert(unit.x == 1,    "unit.x must be 1");
    _Static_assert(unit.y == 2,    "unit.y must be 2");
    _Static_assert(red.r  == 255,  "red.r must be 255");
    _Static_assert(red.g  == 0,    "red.g must be 0");
    CHECK(1, "_Static_assert(unit.x == 1) passed");
    CHECK(1, "_Static_assert(red.r == 255) passed");

    /* Use in constexpr expression */
    constexpr int sum_xy = unit.x + unit.y;   /* 1 + 2 = 3 */
    CHECK(sum_xy == 3, "constexpr int sum_xy = unit.x + unit.y == 3");
}

/* ── GAP-E: typeof_unqual qualifier stripping ────────────────────────────────── */
static void test_typeof_unqual(void) {
    /* typeof_unqual(const int) → int (qualifier stripped) */
    typeof_unqual(const int) x = 42;
    x = 43;   /* would fail to compile if type were const int */
    CHECK(x == 43, "typeof_unqual(const int) is mutable int");
    CHECK(sizeof(x) == 4, "sizeof(typeof_unqual(const int)) == 4");

    /* typeof_unqual(volatile double) → double */
    typeof_unqual(volatile double) d = 3.14;
    d = 2.71;
    CHECK(d > 2.7 && d < 2.72, "typeof_unqual(volatile double) is mutable double");

    /* typeof_unqual on a plain type (no qualifiers) → same type */
    typeof_unqual(int) y = 99;
    CHECK(y == 99, "typeof_unqual(int) == int");

    /* typeof still preserves qualifiers (contrast) */
    double val = 1.5;
    typeof(val) z = val * 2.0;
    CHECK(z > 2.9 && z < 3.1, "typeof(val) preserves type");
}

int main(void) {
    test_iec60559();
    test_string_delimited_escapes();
    test_constexpr_struct();
    test_typeof_unqual();

    printf("C23 complete2: %d passed, %d failed\n", passed, failed);
    return failed ? 1 : 0;
}
