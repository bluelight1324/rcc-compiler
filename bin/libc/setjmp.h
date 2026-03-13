/* setjmp.h — C11/C23 §7.13 Non-local jumps for RCC (Windows x64)
 *
 * setjmp/longjmp provide non-local transfer of control, typically used for
 * error recovery and implementing coroutine-like constructs.
 *
 * On Windows x64, setjmp is a compiler intrinsic that calls _setjmp internally
 * and captures the frame pointer. longjmp maps directly to the MSVCRT export.
 *
 * jmp_buf size: 272 bytes on Windows x64 (CONTEXT-compatible).
 * Platform: Windows x64
 */
#ifndef _RCC_SETJMP_H
#define _RCC_SETJMP_H

/* jmp_buf: opaque buffer to hold execution context.
 * On Windows x64 this must be large enough to save the callee-saved registers,
 * the stack pointer, frame pointer and instruction pointer (272 bytes). */
typedef unsigned char jmp_buf[272];

/* setjmp: save current execution context into env.
 * Returns 0 on direct call; returns val (or 1 if val==0) when resumed via longjmp.
 * On Windows, setjmp is a macro wrapping _setjmp with a null frame pointer.
 * Usage: if (setjmp(env) == 0) { ... try ... } else { ... catch ... }
 */
int _setjmp(jmp_buf _Env, void *_Fp);
#define setjmp(env) _setjmp((env), 0)

/* longjmp: restore execution context saved by setjmp.
 * Transfers control back to the setjmp call that saved env,
 * causing it to return val (or 1 if val == 0). */
void longjmp(jmp_buf _Env, int _Val);

#endif /* _RCC_SETJMP_H */
