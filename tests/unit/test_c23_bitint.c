/* test_c23_bitint.c
 * Task 7.44 — C23 §6.7.1: _BitInt(N) type specifier
 *
 * In RCC, _BitInt(N) is a preprocessor expansion to a standard integer type:
 *   N ≤  8  → signed char
 *   N ≤ 16  → short
 *   N ≤ 32  → int
 *   N ≤ 64  → long long
 *
 * This test verifies the expansion produces correctly-sized storage.
 *
 * Expected: compile with exit 0
 */
#include <stdio.h>

static int g_fails = 0;
static void check(const char *name, int cond) {
    if (!cond) { printf("FAIL: %s\n", name); g_fails++; }
}

int main(void) {
    printf("=== _BitInt Test ===\n");

    /* B1: _BitInt(8) → signed char (1 byte) */
    _BitInt(8) b8 = 127;
    check("_BitInt(8) sizeof == 1", (int)sizeof(b8) == 1);
    check("_BitInt(8) value == 127", (int)b8 == 127);

    /* B2: _BitInt(16) → short (2 bytes) */
    _BitInt(16) b16 = 32767;
    check("_BitInt(16) sizeof == 2", (int)sizeof(b16) == 2);
    check("_BitInt(16) value == 32767", (int)b16 == 32767);

    /* B3: _BitInt(32) → int (4 bytes) */
    _BitInt(32) b32 = 1000000;
    check("_BitInt(32) sizeof == 4", (int)sizeof(b32) == 4);
    check("_BitInt(32) value == 1000000", b32 == 1000000);

    /* B4: _BitInt(64) → long long (8 bytes) */
    _BitInt(64) b64 = 9000000000LL;
    check("_BitInt(64) sizeof == 8", (int)sizeof(b64) == 8);
    check("_BitInt(64) value == 9000000000", b64 == 9000000000LL);

    /* B5: Arithmetic on _BitInt types */
    _BitInt(32) x = 100;
    _BitInt(32) y = 200;
    _BitInt(32) z = x + y;
    check("_BitInt arithmetic: 100+200 == 300", z == 300);

    if (g_fails == 0) printf("ALL _BitInt TESTS PASSED\n");
    else printf("%d _BitInt TEST(S) FAILED\n", g_fails);
    return g_fails;
}
