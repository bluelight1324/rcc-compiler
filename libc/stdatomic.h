/* stdatomic.h — C11 §7.17 atomic operations for RCC (Windows x64)
 *
 * Key operations (atomic_fetch_add, exchange, compare_exchange, bitwise)
 * use Windows Interlocked intrinsics exported from kernel32.dll / ntdll.dll.
 * These provide LOCK-prefix semantics on x86-64: full sequential consistency.
 *
 * atomic_store / atomic_load: on x86-64 (TSO memory model), aligned MOV is
 * sequentially consistent for int-sized operands. Volatile load/store is
 * sufficient for correctness on x86-64 without explicit fences for most uses.
 *
 * __STDC_NO_ATOMICS__ is 0 — hardware atomic operations are provided.
 *
 * _Atomic(T) qualifier stripping is handled in the RCC grammar (task 72.28):
 * _Atomic(T) → T via grammar production; bare _Atomic qualifier → ignored.
 *
 * Platform: Windows x64 (Vista+, kernel32.dll)
 */
#ifndef _RCC_STDATOMIC_H
#define _RCC_STDATOMIC_H

/* C11 §7.17.1: memory_order sequencing constraints */
typedef int memory_order;
#define memory_order_relaxed 0
#define memory_order_consume 1
#define memory_order_acquire 2
#define memory_order_release 3
#define memory_order_acq_rel 4
#define memory_order_seq_cst 5

/* ── Windows Interlocked intrinsics (kernel32.dll / ntdll.dll, Windows Vista+)
 *    These provide real atomic LOCK-prefixed operations on x86-64.
 *    All operate on long long (64-bit) to cover int, long, and pointer types.
 * ─────────────────────────────────────────────────────────────────────────── */
long long InterlockedExchangeAdd64(long long volatile* Addend, long long Value);
long long InterlockedExchange64(long long volatile* Target, long long Value);
long long InterlockedCompareExchange64(long long volatile* Destination,
                                       long long Exchange, long long Comperand);
long long InterlockedOr64(long long volatile* Destination, long long Value);
long long InterlockedAnd64(long long volatile* Destination, long long Value);
long long InterlockedXor64(long long volatile* Destination, long long Value);

/* ── C11 §7.17.8: atomic_flag ─────────────────────────────────────────────── */
typedef struct { long long _val; } atomic_flag;
#define ATOMIC_FLAG_INIT { 0 }

/* C11 §7.17.2: initialization */
#define ATOMIC_VAR_INIT(val)  (val)
#define atomic_init(obj, val) (*(obj) = (val))

/* ── C11 §7.17.7: generic atomic operations ──────────────────────────────── */

/* atomic_store/load: volatile MOV is sequentially consistent on x86-64 TSO */
#define atomic_store(obj, val)               (*(volatile long long*)(obj) = (long long)(val))
#define atomic_store_explicit(obj, val, ord) (*(volatile long long*)(obj) = (long long)(val))
#define atomic_load(obj)                     (*(volatile long long*)(obj))
#define atomic_load_explicit(obj, ord)       (*(volatile long long*)(obj))

/* atomic_exchange: LOCK XCHG via InterlockedExchange64 */
#define atomic_exchange(obj, val) \
    InterlockedExchange64((long long volatile*)(obj), (long long)(val))
#define atomic_exchange_explicit(obj, val, ord) \
    atomic_exchange(obj, val)

/* atomic_fetch_add/sub: LOCK XADD via InterlockedExchangeAdd64 */
#define atomic_fetch_add(obj, val) \
    InterlockedExchangeAdd64((long long volatile*)(obj), (long long)(val))
#define atomic_fetch_add_explicit(obj, val, ord) \
    atomic_fetch_add(obj, val)
#define atomic_fetch_sub(obj, val) \
    InterlockedExchangeAdd64((long long volatile*)(obj), -(long long)(val))
#define atomic_fetch_sub_explicit(obj, val, ord) \
    atomic_fetch_sub(obj, val)

