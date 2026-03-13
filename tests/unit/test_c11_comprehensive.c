/* test_c11_comprehensive.c
 * RCC Task 72.31B — Comprehensive C11 feature test.
 *
 * Covers:
 *   A: _Alignas actual address alignment + _Static_assert
 *   B: stdatomic.h — exchange, compare_exchange, fetch_sub/or/and/xor, atomic_flag
 *   C: threads.h — all function declarations compile (no link required)
 *   D: Global string pointer initializer
 *   E: Feature-test macros (__STDC_NO_ATOMICS__ etc.)
 *
 * Expected: RCC exit 0. Compile, assemble, link, run → exit 0.
 */
#include <stdio.h>
#include <stdatomic.h>
#include <threads.h>
#include <stdalign.h>

/* ── A: _Alignas global alignment ────────────────────────────────────────── */
_Alignas(16) int g_aligned16 = 99;

/* ── D: Global string pointer init ───────────────────────────────────────── */
static const char* g_greeting = "Hello, C11!";

/* ── B: Atomic test variable ─────────────────────────────────────────────── */
static atomic_int g_atom;

/* ── C: Thread API types compile (declare variables of each type) ─────────── */
static void verify_thread_api() {
    thrd_t     dummy_thr;
    mtx_t      dummy_mtx;
    cnd_t      dummy_cnd;
    tss_t      dummy_tss;
    once_flag  dummy_once;
    thrd_start_t dummy_fn;
    tss_dtor_t   dummy_dtor;
    (void)dummy_thr;
    (void)dummy_mtx;
    (void)dummy_cnd;
    (void)dummy_tss;
    (void)dummy_once;
    (void)dummy_fn;
    (void)dummy_dtor;
}

/* ── Simple assertion ────────────────────────────────────────────────────── */
static int g_failures = 0;
static void check(const char* name, int cond) {
    if (!cond) {
        printf("FAIL: %s\n", name);
        g_failures++;
    }
}

