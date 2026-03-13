#ifndef _ASSERT_H
#define _ASSERT_H

extern int printf(const char* fmt, ...);
extern void abort(void);

#ifdef NDEBUG
#define assert(expr) ((void)0)
#else
/* Expression form: valid in any expression context including comma expressions */
#define assert(expr) ((expr) ? (void)0 : (printf("Assertion failed: " #expr " (%s:%d)\n", __FILE__, __LINE__), (void)abort()))
#endif

#endif
