/* test_c11_inttypes.c
 * RCC Task 7.38 — C11 <inttypes.h> format macro runtime test.
 *
 * Tests:
 *   I1: PRId8/16/32/64 produce correct printf output
 *   I2: PRIu32/64 unsigned decimal
 *   I3: PRIx32/64 hex lowercase
 *   I4: imaxabs() from §7.8.2.1
 *
 * Expected: RCC compile + run → exit 0.
 */
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

static int g_failures = 0;
static void check(const char* name, int cond) {
    if (!cond) { printf("FAIL: %s\n", name); g_failures++; }
}

int main(void) {
    printf("=== C11 inttypes.h Test ===\n");

    char buf[64];

    /* ── I1: PRId — signed decimal ───────────────────────────────────────── */
    int32_t x32 = 42;
    sprintf(buf, "%" PRId32, x32);
    check("PRId32 '42'", strcmp(buf, "42") == 0);

    int64_t x64neg = -12345LL;
    sprintf(buf, "%" PRId64, x64neg);
    check("PRId64 '-12345'", strcmp(buf, "-12345") == 0);

    int64_t x64pos = 9876543210LL;
    sprintf(buf, "%" PRId64, x64pos);
    check("PRId64 big positive", strcmp(buf, "9876543210") == 0);

    /* ── I2: PRIu — unsigned decimal ─────────────────────────────────────── */
    uint32_t u32 = 4000000000U;
    sprintf(buf, "%" PRIu32, u32);
    check("PRIu32 '4000000000'", strcmp(buf, "4000000000") == 0);

    uint64_t u64 = 1000000000000LL;
    sprintf(buf, "%" PRIu64, u64);
    check("PRIu64 '1000000000000'", strcmp(buf, "1000000000000") == 0);

    /* ── I3: PRIx — hex lowercase ────────────────────────────────────────── */
    uint32_t h32 = 0xDEAD;
    sprintf(buf, "%" PRIx32, h32);
    check("PRIx32 'dead'", strcmp(buf, "dead") == 0);

    uint64_t h64 = 0xCAFEBABELL;
    sprintf(buf, "%" PRIx64, h64);
    check("PRIx64 'cafebabe'", strcmp(buf, "cafebabe") == 0);

    /* ── I4: PRIu8 / PRId8 ───────────────────────────────────────────────── */
    uint8_t u8 = 255;
    sprintf(buf, "%" PRIu8, u8);
    check("PRIu8 '255'", strcmp(buf, "255") == 0);

    /* ── I5: imaxabs (C11 §7.8.2.1) ─────────────────────────────────────── */
    long long abs_val = imaxabs(-9999LL);
    check("imaxabs(-9999)==9999", abs_val == 9999LL);
    long long abs_pos = imaxabs(12345LL);
    check("imaxabs(12345)==12345", abs_pos == 12345LL);

    if (g_failures == 0) printf("ALL INTTYPES CHECKS PASSED\n");
    else printf("%d INTTYPES CHECK(S) FAILED\n", g_failures);
    return g_failures;
}
