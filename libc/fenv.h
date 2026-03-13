#ifndef _RCC_FENV_H
#define _RCC_FENV_H
/*
 * <fenv.h> — C99/C11 §7.6 Floating-point environment for RCC (Windows x64)
 *
 * Provides C11-compliant type definitions and function declarations that
 * map to x87/SSE control word manipulation via MSVCRT/UCRT.
 *
 * RCC note: __STDC_NO_COMPLEX__ == 1.  FP environment is available on x64.
 */

/* ── Types ──────────────────────────────────────────────────────────────── */
typedef unsigned int  fexcept_t;    /* FP exception flags */
typedef struct {
    unsigned int _control;          /* x87 control word */
    unsigned int _status;           /* x87 status word */
    unsigned int _mxcsr;            /* SSE MXCSR */
} fenv_t;

/* ── Exception flag macros (x87 / SSE MXCSR bits) ───────────────────────── */
#define FE_INVALID    0x01
#define FE_DIVBYZERO  0x04
#define FE_OVERFLOW   0x08
#define FE_UNDERFLOW  0x10
#define FE_INEXACT    0x20
#define FE_ALL_EXCEPT (FE_INVALID | FE_DIVBYZERO | FE_OVERFLOW | FE_UNDERFLOW | FE_INEXACT)

/* ── Rounding direction macros (matches x87 / SSE RC field) ─────────────── */
#define FE_TONEAREST  0x0000
#define FE_DOWNWARD   0x0400
#define FE_UPWARD     0x0800
#define FE_TOWARDZERO 0x0C00

/* ── Default environment ─────────────────────────────────────────────────── */
extern const fenv_t* _RCC_FE_DFL_ENV;
#define FE_DFL_ENV (_RCC_FE_DFL_ENV)

/* ── C99 §7.6.2: FP exception functions ─────────────────────────────────── */
int feclearexcept(int excepts);
int fegetexceptflag(fexcept_t* flagp, int excepts);
int feraiseexcept(int excepts);
int fesetexceptflag(const fexcept_t* flagp, int excepts);
int fetestexcept(int excepts);

/* ── C99 §7.6.3: Rounding direction ─────────────────────────────────────── */
int fegetround(void);
int fesetround(int round);

/* ── C99 §7.6.4: FP environment ─────────────────────────────────────────── */
int fegetenv(fenv_t* envp);
int feholdexcept(fenv_t* envp);
int fesetenv(const fenv_t* envp);
int feupdateenv(const fenv_t* envp);

#endif /* _RCC_FENV_H */
