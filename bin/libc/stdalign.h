/* stdalign.h — C11/C23 alignment support for RCC
 *
 * _Alignof(type) is implemented in the RCC preprocessor and returns the
 * correct natural alignment for all scalar types on Windows x64.
 *
 * _Alignas(n) is parsed as a no-op (the alignment annotation is accepted
 * without generating alignment-enforcement code yet — deferred to a later
 * version).
 *
 * Platform: Windows x64 (MSVC ABI)
 *   char / signed char / unsigned char   → 1 byte
 *   short / unsigned short               → 2 bytes
 *   int / unsigned int / long / float    → 4 bytes
 *   long long / double / pointers        → 8 bytes
 */
#ifndef _RCC_STDALIGN_H
#define _RCC_STDALIGN_H

/* C11 §7.15: Convenience macros for the alignment operators.
 * 'alignas' and 'alignof' are the C23 keywords; in C11 they are macros. */
#ifndef alignas
#define alignas _Alignas
#endif
#ifndef alignof
#define alignof _Alignof
#endif

/* __alignas_is_defined / __alignof_is_defined — conformance markers */
#define __alignas_is_defined 1
#define __alignof_is_defined 1

#endif /* _RCC_STDALIGN_H */
