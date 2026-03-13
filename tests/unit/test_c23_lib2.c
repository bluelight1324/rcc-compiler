/* test_c23_lib2.c
 * Task 8.41: C23 library gaps (round 2) — RCC v5.13.0
 *
 * Tests that all new C23 library additions compile cleanly:
 *   - setjmp.h   : jmp_buf type, setjmp macro, longjmp
 *   - signal.h   : SIGABRT/SIGFPE/SIGILL/SIGINT/SIGSEGV/SIGTERM constants,
 *                  SIG_DFL/SIG_IGN/SIG_ERR, sig_atomic_t, signal(), raise()
 *   - stdlib.h   : free_sized / free_aligned_sized
 *   - math.h     : nextafter/nextafterf, nextup/nextdown family,
 *                  roundevenl, fromfpl/ufromfpl
 *   - time.h     : timespec_getres
 *
 * Expected: exit code 0, no compile errors.
 */
#include <stdio.h>
#include <setjmp.h>
#include <signal.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

/* Global for signal test — sig_atomic_t is a typedef for int */
static sig_atomic_t g_sig_received = 0;

static void my_sighandler(int sig) {
    g_sig_received = sig;
}

int main(void) {
    int pass = 0;

    /* ── setjmp.h (C23 §7.13) ───────────────────────────────────────── */
    jmp_buf env;
    int jv = setjmp(env);
    if (jv == 0) {
        pass++;
    }

    /* ── signal.h constants (C23 §7.14) ────────────────────────────── */
    if (SIGABRT == 22 && SIGFPE == 8 && SIGILL == 4) pass++;
    if (SIGINT == 2 && SIGSEGV == 11 && SIGTERM == 15) pass++;

    /* ── signal.h: sig_atomic_t is an integer type ─────────────────── */
    sig_atomic_t sa = 7;
    if (sa == 7) pass++;

    /* ── signal.h: install a handler, raise, verify it ran ─────────── */
    signal(SIGTERM, my_sighandler);
    raise(SIGTERM);
    if (g_sig_received == SIGTERM) pass++;
    signal(SIGTERM, SIG_DFL);

    /* ── stdlib.h: free_sized / free_aligned_sized (C23 §7.24.3.3) ─── */
    void *p = malloc(64);
    if (p != 0) {
        free_sized(p, 64);
        pass++;
    }
    void *pa = _aligned_malloc(128, 16);
    if (pa != 0) {
        free_aligned_sized(pa, 16, 128);
        pass++;
    }

    /* ── math.h: nextafter/nextafterf (C99 §7.12.11.3) ─────────────── */
    double nx = nextafter(1.0, 2.0);
    if (nx > 1.0) pass++;
    float nxf = nextafterf(1.0f, 2.0f);
    if (nxf > 1.0f) pass++;

    /* ── math.h: nextup / nextdown (C23 §7.12.15.6) ────────────────── */
    double nu = nextup(1.0);
    double nd = nextdown(1.0);
    if (nu > 1.0 && nd < 1.0) pass++;

    float nuf = nextupf(1.0f);
    float ndf = nextdownf(1.0f);
    if (nuf > 1.0f && ndf < 1.0f) pass++;

    /* ── math.h: roundevenl (C23 §7.12.15.5) — use double cast ─────── */
    double rel = roundevenl(2.5);
    double rel2 = roundevenl(3.5);
    if (rel == 2.0 && rel2 == 4.0) pass++;

    /* ── math.h: fromfpl / ufromfpl (C23 §7.12.15.1-2) ────────────── */
    long fv = fromfpl(3.7, FP_INT_TOWARDZERO, 32);
    if (fv == 3) pass++;
    unsigned long ufv = ufromfpl(5.9, FP_INT_UPWARD, 32);
    if (ufv == 5) pass++;

    /* ── time.h: timespec_getres (C23 §7.27.3) ─────────────────────── */
    struct timespec tres;
    int r = timespec_getres(&tres, TIME_UTC);
    if (r == TIME_UTC && tres.tv_nsec > 0) pass++;

    printf("c23_lib2: %d/15 passed\n", pass);
    return (pass == 15) ? 0 : 1;
}
