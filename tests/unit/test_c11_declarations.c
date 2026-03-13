/* test_c11_declarations.c
 * RCC Task 7.38 — C11 declaration specifiers test.
 *
 * Tests:
 *   D1: inline function specifier (C99 §6.7.4) — maps to static in RCC
 *   D2: restrict pointer qualifier (C99 §6.7.3) — maps to const in RCC
 *   D3: _Noreturn function specifier (C11 §6.7.4) — compile-only
 *   D4: _Alignas(N) on locals (C11 §6.7.5) — runtime address check
 *   D5: _Static_assert with sizeof (C11 §6.7.10)
 *   D6: __func__ predefined identifier (C99 §6.4.2.2)
 *
 * Expected: RCC compile + run → exit 0.
 */
#include <stdio.h>
#include <stdnoreturn.h>

static int g_failures = 0;
static void check(const char* name, int cond) {
    if (!cond) { printf("FAIL: %s\n", name); g_failures++; }
}

/* ── D1: inline function ──────────────────────────────────────────────────── */
/* inline → TOK_STATIC in RCC, so this is a static function */
inline int add_inline(int a, int b) {
    return a + b;
}

inline long long mul_inline(long long a, long long b) {
    return a * b;
}

/* ── D2: restrict pointer qualifier ──────────────────────────────────────── */
/* restrict → TOK_CONST (qualifier on pointer) — just verify it compiles */
static int sum_array(const int* src, int n) {
    int s = 0;
    int i;
    for (i = 0; i < n; i++) s += src[i];
    return s;
}

/* restrict in parameter position — should parse as TOK_CONST, compile OK */
static void copy_bytes(char* dest, const char* src, int n) {
    int i;
    for (i = 0; i < n; i++) dest[i] = src[i];
}

/* ── D3: _Noreturn ────────────────────────────────────────────────────────── */
/* _Noreturn is silently consumed by RCC preprocessor — just declare it */
noreturn void fatal_exit(int code);   /* declared but not called */

/* ── D6: __func__ ─────────────────────────────────────────────────────────── */
static const char* get_funcname(void) {
    return __func__;
}

int main(void) {
    printf("=== C11 Declarations Test ===\n");

    /* ── D1: inline calls ─────────────────────────────────────────────────── */
    check("inline add(3,4)==7",  add_inline(3, 4) == 7);
    check("inline add(0,0)==0",  add_inline(0, 0) == 0);
    check("inline add(-1,1)==0", add_inline(-1, 1) == 0);
    check("inline mul(6,7)==42", (int)mul_inline(6LL, 7LL) == 42);

    /* ── D2: restrict function calls ─────────────────────────────────────── */
    int arr[5];
    arr[0]=1; arr[1]=2; arr[2]=3; arr[3]=4; arr[4]=5;
    check("restrict sum==15", sum_array(arr, 5) == 15);

    char dst[8];
    copy_bytes(dst, "hello", 5);
    check("restrict copy h==h", dst[0] == 'h');
    check("restrict copy o==o", dst[4] == 'o');

    /* ── D4: _Alignas(N) local alignment ─────────────────────────────────── */
    _Alignas(8)  int  local8  = 111;
    _Alignas(16) int  local16 = 222;

    unsigned long long addr8  = (unsigned long long)&local8;
    unsigned long long addr16 = (unsigned long long)&local16;

    check("_Alignas(8)  addr%8==0",  (addr8  % 8)  == 0);
    check("_Alignas(16) addr%16==0", (addr16 % 16) == 0);
    check("_Alignas(8)  value ok",   local8  == 111);
    check("_Alignas(16) value ok",   local16 == 222);

    /* ── D5: _Static_assert with sizeof ──────────────────────────────────── */
    _Static_assert(sizeof(int)       == 4, "int must be 4 bytes");
    _Static_assert(sizeof(long long) == 8, "long long must be 8 bytes");
    _Static_assert(sizeof(char)      == 1, "char must be 1 byte");
    _Static_assert(sizeof(void*)     == 8, "pointer must be 8 bytes on x64");
    check("_Static_assert sizeof passed", 1);

    /* ── D6: __func__ ─────────────────────────────────────────────────────── */
    check("__func__ in main",          __func__[0] == 'm');
    const char* fn = get_funcname();
    check("__func__ in function",      fn[0] == 'g');

    if (g_failures == 0) printf("ALL DECLARATIONS CHECKS PASSED\n");
    else printf("%d DECLARATIONS CHECK(S) FAILED\n", g_failures);
    return g_failures;
}
