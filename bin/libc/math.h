#ifndef _RCC_MATH_H
#define _RCC_MATH_H

/* ── Constants ──────────────────────────────────────────────────────────── */
#define M_E        2.71828182845904523536
#define M_LOG2E    1.44269504088896340736
#define M_LOG10E   0.43429448190325182765
#define M_LN2      0.69314718055994530941
#define M_LN10     2.30258509299404568402
#define M_PI       3.14159265358979323846
#define M_PI_2     1.57079632679489661923
#define M_PI_4     0.78539816339744830962
#define M_1_PI     0.31830988618379067154
#define M_2_PI     0.63661977236758134308
#define M_2_SQRTPI 1.12837916709551257390
#define M_SQRT2    1.41421356237309504880
#define M_SQRT1_2  0.70710678118654752440

/* C99: special floating-point values */
#define HUGE_VAL   (1.0/0.0)
#define HUGE_VALF  (1.0f/0.0f)
#define HUGE_VALL  (1.0/0.0)
#define INFINITY   (1.0f/0.0f)
#define NAN        (0.0f/0.0f)

/* C99: math error handling */
#define MATH_ERRNO      1
#define MATH_ERREXCEPT  2
#define math_errhandling (MATH_ERRNO | MATH_ERREXCEPT)

/* C99: FP classification return values */
#define FP_INFINITE  1
#define FP_NAN       2
#define FP_NORMAL    4
#define FP_SUBNORMAL 8
#define FP_ZERO      16

/* ── C99 classification macros ─────────────────────────────────────────── */
/* RCC: implemented as always-false stubs (no FPU status query) */
#define isnan(x)      ((x) != (x))
#define isinf(x)      (((x) == INFINITY) || ((x) == -INFINITY))
#define isfinite(x)   (!isnan(x) && !isinf(x))
#define isnormal(x)   ((x) != 0.0 && isfinite(x))
#define signbit(x)    ((x) < 0.0)
#define fpclassify(x) (isnan(x) ? FP_NAN : isinf(x) ? FP_INFINITE : \
                       (x) == 0.0 ? FP_ZERO : isnormal(x) ? FP_NORMAL : FP_SUBNORMAL)

/* ── double variants ────────────────────────────────────────────────────── */
double sin(double x);
double cos(double x);
double tan(double x);
double asin(double x);
double acos(double x);
double atan(double x);
double atan2(double y, double x);
double sinh(double x);
double cosh(double x);
double tanh(double x);
double asinh(double x);
double acosh(double x);
double atanh(double x);
double sqrt(double x);
double cbrt(double x);
double hypot(double x, double y);
double pow(double base, double exp);
double exp(double x);
double exp2(double x);
double expm1(double x);
double log(double x);
double log2(double x);
double log10(double x);
double log1p(double x);
double logb(double x);
double ilogb(double x);
double ceil(double x);
double floor(double x);
double round(double x);
double trunc(double x);
double rint(double x);
double nearbyint(double x);
double fabs(double x);
double fmod(double x, double y);
double remainder(double x, double y);
double fmin(double x, double y);
double fmax(double x, double y);
double fdim(double x, double y);
double fma(double x, double y, double z);
double frexp(double x, int* exp);
double ldexp(double x, int exp);
double modf(double x, double* iptr);
double scalbn(double x, int n);
double scalbln(double x, long n);
int    fpclassify_d(double x);

/* C99: long-integer rounding */
long      lround(double x);
long long llround(double x);
long      lrint(double x);
long long llrint(double x);

/* ── float variants ─────────────────────────────────────────────────────── */
float sinf(float x);
float cosf(float x);
float tanf(float x);
float asinf(float x);
float acosf(float x);
float atanf(float x);
float atan2f(float y, float x);
float sinhf(float x);
float coshf(float x);
float tanhf(float x);
float asinhf(float x);
float acoshf(float x);
float atanhf(float x);
float sqrtf(float x);
float cbrtf(float x);
float hypotf(float x, float y);
float powf(float base, float exp);
float expf(float x);
float exp2f(float x);
float expm1f(float x);
float logf(float x);
float log2f(float x);
float log10f(float x);
float log1pf(float x);
float logbf(float x);
float ilogbf(float x);
float ceilf(float x);
float floorf(float x);
float roundf(float x);
float truncf(float x);
float rintf(float x);
float nearbyintf(float x);
float fabsf(float x);
float fmodf(float x, float y);
float remainderf(float x, float y);
float fminf(float x, float y);
float fmaxf(float x, float y);
float fdimf(float x, float y);
float fmaf(float x, float y, float z);
float frexpf(float x, int* exp);
float ldexpf(float x, int exp);
float modff(float x, float* iptr);
float scalbnf(float x, int n);
float scalblnf(float x, long n);
long      lroundf(float x);
long long llroundf(float x);
long      lrintf(float x);
long long llrintf(float x);

