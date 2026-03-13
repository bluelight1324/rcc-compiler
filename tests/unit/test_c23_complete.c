/* test_c23_complete.c — Task 7.57: 100% C23 compliance verification
 * Tests items added/fixed in task 7.57:
 *  1. __STDC_EMBED_FOUND__ / __STDC_EMBED_NOT_FOUND__ / __STDC_EMBED_EMPTY__ macros
 *  2. u8'a' UTF-8 character literals (char8_t)
 *  3. typeof(VAR) in declaration context
 *  4. constexpr float / double with cross-use
 *  5. memalignment() from stdlib.h
 *  6. timegm() / _mkgmtime alias from time.h
 */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static int passed = 0;
static int failed = 0;

#define CHECK(cond, msg) \
    do { if (cond) { passed++; } else { failed++; printf("FAIL: %s\n", msg); } } while(0)

/* ── Test 1: __STDC_EMBED_* macros ─────────────────────────────────────── */
static void test_embed_macros(void) {
    /* C23 §6.10.4 — these three macros must be defined */
    int nf = __STDC_EMBED_NOT_FOUND__;
    int fd = __STDC_EMBED_FOUND__;
    int em = __STDC_EMBED_EMPTY__;
    CHECK(nf == 0, "STDC_EMBED_NOT_FOUND__ == 0");
    CHECK(fd == 1, "STDC_EMBED_FOUND__ == 1");
    CHECK(em == 2, "STDC_EMBED_EMPTY__ == 2");
    /* The ordered relationship must hold */
    CHECK(nf < fd && fd < em, "embed macros ordered 0<1<2");
}

/* ── Test 2: u8'a' character literals ───────────────────────────────────── */
static void test_u8_char_literal(void) {
    char8_t c1 = u8'A';
    char8_t c2 = u8'z';
    char8_t c3 = u8'0';
    CHECK(c1 == 65,  "u8'A' == 65");
    CHECK(c2 == 122, "u8'z' == 122");
    CHECK(c3 == 48,  "u8'0' == 48");
    /* Must equal the plain char literal value */
    CHECK(c1 == 'A', "u8'A' == 'A'");
}

/* ── Test 3: typeof in declaration context ──────────────────────────────── */
static void test_typeof_declaration(void) {
    double x = 3.14;
    typeof(x) y = 2.71;          /* typeof(x) → double */
    typeof(3.14) pi = 3.14159;   /* typeof(3.14) → double */
    int n = 42;
    typeof(n) m = n * 2;         /* typeof(n) → int */
    float f = 1.5f;
    typeof(f) g = f + 0.5f;      /* typeof(f) → float */

    CHECK(y > 2.7 && y < 2.72, "typeof(double_var) declaration");
    CHECK(pi > 3.14 && pi < 3.15, "typeof(float_literal) declaration");
    CHECK(m == 84, "typeof(int_var) declaration");
    CHECK(g > 1.9f && g < 2.1f, "typeof(float_var) declaration");
}

/* ── Test 4: constexpr float / double ────────────────────────────────────── */
/* C23 §6.7.1 — constexpr objects with float type */
static void test_constexpr_float(void) {
    constexpr double PI    = 3.14159265358979;
    constexpr double TWO   = 2.0;
    constexpr double TAU   = PI * TWO;    /* cross-constexpr arithmetic */
    constexpr float  HALF  = 0.5f;
    constexpr int    THREE = 3;           /* integer constexpr still works */

    CHECK(PI > 3.14 && PI < 3.15,           "constexpr double PI");
    CHECK(TAU > 6.28 && TAU < 6.29,         "constexpr double TAU = PI * 2");
    CHECK(HALF > 0.49f && HALF < 0.51f,     "constexpr float HALF");
    CHECK(THREE == 3,                        "constexpr int THREE");

    /* Use in arithmetic */
    double r = PI * (double)THREE;
    CHECK(r > 9.42 && r < 9.43, "constexpr float used in expression");
}

/* ── Test 5: memalignment() ──────────────────────────────────────────────── */
static void test_memalignment(void) {
    /* C23 §7.24.3.6 */
    void* p8  = malloc(8);
    /* Freshly allocated memory alignment must be >= 8 on x64 */
    unsigned long long a = memalignment(p8);
    CHECK(a >= 8, "memalignment(malloc(8)) >= 8");
    /* Null pointer must return 0 */
    CHECK(memalignment(NULL) == 0, "memalignment(NULL) == 0");
    free(p8);
}

/* ── Test 6: timegm() / _mkgmtime ───────────────────────────────────────── */
static void test_timegm(void) {
    /* C23 §7.27.2.3 — convert UTC struct tm to time_t */
    struct tm t;
    t.tm_year  = 70;  /* 1970 */
    t.tm_mon   = 0;   /* January */
    t.tm_mday  = 1;   /* 1st */
    t.tm_hour  = 0;
    t.tm_min   = 0;
    t.tm_sec   = 0;
    t.tm_isdst = 0;
    time_t epoch = timegm(&t);
    CHECK(epoch == 0, "timegm(1970-01-01 00:00:00 UTC) == 0");

    /* One day later */
    t.tm_mday = 2;
    time_t next = timegm(&t);
    CHECK(next == 86400, "timegm(1970-01-02 00:00:00 UTC) == 86400");
}

int main(void) {
    test_embed_macros();
    test_u8_char_literal();
    test_typeof_declaration();
    test_constexpr_float();
    test_memalignment();
    test_timegm();

    printf("C23 complete: %d passed, %d failed\n", passed, failed);
    return failed ? 1 : 0;
}
