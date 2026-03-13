#ifndef _STDARG_H
#define _STDARG_H

typedef char* va_list;

/* C23 §7.16.1.4: va_start supports 1 arg (variadic-only fn) or 2 args (last named param).
 * The codegen handler computes the offset from the function's param count, so the
 * second argument (last named param) is accepted but not used in code generation. */
#define va_start(ap, ...) __builtin_va_start(ap)
/* va_arg: the type argument is accepted for compatibility but is not forwarded to
 * the parser (RCC ignores it — always loads 8 bytes from the vararg stack). */
#define va_arg(ap, type) __builtin_va_arg(ap)
#define va_end(ap) __builtin_va_end(ap)
#define va_copy(dest, src) __builtin_va_copy(dest, src)

#endif
