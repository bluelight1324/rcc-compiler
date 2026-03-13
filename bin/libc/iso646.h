/* iso646.h — C95/C99/C11/C23 §7.9 Alternative spellings for RCC
 *
 * Provides keyword-like macros for operators that use characters
 * not universally available in all character sets.
 *
 * C95 amendment 1 (ISO/IEC 9899:1990/AMD 1:1995) §7.9:
 * All names in this header are macros expanding to the corresponding
 * punctuation tokens.
 */
#ifndef _RCC_ISO646_H
#define _RCC_ISO646_H

#define and     &&
#define and_eq  &=
#define bitand  &
#define bitor   |
#define compl   ~
#define not     !
#define not_eq  !=
#define or      ||
#define or_eq   |=
#define xor     ^
#define xor_eq  ^=

#endif /* _RCC_ISO646_H */
