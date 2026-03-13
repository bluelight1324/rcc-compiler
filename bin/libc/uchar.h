/* uchar.h — C11 Unicode character types for RCC
 *
 * C11 §7.28: Unicode character types.
 *
 * char16_t: unsigned 16-bit type for UTF-16 code units (Windows wchar_t ABI)
 * char32_t: unsigned 32-bit type for UTF-32 code points
 *
 * On Windows x64: char16_t == unsigned short, char32_t == unsigned int
 *
 * The conversion functions (mbrtoc16, mbrtoc32, c16rtomb, c32rtomb) map to
 * the MSVCRT implementation. They require a valid locale and are declared here
 * for declaration compatibility.
 *
 * Note: u"..." (UTF-16) and U"..." (UTF-32) string literal prefixes are
 * deferred to RCC v4.8.0. char8_t / u8"..." are already supported (task 72.17).
 */
#ifndef _RCC_UCHAR_H
#define _RCC_UCHAR_H

#ifndef _RCC_SIZE_T
#define _RCC_SIZE_T
typedef unsigned long long size_t;
#endif

/* mbstate_t — opaque multibyte conversion state */
#ifndef _RCC_MBSTATE_T
#define _RCC_MBSTATE_T
typedef struct { unsigned long long _opaque[2]; } mbstate_t;
#endif

/* char16_t: C11 UTF-16 code unit (uint_least16_t on this platform) */
typedef unsigned short char16_t;

/* char32_t: C11 UTF-32 code point (uint_least32_t on this platform) */
typedef unsigned int   char32_t;

/* C11 §7.28.1: Restartable multibyte/wide character conversion functions.
 * These pass through to MSVCRT which implements them via the current locale. */
size_t mbrtoc16(char16_t* pc16, const char* s, size_t n, mbstate_t* ps);
size_t c16rtomb(char* s, char16_t c16, mbstate_t* ps);
size_t mbrtoc32(char32_t* pc32, const char* s, size_t n, mbstate_t* ps);
size_t c32rtomb(char* s, char32_t c32, mbstate_t* ps);

/* C23 §7.30.2: UTF-8 char8_t multibyte conversion.
 * char8_t is unsigned char (RCC predefined macro).
 * When __STDC_UTF_8__ is defined (source encoding is UTF-8), these functions
 * pass through one UTF-8 byte at a time.  Each call consumes exactly one byte
 * and emits exactly one char8_t code unit, relying on the caller to iterate
 * over multi-byte sequences byte-by-byte.  This is correct for UTF-8 sources
 * because each byte in a valid UTF-8 sequence is already a valid char8_t unit. */
static __inline size_t mbrtoc8(unsigned char* pc8, const char* s,
                                size_t n, mbstate_t* ps) {
    (void)ps;
    if (!s) return 0;           /* null: reset state (stateless impl) */
    if (n == 0) return (size_t)-2;  /* incomplete sequence */
    unsigned char b = (unsigned char)*s;
    if (pc8) *pc8 = b;
    return (b == 0) ? 0 : 1;   /* consume 1 byte, emit 1 char8_t unit */
}

static __inline size_t c8rtomb(char* s, unsigned char c8, mbstate_t* ps) {
    (void)ps;
    if (!s) return 1;           /* reset: stateless, always 1 */
    *s = (char)c8;              /* pass through byte unchanged */
    return 1;
}

#endif /* _RCC_UCHAR_H */
