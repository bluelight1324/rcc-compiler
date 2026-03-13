/* complex.h — C11 §7.3 complex arithmetic stub for RCC
 *
 * RCC does not implement complex floating-point arithmetic.
 * __STDC_NO_COMPLEX__ is predefined to 1 in the RCC preprocessor so user
 * code can test for complex support and provide fallbacks.
 *
 * The 'complex' macro expands to nothing so declarations like
 *   double complex z;
 * parse correctly as plain 'double' declarations.
 *
 * The constant I is defined as 0.0f as a compile-time placeholder.
 * Do NOT use complex arithmetic at runtime — results will be wrong.
 *
 * Platform: Windows x64
 */
#ifndef _RCC_COMPLEX_H
#define _RCC_COMPLEX_H

/* Signal that complex arithmetic is not supported (C11 §6.10.9) */
#ifndef __STDC_NO_COMPLEX__
#define __STDC_NO_COMPLEX__ 1
#endif

/* C11 §7.3.1: Type macros.
 * 'complex' expands to empty so 'double complex z;' compiles as 'double z;'.
 * '_Complex' is not a recognized keyword in RCC; strip it. */
#define complex         /* stub: _Complex not supported by RCC */
#define _Complex_I      0.0f
#define imaginary       /* stub: _Imaginary not supported by RCC */
#define I               _Complex_I

#endif /* _RCC_COMPLEX_H */
