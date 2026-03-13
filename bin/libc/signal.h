/* signal.h — C11/C23 §7.14 Signal handling for RCC (Windows x64)
 *
 * Provides signal constants, the signal() function and raise() function.
 * All declarations map to MSVCRT implementations.
 *
 * On Windows, signal numbers differ from POSIX. The constants below use
 * the MSVCRT values which match POSIX for the standard C signals.
 *
 * Platform: Windows x64
 */
#ifndef _RCC_SIGNAL_H
#define _RCC_SIGNAL_H

/* Standard C signal numbers (match MSVCRT and POSIX values) */
#define SIGABRT  22    /* Abnormal termination (abort()) */
#define SIGFPE    8    /* Floating-point exception */
#define SIGILL    4    /* Illegal instruction */
#define SIGINT    2    /* Interactive attention (Ctrl+C) */
#define SIGSEGV  11    /* Segmentation fault (invalid memory access) */
#define SIGTERM  15    /* Termination request */

/* Special signal handler values */
#define SIG_DFL  ((void (*)(int))0)    /* default signal action */
#define SIG_ERR  ((void (*)(int))-1)   /* error return from signal() */
#define SIG_IGN  ((void (*)(int))1)    /* ignore signal */

/* sig_atomic_t: integer type that can be read/written atomically */
typedef int sig_atomic_t;

/* §7.14.1.1: signal — install a handler for signal sig.
 * Returns the previous handler, or SIG_ERR on error.
 * Handler is called with the signal number as argument. */
void (*signal(int _Sig, void (*_Func)(int)))(int);

/* §7.14.1.2: raise — send signal sig to the executing program.
 * Returns 0 on success, non-zero on failure. */
int raise(int _Sig);

#endif /* _RCC_SIGNAL_H */
