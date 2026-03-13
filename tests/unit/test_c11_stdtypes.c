/* test_c11_stdtypes.c
 * RCC Task 7.38 — C11 standard type sizes and limits at runtime.
 *
 * Tests:
 *   S1: sizeof fixed-width integer types match expected sizes
 *   S2: integer limit macros have correct values
 *   S3: bool / true / false semantics
 *   S4: intptr_t is pointer-sized
 *
 * Expected: RCC compile + run → exit 0.
 */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

static int g_failures = 0;
static void check(const char* name, int cond) {
    if (!cond) { printf("FAIL: %s\n", name); g_failures++; }
}

int main(void) {
    printf("=== C11 Standard Types Test ===\n");

    /* ── S1: sizeof fixed-width types ────────────────────────────────────── */
    check("sizeof(int8_t)==1",   (int)sizeof(int8_t)  == 1);
    check("sizeof(int16_t)==2",  (int)sizeof(int16_t) == 2);
    check("sizeof(int32_t)==4",  (int)sizeof(int32_t) == 4);
    check("sizeof(int64_t)==8",  (int)sizeof(int64_t) == 8);
    check("sizeof(uint8_t)==1",  (int)sizeof(uint8_t)  == 1);
    check("sizeof(uint16_t)==2", (int)sizeof(uint16_t) == 2);
    check("sizeof(uint32_t)==4", (int)sizeof(uint32_t) == 4);
    check("sizeof(uint64_t)==8", (int)sizeof(uint64_t) == 8);

    /* ── S2: intptr_t is pointer-sized on x64 ────────────────────────────── */
    check("sizeof(intptr_t)==8",  (int)sizeof(intptr_t)  == 8);
    check("sizeof(uintptr_t)==8", (int)sizeof(uintptr_t) == 8);
    check("sizeof(intmax_t)==8",  (int)sizeof(intmax_t)  == 8);

    /* ── S3: integer limits ───────────────────────────────────────────────── */
    check("INT8_MIN==-128",       INT8_MIN  == -128);
    check("INT8_MAX==127",        INT8_MAX  ==  127);
    check("INT16_MIN==-32768",    INT16_MIN == -32768);
    check("INT16_MAX==32767",     INT16_MAX ==  32767);
    check("INT32_MIN==MIN",       INT32_MIN == (-2147483647-1));
    check("INT32_MAX==MAX",       INT32_MAX ==  2147483647);
    check("UINT8_MAX==255",       UINT8_MAX  == 255);
    check("UINT16_MAX==65535",    UINT16_MAX == 65535);
    check("UINT32_MAX==4294967295", (unsigned int)UINT32_MAX == 4294967295U);

    /* ── S4: bool / true / false ─────────────────────────────────────────── */
    check("true==1",   true  == 1);
    check("false==0",  false == 0);
    check("!false",    !false);
    check("!!true",    !!true);
    bool flag = true;
    check("bool flag true",  flag);
    flag = false;
    check("bool flag false", !flag);

    /* ── S5: intptr_t round-trips a pointer ──────────────────────────────── */
    int dummy = 42;
    intptr_t addr = (intptr_t)&dummy;
    int* recovered = (int*)addr;
    check("intptr_t round-trip", *recovered == 42);

    /* ── S6: _Static_assert with sizeof (C11 §6.7.10) ────────────────────── */
    _Static_assert(sizeof(int8_t)  == 1, "int8_t must be 1 byte");
    _Static_assert(sizeof(int32_t) == 4, "int32_t must be 4 bytes");
    _Static_assert(sizeof(int64_t) == 8, "int64_t must be 8 bytes");
    check("_Static_assert sizeof passed", 1);

    if (g_failures == 0) printf("ALL STDTYPES CHECKS PASSED\n");
    else printf("%d STDTYPES CHECK(S) FAILED\n", g_failures);
    return g_failures;
}
