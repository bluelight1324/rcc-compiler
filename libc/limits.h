#ifndef _LIMITS_H
#define _LIMITS_H

#define CHAR_BIT 8
#define CHAR_MIN (-128)
#define CHAR_MAX 127
#define SCHAR_MIN (-128)
#define SCHAR_MAX 127
#define UCHAR_MAX 255
#define SHRT_MIN (-32768)
#define SHRT_MAX 32767
#define USHRT_MAX 65535
#define INT_MIN (-2147483647-1)
#define INT_MAX 2147483647
#define UINT_MAX 4294967295U
#define LONG_MIN (-2147483647-1)
#define LONG_MAX 2147483647
#define ULONG_MAX 4294967295UL
#define LLONG_MIN (-9223372036854775807LL-1)
#define LLONG_MAX 9223372036854775807LL
#define ULLONG_MAX 18446744073709551615ULL

/* C23 §5.2.4.2.1: integer width macros (number of value + sign bits) */
#define BOOL_WIDTH   1
#define CHAR_WIDTH   8
#define SCHAR_WIDTH  8
#define UCHAR_WIDTH  8
#define SHRT_WIDTH  16
#define USHRT_WIDTH 16
#define INT_WIDTH   32
#define UINT_WIDTH  32
#define LONG_WIDTH  32   /* Windows x64: long is 32-bit */
#define ULONG_WIDTH 32
#define LLONG_WIDTH  64
#define ULLONG_WIDTH 64

/* C23: BOOL_MAX */
#define BOOL_MAX 1

#endif