int main(void) {
    printf("=== C11 Comprehensive Test ===\n");

    /* ── Section A: _Alignas alignment ────────────────────────────────────── */
    /* A1: global _Alignas(16) variable address is 16-byte aligned */
    unsigned long long addr16 = (unsigned long long)&g_aligned16;
    check("A1 global _Alignas(16) address % 16 == 0", (addr16 % 16) == 0);

    /* A2: local _Alignas(8) variable address is 8-byte aligned */
    _Alignas(8) int local8 = 42;
    unsigned long long laddr = (unsigned long long)&local8;
    check("A2 local _Alignas(8) address % 8 == 0", (laddr % 8) == 0);
    check("A2 local8 value preserved", local8 == 42);

    /* A3: _Static_assert with constant conditions (must not abort compilation) */
    _Static_assert(1, "always true");
    _Static_assert(4 == 4, "arithmetic constant expression");
    check("A3 _Static_assert passed compilation", 1);

    /* ── Section B: stdatomic.h ────────────────────────────────────────────── */
    /* B1: atomic_store + atomic_load */
    atomic_store(&g_atom, 0LL);
    check("B1 atomic_store/load round-trip", atomic_load(&g_atom) == 0);

    /* B2: atomic_exchange — returns old value, sets new */
    long long old = atomic_exchange(&g_atom, 42LL);
    check("B2 atomic_exchange old value == 0", old == 0);
    check("B2 atomic_exchange new value == 42", atomic_load(&g_atom) == 42);

    /* B3: atomic_compare_exchange_strong — success path */
    long long expected = 42;
    int ok = atomic_compare_exchange_strong(&g_atom, &expected, 100LL);
    check("B3 CAS success returns 1", ok == 1);
    check("B3 CAS success new value == 100", atomic_load(&g_atom) == 100);

    /* B4: atomic_compare_exchange_strong — failure path */
    expected = 999;  /* wrong — actual is 100 */
    ok = atomic_compare_exchange_strong(&g_atom, &expected, 200LL);
    check("B4 CAS failure returns 0", ok == 0);
    check("B4 CAS failure updates expected", expected == 100);
    check("B4 CAS failure does not change atom", atomic_load(&g_atom) == 100);

    /* B5: atomic_fetch_add */
    atomic_store(&g_atom, 10LL);
    old = atomic_fetch_add(&g_atom, 5LL);
    check("B5 fetch_add old == 10", old == 10);
    check("B5 fetch_add new == 15", atomic_load(&g_atom) == 15);

    /* B6: atomic_fetch_sub */
    old = atomic_fetch_sub(&g_atom, 3LL);
    check("B6 fetch_sub old == 15", old == 15);
    check("B6 fetch_sub new == 12", atomic_load(&g_atom) == 12);

    /* B7: atomic_fetch_or */
    atomic_store(&g_atom, 0x0FLL);
    old = atomic_fetch_or(&g_atom, 0xF0LL);
    check("B7 fetch_or old == 0x0F", old == 0x0F);
    check("B7 fetch_or new == 0xFF", atomic_load(&g_atom) == 0xFF);

    /* B8: atomic_fetch_and */
    old = atomic_fetch_and(&g_atom, 0x0FLL);
    check("B8 fetch_and old == 0xFF", old == 0xFF);
    check("B8 fetch_and new == 0x0F", atomic_load(&g_atom) == 0x0F);

    /* B9: atomic_fetch_xor */
    old = atomic_fetch_xor(&g_atom, 0x06LL);
    check("B9 fetch_xor old == 0x0F", old == 0x0F);
    check("B9 fetch_xor new == 0x09", atomic_load(&g_atom) == 0x09);

    /* B10: atomic_flag test-and-set / clear */
    atomic_flag flag = ATOMIC_FLAG_INIT;
    check("B10 flag TAS first call == 0", atomic_flag_test_and_set(&flag) == 0);
    check("B10 flag TAS second call != 0", atomic_flag_test_and_set(&flag) != 0);
    atomic_flag_clear(&flag);
    check("B10 flag TAS after clear == 0", atomic_flag_test_and_set(&flag) == 0);

    /* B11: atomic_is_lock_free */
    check("B11 atomic_is_lock_free == 1", atomic_is_lock_free(&g_atom) == 1);

    /* ── Section C: threads.h declarations compile ─────────────────────────── */
    verify_thread_api();
    check("C1 threads.h API declarations compile", 1);

    /* ── Section D: Global string pointer ─────────────────────────────────── */
    check("D1 g_greeting != NULL", g_greeting != 0);
    check("D1 g_greeting[0] == 'H'", g_greeting[0] == 'H');
    check("D1 g_greeting[5] == ','", g_greeting[5] == ',');
    check("D1 g_greeting[7] == 'C'", g_greeting[7] == 'C');

    /* ── Section E: Feature-test macros ───────────────────────────────────── */
#if __STDC_NO_ATOMICS__ != 0
    check("E1 __STDC_NO_ATOMICS__ == 0", 0);
#else
    check("E1 __STDC_NO_ATOMICS__ == 0", 1);
#endif

#if __STDC_NO_THREADS__ != 1
    check("E2 __STDC_NO_THREADS__ == 1", 0);
#else
    check("E2 __STDC_NO_THREADS__ == 1", 1);
#endif

#if __STDC_NO_COMPLEX__ != 1
    check("E3 __STDC_NO_COMPLEX__ == 1", 0);
#else
    check("E3 __STDC_NO_COMPLEX__ == 1", 1);
#endif

#if __STDC_VERSION__ < 201112L
    check("E4 __STDC_VERSION__ >= 201112L", 0);
#else
    check("E4 __STDC_VERSION__ >= 201112L", 1);
#endif

    /* ── Summary ────────────────────────────────────────────────────────────── */
    if (g_failures == 0) {
        printf("ALL C11 COMPREHENSIVE CHECKS PASSED\n");
    } else {
        printf("%d C11 COMPREHENSIVE CHECK(S) FAILED\n", g_failures);
    }
    return g_failures;
}
