#ifndef _TIME_H
#define _TIME_H

#define NULL ((void*)0)

typedef long long time_t;
typedef long long clock_t;

struct tm {
    int tm_sec;
    int tm_min;
    int tm_hour;
    int tm_mday;
    int tm_mon;
    int tm_year;
    int tm_wday;
    int tm_yday;
    int tm_isdst;
};

#define CLOCKS_PER_SEC 1000

extern time_t time(time_t* timer);
extern clock_t clock(void);
extern double difftime(time_t end, time_t beginning);
extern struct tm* localtime(const time_t* timer);
extern struct tm* gmtime(const time_t* timer);
extern time_t mktime(struct tm* timeptr);
extern char* asctime(const struct tm* timeptr);
extern char* ctime(const time_t* timer);
extern long long strftime(char* str, long long maxsize, const char* format, const struct tm* timeptr);

/* ── C11 §7.27.1: struct timespec ───────────────────────────────────────── */
struct timespec {
    long long tv_sec;   /* whole seconds (>= 0) */
    long long tv_nsec;  /* nanoseconds [0, 999999999] */
};

/* C11 §7.27.2.5: timespec_get — get time in seconds+nanoseconds.
 * base = TIME_UTC (1): fill *ts with UTC time since the Epoch.
 * Returns base on success, 0 on error.
 * On Windows, maps to timespec_get() from UCRT (Windows 10+). */
#define TIME_UTC 1
extern int timespec_get(struct timespec* ts, int base);

/* C23 §7.27.2.3: timegm — convert struct tm (UTC) to time_t.
 * Inverse of gmtime(); does not adjust for local timezone.
 * On Windows UCRT, _mkgmtime() provides the same semantics. */
#ifndef timegm
#define timegm _mkgmtime
#endif

/* C23 §7.27.3: timespec_getres — query the resolution of the time source.
 * Fills *ts with the resolution (smallest representable increment).
 * Returns base on success, 0 if base is not supported.
 * On Windows x64, the system clock resolution is typically 100 nanoseconds
 * (the resolution of FILETIME / QueryPerformanceCounter on modern systems).
 * This inline provides a conservative estimate; actual resolution may be finer. */
#ifndef _RCC_SIZE_T
#define _RCC_SIZE_T
typedef unsigned long long size_t;
#endif
static __inline int timespec_getres(struct timespec *ts, int base) {
    if (base != TIME_UTC) return 0;
    if (ts) {
        ts->tv_sec  = 0;
        ts->tv_nsec = 100;  /* 100 ns = Windows FILETIME resolution */
    }
    return TIME_UTC;
}

#endif