/* ── integer ────────────────────────────────────────────────────────────── */
int abs(int n);
long labs(long n);
long long llabs(long long n);

/* ── C23 §7.12.15: nearest-integer functions ────────────────────────────── */

/* FP_INT_* rounding direction constants (C23 §7.12.15.1) */
#define FP_INT_UPWARD             0
#define FP_INT_DOWNWARD           1
#define FP_INT_TOWARDZERO         2
#define FP_INT_TONEARESTFROMZERO  3
#define FP_INT_TONEAREST          4

/* C23 §7.12.15.5: roundeven — round to nearest, ties to even (banker's rounding).
 * Not available in MSVCRT; provided as inline. */
static __inline double roundeven(double x) {
    double fl = floor(x);
    double diff = x - fl;
    if (diff > 0.5) return fl + 1.0;
    if (diff < 0.5) return fl;
    /* exact tie: round to the nearest even integer */
    long long i = (long long)fl;
    return (i % 2 == 0) ? fl : fl + 1.0;
}
static __inline float roundevenf(float x) {
    float fl = floorf(x);
    float diff = x - fl;
    if (diff > 0.5f) return fl + 1.0f;
    if (diff < 0.5f) return fl;
    long long i = (long long)fl;
    return (i % 2 == 0) ? fl : fl + 1.0f;
}

/* C23 §7.12.15.1-2: fromfp/ufromfp — convert to integer with explicit rounding.
 * Uses ceil/floor/trunc/round/rint to honour the FP_INT_* rounding direction. */
static __inline double _rcc_applyRnd(double x, int rnd) {
    switch (rnd) {
        case FP_INT_UPWARD:            return ceil(x);
        case FP_INT_DOWNWARD:          return floor(x);
        case FP_INT_TONEARESTFROMZERO: return round(x);
        case FP_INT_TONEAREST:         return rint(x);
        default: /* FP_INT_TOWARDZERO */ return trunc(x);
    }
}
static __inline long fromfp(double x, int rnd, unsigned width) {
    (void)width; return (long)_rcc_applyRnd(x, rnd);
}
static __inline long fromfpf(float x, int rnd, unsigned width) {
    (void)width; return (long)_rcc_applyRnd((double)x, rnd);
}
static __inline long fromfpl(long double x, int rnd, unsigned width) {
    (void)width; return (long)_rcc_applyRnd((double)x, rnd);
}
static __inline unsigned long ufromfp(double x, int rnd, unsigned width) {
    (void)width; return (unsigned long)_rcc_applyRnd(x, rnd);
}
static __inline unsigned long ufromfpf(float x, int rnd, unsigned width) {
    (void)width; return (unsigned long)_rcc_applyRnd((double)x, rnd);
}
static __inline unsigned long ufromfpl(long double x, int rnd, unsigned width) {
    (void)width; return (unsigned long)_rcc_applyRnd((double)x, rnd);
}

/* C23 §7.12.15.5: roundevenl — long double variant of roundeven. */
static __inline long double roundevenl(long double x) {
    return (long double)roundeven((double)x);
}

/* ── C99 §7.12.11.3: nextafter/nextafterf — next representable value ─────── */
double nextafter(double x, double y);
float  nextafterf(float x, float y);

/* ── C23 §7.12.15.6: nextup / nextdown ─────────────────────────────────── */
/* nextup(x): smallest floating-point number greater than x. */
static __inline double nextup(double x) {
    return nextafter(x,  (1.0/0.0));  /* INFINITY */
}
static __inline float  nextupf(float x) {
    return nextafterf(x, (1.0f/0.0f));
}
static __inline long double nextupl(long double x) {
    return (long double)nextafter((double)x, (1.0/0.0));
}
/* nextdown(x): largest floating-point number less than x. */
static __inline double nextdown(double x) {
    return nextafter(x, -(1.0/0.0));  /* -INFINITY */
}
static __inline float  nextdownf(float x) {
    return nextafterf(x, -(1.0f/0.0f));
}
static __inline long double nextdownl(long double x) {
    return (long double)nextafter((double)x, -(1.0/0.0));
}

#endif
