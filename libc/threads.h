#ifndef _RCC_THREADS_H
#define _RCC_THREADS_H
/*
 * <threads.h> — C11 §7.26 Thread support library for RCC (Windows x64)
 *
 * Provides C11-compliant type definitions AND function declarations that
 * map to Win32 thread primitives (CreateThread, CRITICAL_SECTION, etc.).
 *
 * __STDC_NO_THREADS__ is 1 — implementations are not linked by default.
 * This header provides declarations so thread-aware code can compile.
 *
 * Platform: Windows x64 (Vista+, kernel32.dll)
 */
#include <time.h>   /* struct timespec for timed functions (C11 §7.27.1) */

#ifndef __STDC_NO_THREADS__
#define __STDC_NO_THREADS__ 1
#endif

/* ── Basic types ─────────────────────────────────────────────────────────── */
typedef void*         thrd_t;       /* Win32 HANDLE */
typedef unsigned long tss_t;        /* Win32 DWORD (TLS index) */
typedef long          once_flag;    /* InterlockedCompareExchange once flag */

/* Mutex: opaque blob sized for Win64 CRITICAL_SECTION (40 bytes) */
typedef struct { long long _a; long long _b; long long _c;
                 long long _d; long long _e; } mtx_t;

/* Condition variable: opaque blob for Win64 CONDITION_VARIABLE (8 bytes) */
typedef struct { void* _ptr; } cnd_t;

/* Thread start function and TSS destructor */
typedef int  (*thrd_start_t)(void*);
typedef void (*tss_dtor_t)(void*);

/* Call-once initialiser */
#define ONCE_FLAG_INIT 0

/* ── Return codes (C11 §7.26.1) ─────────────────────────────────────────── */
enum {
    thrd_success  = 0,
    thrd_nomem    = 1,
    thrd_timedout = 2,
    thrd_busy     = 3,
    thrd_error    = 4
};

/* ── Mutex types (C11 §7.26.4.1) ────────────────────────────────────────── */
enum {
    mtx_plain     = 0,
    mtx_recursive = 1,
    mtx_timed     = 2
};

/* ── C11 §7.26.2: Thread management ─────────────────────────────────────── */
int    thrd_create(thrd_t* thr, thrd_start_t func, void* arg);
int    thrd_equal(thrd_t thr0, thrd_t thr1);
thrd_t thrd_current(void);
int    thrd_sleep(const struct timespec* duration, struct timespec* remaining);
void   thrd_yield(void);
void   thrd_exit(int res);
int    thrd_detach(thrd_t thr);
int    thrd_join(thrd_t thr, int* res);

/* ── C11 §7.26.3: Call once ──────────────────────────────────────────────── */
void   call_once(once_flag* flag, void (*func)(void));

/* ── C11 §7.26.4: Mutex functions ───────────────────────────────────────── */
int    mtx_init(mtx_t* mtx, int type);
int    mtx_lock(mtx_t* mtx);
int    mtx_timedlock(mtx_t* mtx, const struct timespec* ts);
int    mtx_trylock(mtx_t* mtx);
int    mtx_unlock(mtx_t* mtx);
void   mtx_destroy(mtx_t* mtx);

/* ── C11 §7.26.5: Condition variable functions ───────────────────────────── */
int    cnd_init(cnd_t* cond);
int    cnd_signal(cnd_t* cond);
int    cnd_broadcast(cnd_t* cond);
int    cnd_wait(cnd_t* cond, mtx_t* mtx);
int    cnd_timedwait(cnd_t* cond, mtx_t* mtx, const struct timespec* ts);
void   cnd_destroy(cnd_t* cond);

/* ── C11 §7.26.6: Thread-specific storage ────────────────────────────────── */
int    tss_create(tss_t* key, tss_dtor_t dtor);
void*  tss_get(tss_t key);
int    tss_set(tss_t key, void* val);
void   tss_delete(tss_t key);

#endif /* _RCC_THREADS_H */
