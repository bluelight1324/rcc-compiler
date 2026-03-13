#ifndef _RCC_INTTYPES_H
#define _RCC_INTTYPES_H
/*
 * <inttypes.h> — C99/C11 §7.8 Format conversion of integer types for RCC
 *
 * Provides PRId/PRIu/PRIx/PRIo macros for printf/scanf, matching
 * Windows x64 LP64 (int=32, long=32, long long=64, pointer=64).
 */
#include <stdint.h>

/* ── printf format macros ───────────────────────────────────────────────── */
/* Decimal signed */
#define PRId8    "d"
#define PRId16   "d"
#define PRId32   "d"
#define PRId64   "lld"
#define PRIdLEAST8  "d"
#define PRIdLEAST16 "d"
#define PRIdLEAST32 "d"
#define PRIdLEAST64 "lld"
#define PRIdFAST8   "d"
#define PRIdFAST16  "d"
#define PRIdFAST32  "d"
#define PRIdFAST64  "lld"
#define PRIdMAX  "lld"
#define PRIdPTR  "lld"

/* Decimal unsigned */
#define PRIu8    "u"
#define PRIu16   "u"
#define PRIu32   "u"
#define PRIu64   "llu"
#define PRIuLEAST8  "u"
#define PRIuLEAST16 "u"
#define PRIuLEAST32 "u"
#define PRIuLEAST64 "llu"
#define PRIuFAST8   "u"
#define PRIuFAST16  "u"
#define PRIuFAST32  "u"
#define PRIuFAST64  "llu"
#define PRIuMAX  "llu"
#define PRIuPTR  "llu"

/* Hexadecimal lowercase */
#define PRIx8    "x"
#define PRIx16   "x"
#define PRIx32   "x"
#define PRIx64   "llx"
#define PRIxLEAST8  "x"
#define PRIxLEAST16 "x"
#define PRIxLEAST32 "x"
#define PRIxLEAST64 "llx"
#define PRIxFAST8   "x"
#define PRIxFAST16  "x"
#define PRIxFAST32  "x"
#define PRIxFAST64  "llx"
#define PRIxMAX  "llx"
#define PRIxPTR  "llx"

/* Hexadecimal uppercase */
#define PRIX8    "X"
#define PRIX16   "X"
#define PRIX32   "X"
#define PRIX64   "llX"
#define PRIXlEAST8  "X"
#define PRIXlEAST16 "X"
#define PRIXlEAST32 "X"
#define PRIXlEAST64 "llX"
#define PRIXfAST8   "X"
#define PRIXfAST16  "X"
#define PRIXfAST32  "X"
#define PRIXfAST64  "llX"
#define PRIXMAX  "llX"
#define PRIXPTR  "llX"

/* Octal */
#define PRIo8    "o"
#define PRIo16   "o"
#define PRIo32   "o"
#define PRIo64   "llo"
#define PRIoLEAST8  "o"
#define PRIoLEAST16 "o"
#define PRIoLEAST32 "o"
#define PRIoLEAST64 "llo"
#define PRIoFAST8   "o"
#define PRIoFAST16  "o"
#define PRIoFAST32  "o"
#define PRIoFAST64  "llo"
#define PRIoMAX  "llo"
#define PRIoPTR  "llo"

/* Signed (i) — same as d */
#define PRIi8    "i"
#define PRIi16   "i"
#define PRIi32   "i"
#define PRIi64   "lli"
#define PRIiLEAST8  "i"
#define PRIiLEAST16 "i"
#define PRIiLEAST32 "i"
#define PRIiLEAST64 "lli"
#define PRIiFAST8   "i"
#define PRIiFAST16  "i"
#define PRIiFAST32  "i"
#define PRIiFAST64  "lli"
#define PRIiMAX  "lli"
#define PRIiPTR  "lli"

/* ── scanf format macros ─────────────────────────────────────────────────── */
#define SCNd8    "hhd"
#define SCNd16   "hd"
#define SCNd32   "d"
#define SCNd64   "lld"
#define SCNdMAX  "lld"
#define SCNdPTR  "lld"

#define SCNu8    "hhu"
#define SCNu16   "hu"
#define SCNu32   "u"
#define SCNu64   "llu"
#define SCNuMAX  "llu"
#define SCNuPTR  "llu"

#define SCNx8    "hhx"
#define SCNx16   "hx"
#define SCNx32   "x"
#define SCNx64   "llx"
#define SCNxMAX  "llx"
#define SCNxPTR  "llx"

#define SCNi8    "hhi"
#define SCNi16   "hi"
#define SCNi32   "i"
#define SCNi64   "lli"
#define SCNiMAX  "lli"
#define SCNiPTR  "lli"

/* ── C99 §7.8.2.1: imaxabs ──────────────────────────────────────────────── */
long long imaxabs(long long n);

/* ── C99 §7.8.2.3-4: strtoimax / strtoumax ─────────────────────────────── */
long long          strtoimax(const char* s, char** endp, int base);
unsigned long long strtoumax(const char* s, char** endp, int base);

/* ── C99 §7.8.2.2: imaxdiv (return type requires full struct; use lldiv) ── */
/* imaxdiv_t and imaxdiv() omitted — use lldiv() from <stdlib.h> instead. */

#endif /* _RCC_INTTYPES_H */
