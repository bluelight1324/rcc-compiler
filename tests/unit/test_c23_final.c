/* test_c23_final.c — Task 7.581: Final C23 compliance gaps (GAP-1 through GAP-6)
 *  GAP-1: memccpy from string.h
 *  GAP-2: char8_t maps to unsigned char
 *  GAP-3: evalConstExpr checks constexpr_float_values_ (float constexpr as int)
 *  GAP-4: typeof(int*) / typeof(struct S) / typeof(long long) type passthrough
 *  GAP-5: Delimited escape sequences \x{N}, \o{N}, \u{N}, \U{N}
 *  GAP-6: constexpr array folding (arr[0] as constant expression)
 */
#include <stdio.h>
#include <string.h>
#include <stddef.h>

static int passed = 0;
static int failed = 0;

#define CHECK(cond, msg) \
    do { if (cond) { passed++; } else { failed++; printf("FAIL: %s\n", msg); } } while(0)

/* ── GAP-1: memccpy ────────────────────────────────────────────────────────── */
static void test_memccpy(void) {
    char src[] = "hello world";
    char dst[32] = {0};
    /* Copy until space character */
    void* ret = memccpy(dst, src, ' ', sizeof(src));
    /* After memccpy, dst should contain "hello " (includes the space) */
    CHECK(dst[0] == 'h', "memccpy dst[0]=='h'");
    CHECK(dst[4] == 'o', "memccpy dst[4]=='o'");
    CHECK(dst[5] == ' ', "memccpy dst[5]==' '");
    /* Return pointer should be one byte after the space in dst */
    CHECK(ret != NULL, "memccpy returns non-NULL when c found");
    /* When c not found in n bytes, returns NULL */
    char tiny[3] = {0};
    void* ret2 = memccpy(tiny, "abcdef", 'z', 3);
    CHECK(ret2 == NULL, "memccpy returns NULL when c not found");
}

/* ── GAP-2: char8_t is unsigned char ────────────────────────────────────────── */
static void test_char8_t(void) {
    /* char8_t should be unsigned char — same size as unsigned char */
    CHECK(sizeof(char8_t) == 1, "sizeof(char8_t) == 1");
    /* Verify it can hold high values (0x80–0xFF) without sign-extension issues */
    char8_t hi = (char8_t)200;
    int v = (int)hi;
    CHECK(v == 200, "char8_t 200 stays 200 (unsigned, no sign-extension)");
    /* u8 character literal type */
    char8_t a = u8'A';
    CHECK(a == 65, "u8'A' == 65");
}

/* ── GAP-3: constexpr float in integer context ───────────────────────────────── */
static void test_constexpr_float_as_int(void) {
    constexpr double PI = 3.14159;
    /* Use PI in an integer context — should truncate to 3 */
    constexpr int N = (int)PI;
    CHECK(N == 3, "constexpr int N = (int)PI == 3");
    /* Arithmetic using float constexpr as int */
    constexpr double E = 2.71828;
    constexpr int M = (int)E;
    CHECK(M == 2, "constexpr int M = (int)E == 2");
}

/* ── GAP-4: typeof with type keywords and pointer types ─────────────────────── */
static void test_typeof_type_keywords(void) {
    /* typeof(int) — type keyword passthrough */
    typeof(int) x = 42;
    CHECK(x == 42, "typeof(int) x = 42");
    CHECK(sizeof(x) == 4, "sizeof(typeof(int)) == 4");

    /* typeof(double) — float keyword passthrough */
    typeof(double) d = 3.14;
    CHECK(d > 3.1 && d < 3.2, "typeof(double) d = 3.14");

    /* typeof(int*) — pointer type passthrough */
    int val = 99;
    typeof(int*) p = &val;
    CHECK(*p == 99, "typeof(int*) p = &val; *p == 99");
    CHECK(sizeof(p) == 8, "sizeof(typeof(int*)) == 8 (pointer on x64)");

    /* typeof(unsigned char) — two-word type */
    typeof(unsigned char) uc = 200;
    CHECK(uc == 200, "typeof(unsigned char) uc = 200");
}

/* ── GAP-5: Delimited escape sequences ─────────────────────────────────────── */
static void test_delimited_escapes(void) {
    /* \x{N} — arbitrary hex value */
    char a = '\x{41}';   /* 'A' = 0x41 */
    CHECK(a == 'A', "\\x{41} == 'A'");

    char b = '\x{7A}';   /* 'z' = 0x7A */
    CHECK(b == 'z', "\\x{7A} == 'z'");

    /* \o{N} — octal value */
    char c = '\o{101}';  /* 'A' = 0101 octal */
    CHECK(c == 'A', "\\o{101} == 'A'");

    char d = '\o{60}';   /* '0' = 060 octal */
    CHECK(d == '0', "\\o{60} == '0'");

    /* \u{N} — Unicode scalar (ASCII range) */
    char e = '\u{41}';   /* 'A' */
    CHECK(e == 'A', "\\u{41} == 'A'");

    /* \U{N} — Unicode scalar (extended form) */
    char f = '\U{00007A}'; /* 'z' */
    CHECK(f == 'z', "\\U{00007A} == 'z'");
}

/* ── GAP-6: constexpr array folding ─────────────────────────────────────────── */
static void test_constexpr_array(void) {
    constexpr int primes[5] = {2, 3, 5, 7, 11};

    /* Array elements should be usable as constant expressions */
    int a = primes[0];
    int b = primes[4];
    CHECK(a == 2,  "constexpr int primes[0] == 2");
    CHECK(b == 11, "constexpr int primes[4] == 11");

    /* Use in _Static_assert (requires element to be compile-time constant) */
    _Static_assert(primes[0] == 2, "primes[0] must be 2");
    _Static_assert(primes[2] == 5, "primes[2] must be 5");
    CHECK(1, "_Static_assert(primes[0]==2) passed");
    CHECK(1, "_Static_assert(primes[2]==5) passed");

    /* Arithmetic on array elements */
    int sum = primes[0] + primes[1] + primes[2];
    CHECK(sum == 10, "primes[0]+primes[1]+primes[2] == 10");
}

int main(void) {
    test_memccpy();
    test_char8_t();
    test_constexpr_float_as_int();
    test_typeof_type_keywords();
    test_delimited_escapes();
    test_constexpr_array();

    printf("C23 final: %d passed, %d failed\n", passed, failed);
    return failed ? 1 : 0;
}
