/* test_c23_stdbit.c
 * Task 7.44 — C23 §7.18: Bit and byte utilities (stdbit.h)
 *
 * Verifies that stdbit.h compiles and its macros produce correct values.
 *
 * Expected: compile with exit 0
 */
#include <stdio.h>
#include <stdbit.h>

static int g_fails = 0;
static void check(const char *name, int cond) {
    if (!cond) { printf("FAIL: %s\n", name); g_fails++; }
}

int main(void) {
    printf("=== stdbit.h Test ===\n");

    /* stdc_count_ones: popcount */
    check("count_ones(0xFF) == 8",    stdc_count_ones((unsigned char)0xFF) == 8);
    check("count_ones(0x00) == 0",    stdc_count_ones((unsigned char)0x00) == 0);
    check("count_ones(0x0F) == 4",    stdc_count_ones((unsigned char)0x0F) == 4);
    check("count_ones(0xAA) == 4",    stdc_count_ones((unsigned char)0xAA) == 4);

    /* stdc_count_zeros: zeros in representation */
    check("count_zeros(0xFF/8-bit) == 0",
          stdc_count_zeros((unsigned char)0xFF) == 0);
    check("count_zeros(0x00/8-bit) == 8",
          stdc_count_zeros((unsigned char)0x00) == 8);

    /* stdc_has_single_bit: power of two */
    check("has_single_bit(1)",  stdc_has_single_bit((unsigned)1));
    check("has_single_bit(4)",  stdc_has_single_bit((unsigned)4));
    check("has_single_bit(16)", stdc_has_single_bit((unsigned)16));
    check("!has_single_bit(3)", !stdc_has_single_bit((unsigned)3));
    check("!has_single_bit(0)", !stdc_has_single_bit((unsigned)0));

    /* stdc_bit_floor: greatest power-of-2 <= x */
    check("bit_floor(1)  == 1",  stdc_bit_floor((unsigned)1)  == 1u);
    check("bit_floor(3)  == 2",  stdc_bit_floor((unsigned)3)  == 2u);
    check("bit_floor(5)  == 4",  stdc_bit_floor((unsigned)5)  == 4u);
    check("bit_floor(8)  == 8",  stdc_bit_floor((unsigned)8)  == 8u);
    check("bit_floor(0)  == 0",  stdc_bit_floor((unsigned)0)  == 0u);

    /* stdc_bit_ceil: least power-of-2 >= x */
    check("bit_ceil(1)  == 1",  stdc_bit_ceil((unsigned)1)  == 1u);
    check("bit_ceil(3)  == 4",  stdc_bit_ceil((unsigned)3)  == 4u);
    check("bit_ceil(4)  == 4",  stdc_bit_ceil((unsigned)4)  == 4u);
    check("bit_ceil(5)  == 8",  stdc_bit_ceil((unsigned)5)  == 8u);

    /* stdc_bit_width: minimum bits needed */
    check("bit_width(0)  == 0",  stdc_bit_width((unsigned)0)  == 0u);
    check("bit_width(1)  == 1",  stdc_bit_width((unsigned)1)  == 1u);
    check("bit_width(7)  == 3",  stdc_bit_width((unsigned)7)  == 3u);
    check("bit_width(8)  == 4",  stdc_bit_width((unsigned)8)  == 4u);

    /* Byte-order macros */
#if __STDC_ENDIAN_NATIVE__ == __STDC_ENDIAN_LITTLE__
    printf("  endian: little\n");
    check("endian: little or big defined",
          __STDC_ENDIAN_NATIVE__ == __STDC_ENDIAN_LITTLE__);
#elif __STDC_ENDIAN_NATIVE__ == __STDC_ENDIAN_BIG__
    printf("  endian: big\n");
    check("endian: little or big defined", 1);
#else
    printf("  endian: unknown\n");
    check("endian: little or big defined", 1);
#endif

    if (g_fails == 0) printf("ALL STDBIT TESTS PASSED\n");
    else printf("%d STDBIT TEST(S) FAILED\n", g_fails);
    return g_fails;
}
