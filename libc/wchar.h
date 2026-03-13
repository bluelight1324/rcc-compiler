/* wchar.h — C99/C11 §7.29 wide character utilities for RCC
 *
 * wchar_t is unsigned short (UTF-16 code unit, 2 bytes) on Windows x64.
 * wint_t is unsigned short (same width, used for wchar_t + WEOF).
 *
 * Function declarations map to Windows MSVCRT implementations.
 * Link with the C runtime to resolve these symbols.
 *
 * Platform: Windows x64
 */
#ifndef _RCC_WCHAR_H
#define _RCC_WCHAR_H

#ifndef _RCC_SIZE_T
#define _RCC_SIZE_T
typedef unsigned long long size_t;
#endif

#ifndef NULL
#define NULL ((void*)0)
#endif

/* wchar_t: UTF-16 code unit (unsigned short on Windows) */
#ifndef _RCC_WCHAR_T
#define _RCC_WCHAR_T
typedef unsigned short wchar_t;
#endif

/* wint_t: wide character result type; holds WEOF or any wchar_t value */
#ifndef _RCC_WINT_T
#define _RCC_WINT_T
typedef unsigned short wint_t;
#endif

/* mbstate_t: opaque multibyte conversion state */
#ifndef _RCC_MBSTATE_T
#define _RCC_MBSTATE_T
typedef struct { unsigned long long _opaque[2]; } mbstate_t;
#endif

/* Sentinel value returned by wide character I/O functions on error/EOF */
#define WEOF      ((wint_t)0xFFFF)
#define WCHAR_MIN 0
#define WCHAR_MAX 65535

/* §7.29.4 — Wide string functions */
size_t   wcslen (const wchar_t* s);
wchar_t* wcscpy (wchar_t* dst, const wchar_t* src);
wchar_t* wcsncpy(wchar_t* dst, const wchar_t* src, size_t n);
wchar_t* wcscat (wchar_t* dst, const wchar_t* src);
wchar_t* wcsncat(wchar_t* dst, const wchar_t* src, size_t n);
int      wcscmp (const wchar_t* s1, const wchar_t* s2);
int      wcsncmp(const wchar_t* s1, const wchar_t* s2, size_t n);
wchar_t* wcschr (const wchar_t* s, wchar_t c);
wchar_t* wcsrchr(const wchar_t* s, wchar_t c);
wchar_t* wcsstr (const wchar_t* haystack, const wchar_t* needle);

/* §7.29.3 — Wide character I/O */
int wprintf (const wchar_t* fmt, ...);
int swprintf(wchar_t* buf, size_t n, const wchar_t* fmt, ...);

/* §7.29.6 — Wide/multibyte character conversion */
wchar_t btowc(int c);
int     wctob(wint_t c);

/* C23 §7.31.4.1: wcsdup — duplicate a wide string.
 * Maps to Windows _wcsdup (POSIX extension promoted to C23 standard). */
wchar_t* _wcsdup(const wchar_t* s);
#define wcsdup _wcsdup

#endif /* _RCC_WCHAR_H */
