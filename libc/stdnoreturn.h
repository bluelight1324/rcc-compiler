/* stdnoreturn.h — C11 §7.23 _Noreturn convenience macro for RCC
 *
 * C11 §7.23: Provides the 'noreturn' macro as a spelling of _Noreturn.
 * RCC accepts _Noreturn on function declarations and discards it (no-op).
 * The specifier has no effect on code generation but allows C11-conforming
 * code to declare non-returning functions portably.
 *
 * Platform: Windows x64
 */
#ifndef _RCC_STDNORETURN_H
#define _RCC_STDNORETURN_H

/* C11 §7.23.1: noreturn convenience macro.
 * 'noreturn' expands to '_Noreturn' which the RCC preprocessor then
 * silently consumes (no-op annotation). */
#ifndef noreturn
#define noreturn _Noreturn
#endif

/* Conformance marker (required by C11 §7.23) */
#define __noreturn_is_defined 1

#endif /* _RCC_STDNORETURN_H */
