#ifndef _STDDEF_H
#define _STDDEF_H

#define NULL ((void*)0)
typedef long long size_t;
typedef long long ptrdiff_t;
typedef long long wchar_t;

/* C23 §7.19: nullptr_t — the type of nullptr */
typedef void* nullptr_t;

/* C23 §7.15: max_align_t — type with the strictest fundamental alignment */
typedef long double max_align_t;

/* C23 §7.1.6.1: unreachable() — invokes undefined behaviour (marks unreachable code).
 * On MSVC/RCC x86-64, __assume(0) hints the optimizer this path is dead.
 * Falls back to no-op which is standards-conforming (UB, so any behaviour is valid). */
#ifdef _MSC_VER
#  define unreachable() __assume(0)
#else
#  define unreachable() ((void)0)
#endif

/* C11 §7.19: offsetof — byte offset of member within struct */
#ifndef offsetof
#  define offsetof(type, member) ((size_t)&((type*)0)->member)
#endif

#endif
