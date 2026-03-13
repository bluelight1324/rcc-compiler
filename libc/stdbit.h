/* stdbit.h — C23 §7.18 Bit and Byte Utilities
 *
 * Provides type-generic bit-counting and bit-manipulation functions.
 * All functions operate on UNSIGNED integer types.
 *
 * Implementation note: RCC does not expose compiler intrinsics, so these
 * are implemented as static inline C functions using portable arithmetic.
 * They are wrapped in a macro layer that casts the argument to the correct
 * width (unsigned char, unsigned short, unsigned int, unsigned long long).
 */

#ifndef _STDBIT_H
#define _STDBIT_H

#ifndef _RCC_SIZE_T
#define _RCC_SIZE_T
typedef unsigned long long size_t;
#endif

/* Byte order macros (C23 §7.18.1) */
#define __STDC_ENDIAN_LITTLE__ 1234
#define __STDC_ENDIAN_BIG__    4321
#define __STDC_ENDIAN_NATIVE__ __STDC_ENDIAN_LITTLE__   /* x86-64 is little-endian */

/* ── Helper static-inline core implementations ───────────────────────── */

static unsigned __rcc_clz64(unsigned long long x) {
    if (!x) return 64;
    unsigned n = 0;
    if (!(x >> 32)) { n += 32; x <<= 32; }
    if (!(x >> 48)) { n += 16; x <<= 16; }
    if (!(x >> 56)) { n +=  8; x <<= 8;  }
    if (!(x >> 60)) { n +=  4; x <<= 4;  }
    if (!(x >> 62)) { n +=  2; x <<= 2;  }
    if (!(x >> 63)) { n +=  1; }
    return n;
}

static unsigned __rcc_ctz64(unsigned long long x) {
    if (!x) return 64;
    unsigned n = 0;
    if (!(x & 0xFFFFFFFFULL)) { n += 32; x >>= 32; }
    if (!(x & 0x0000FFFFULL)) { n += 16; x >>= 16; }
    if (!(x & 0x000000FFULL)) { n +=  8; x >>=  8; }
    if (!(x & 0x0000000FULL)) { n +=  4; x >>=  4; }
    if (!(x & 0x00000003ULL)) { n +=  2; x >>=  2; }
    if (!(x & 0x00000001ULL)) { n +=  1; }
    return n;
}

static unsigned __rcc_popcount64(unsigned long long x) {
    x = x - ((x >> 1) & 0x5555555555555555ULL);
    x = (x & 0x3333333333333333ULL) + ((x >> 2) & 0x3333333333333333ULL);
    x = (x + (x >> 4)) & 0x0f0f0f0f0f0f0f0fULL;
    return (unsigned)((x * 0x0101010101010101ULL) >> 56);
}

static unsigned long long __rcc_bit_floor64(unsigned long long x) {
    if (!x) return 0;
    return 1ULL << (63 - __rcc_clz64(x));
}

static unsigned long long __rcc_bit_ceil64(unsigned long long x) {
    if (x <= 1) return 1;
    return 1ULL << (64 - __rcc_clz64(x - 1));
}

/* ── Type-generic macros — cast to ull then delegate ─────────────────── */

/* C23 §7.18.2: Leading zeros / ones */
#define stdc_leading_zeros(x)    __rcc_clz64((unsigned long long)(x))
#define stdc_leading_zeros_uc(x) ((unsigned)(__rcc_clz64((unsigned long long)(unsigned char)(x)) - 56))
#define stdc_leading_zeros_us(x) ((unsigned)(__rcc_clz64((unsigned long long)(unsigned short)(x)) - 48))
#define stdc_leading_zeros_ui(x) ((unsigned)(__rcc_clz64((unsigned long long)(unsigned int)(x)) - 32))
#define stdc_leading_zeros_ul(x) ((unsigned)(__rcc_clz64((unsigned long long)(unsigned long)(x))))
#define stdc_leading_zeros_ull(x) __rcc_clz64((unsigned long long)(x))

#define stdc_leading_ones(x)     stdc_leading_zeros(~(unsigned long long)(x))

/* C23 §7.18.3: Trailing zeros / ones */
#define stdc_trailing_zeros(x)   __rcc_ctz64((unsigned long long)(x))
#define stdc_trailing_ones(x)    __rcc_ctz64(~(unsigned long long)(x))

/* C23 §7.18.4: First leading / trailing zero / one (1-based; 0 if not found) */
#define stdc_first_leading_zero(x)  (((unsigned long long)(x) == (unsigned long long)-1) ? 0 : stdc_leading_zeros(~(unsigned long long)(x)) + 1)
#define stdc_first_leading_one(x)   ((x) ? stdc_leading_zeros((unsigned long long)(x)) + 1 : 0)
#define stdc_first_trailing_zero(x) (((unsigned long long)(x) == (unsigned long long)-1) ? 0 : stdc_trailing_zeros(~(unsigned long long)(x)) + 1)
#define stdc_first_trailing_one(x)  ((x) ? stdc_trailing_zeros((unsigned long long)(x)) + 1 : 0)

/* C23 §7.18.5: Count zeros / ones */
#define stdc_count_zeros(x)   ((unsigned)(sizeof(unsigned long long)*8 - __rcc_popcount64((unsigned long long)(x))))
#define stdc_count_ones(x)    __rcc_popcount64((unsigned long long)(x))

/* C23 §7.18.6: Single bit predicates */
#define stdc_has_single_bit(x) ((x) && !((unsigned long long)(x) & ((unsigned long long)(x) - 1)))

/* C23 §7.18.7: Bit floor, ceiling, width */
#define stdc_bit_floor(x)  __rcc_bit_floor64((unsigned long long)(x))
#define stdc_bit_ceil(x)   __rcc_bit_ceil64((unsigned long long)(x))
#define stdc_bit_width(x)  ((unsigned)(64 - __rcc_clz64((unsigned long long)(x))))

#endif /* _STDBIT_H */