/* atomic_fetch_or/and/xor: LOCK OR/AND/XOR via Interlocked64 */
#define atomic_fetch_or(obj, val) \
    InterlockedOr64((long long volatile*)(obj), (long long)(val))
#define atomic_fetch_or_explicit(obj, val, ord) \
    atomic_fetch_or(obj, val)
#define atomic_fetch_and(obj, val) \
    InterlockedAnd64((long long volatile*)(obj), (long long)(val))
#define atomic_fetch_and_explicit(obj, val, ord) \
    atomic_fetch_and(obj, val)
#define atomic_fetch_xor(obj, val) \
    InterlockedXor64((long long volatile*)(obj), (long long)(val))
#define atomic_fetch_xor_explicit(obj, val, ord) \
    atomic_fetch_xor(obj, val)

/* atomic_compare_exchange: LOCK CMPXCHG via InterlockedCompareExchange64
 * Returns 1 if exchange occurred (old == expected), 0 otherwise.
 * On failure, *expected is updated with the current value. */
#define atomic_compare_exchange_strong(obj, exp, val) \
    (InterlockedCompareExchange64((long long volatile*)(obj), \
        (long long)(val), *(long long*)(exp)) == *(long long*)(exp) \
     ? 1 \
     : (*(long long*)(exp) = *(volatile long long*)(obj), 0))
#define atomic_compare_exchange_weak(obj, exp, val) \
    atomic_compare_exchange_strong(obj, exp, val)
#define atomic_compare_exchange_strong_explicit(obj, exp, val, s, f) \
    atomic_compare_exchange_strong(obj, exp, val)
#define atomic_compare_exchange_weak_explicit(obj, exp, val, s, f) \
    atomic_compare_exchange_strong(obj, exp, val)

/* ── C11 §7.17.8: atomic_flag operations ────────────────────────────────── */
#define atomic_flag_test_and_set(obj) \
    (InterlockedExchange64((long long volatile*)&(obj)->_val, 1LL) != 0)
#define atomic_flag_test_and_set_explicit(obj, ord) \
    atomic_flag_test_and_set(obj)
#define atomic_flag_clear(obj) \
    (InterlockedExchange64((long long volatile*)&(obj)->_val, 0LL), (void)0)
#define atomic_flag_clear_explicit(obj, ord) \
    atomic_flag_clear(obj)

/* ── C11 §7.17.4: memory fences ─────────────────────────────────────────── */
/* MemoryBarrier() from kernel32.dll emits MFENCE on x86-64 — a full
 * sequential fence (StoreLoad barrier). On x86-64 TSO, acquire and
 * release fences are guaranteed by the hardware for free.
 * We call MemoryBarrier() for seq_cst; use no-op for weaker orders. */
void MemoryBarrier(void);
#define atomic_thread_fence(ord) \
    ((ord) == memory_order_seq_cst ? (MemoryBarrier(), (void)0) : (void)0)
#define atomic_signal_fence(ord) ((void)0)
#define atomic_is_lock_free(obj) 1   /* all types are lock-free on x86-64 */

/* ── C11 §7.17.6: atomic type aliases ───────────────────────────────────── */
typedef long long          atomic_int;
typedef long long          atomic_uint;
typedef long long          atomic_long;
typedef long long          atomic_ulong;
typedef long long          atomic_llong;
typedef long long          atomic_ullong;
typedef long long          atomic_char;
typedef long long          atomic_schar;
typedef long long          atomic_uchar;
typedef long long          atomic_short;
typedef long long          atomic_ushort;
typedef long long          atomic_bool;
typedef long long          atomic_intptr_t;
typedef long long          atomic_uintptr_t;
typedef long long          atomic_size_t;
typedef long long          atomic_ptrdiff_t;
typedef long long          atomic_intmax_t;
typedef long long          atomic_uintmax_t;

#endif /* _RCC_STDATOMIC_H */
