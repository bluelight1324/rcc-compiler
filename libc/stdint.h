#ifndef _STDINT_H
#define _STDINT_H

typedef char int8_t;
typedef short int16_t;
typedef int int32_t;
typedef long long int64_t;

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;

typedef long long intptr_t;
typedef unsigned long long uintptr_t;

typedef long long intmax_t;
typedef unsigned long long uintmax_t;

#define INT8_MIN (-128)
#define INT8_MAX 127
#define INT16_MIN (-32768)
#define INT16_MAX 32767
#define INT32_MIN (-2147483647-1)
#define INT32_MAX 2147483647
#define INT64_MIN (-9223372036854775807LL-1)
#define INT64_MAX 9223372036854775807LL

#define UINT8_MAX 255U
#define UINT16_MAX 65535U
#define UINT32_MAX 4294967295U
#define UINT64_MAX 18446744073709551615ULL

#define INTPTR_MIN  (-9223372036854775807LL-1)
#define INTPTR_MAX  9223372036854775807LL
#define UINTPTR_MAX 18446744073709551615ULL

#define INTMAX_MIN  (-9223372036854775807LL-1)
#define INTMAX_MAX  9223372036854775807LL
#define UINTMAX_MAX 18446744073709551615ULL

#define SIZE_MAX UINT64_MAX

/* C23 §7.20.2.2: integer type width macros */
#define INT8_WIDTH    8
#define INT16_WIDTH   16
#define INT32_WIDTH   32
#define INT64_WIDTH   64
#define UINT8_WIDTH   8
#define UINT16_WIDTH  16
#define UINT32_WIDTH  32
#define UINT64_WIDTH  64
#define INTPTR_WIDTH  64
#define UINTPTR_WIDTH 64
#define INTMAX_WIDTH  64
#define UINTMAX_WIDTH 64
#define SIZE_WIDTH    64

#endif
