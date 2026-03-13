/* test_typed_enum.c
 * C23 §6.7.2.2 — Typed enum with underlying type specifier.
 *
 * RCC preprocesses 'enum TAG : TYPE { ... }' by stripping ': TYPE'.
 * The preprocessor records the underlying size in g_enum_underlying_sizes.
 * CodeGen::resolveType looks up this table for sizeof/alignof.
 *
 * Expected: RCC exit 0 (compile success), correct output.
 */
#include <stdio.h>

/* Global typed enums with various underlying types */
enum Color : unsigned char { RED = 0, GREEN = 1, BLUE = 2 };
enum Status : int { STATUS_OK = 0, STATUS_ERR = 1 };
enum Bits : unsigned int { BIT0 = 1, BIT1 = 2, BIT2 = 4 };
enum Wide : long long { WIDE_A = 0, WIDE_B = 1, WIDE_MAX = 0x7FFFFFFFLL };

int main(void) {
    enum Color c = GREEN;
    enum Status s = STATUS_ERR;
    enum Bits b = BIT1;

    /* Local typed enum */
    enum Dir : unsigned char { NORTH = 0, SOUTH = 1, EAST = 2, WEST = 3 };
    enum Dir d = EAST;

    printf("Color=%d Status=%d Bits=%d Dir=%d\n",
           (int)c, (int)s, (int)b, (int)d);

    /* sizeof assertions for typed enum underlying types */
    int sc = (int)sizeof(enum Color);   /* : unsigned char → 1 */
    int ss = (int)sizeof(enum Status);  /* : int           → 4 */
    int sb = (int)sizeof(enum Bits);    /* : unsigned int  → 4 */
    int sw = (int)sizeof(enum Wide);    /* : long long     → 8 */

    printf("sizeof: Color=%d Status=%d Bits=%d Wide=%d\n", sc, ss, sb, sw);

    if (sc != 1) { printf("FAIL: sizeof(enum Color) == %d, want 1\n", sc); return 1; }
    if (ss != 4) { printf("FAIL: sizeof(enum Status) == %d, want 4\n", ss); return 1; }
    if (sb != 4) { printf("FAIL: sizeof(enum Bits) == %d, want 4\n", sb); return 1; }
    if (sw != 8) { printf("FAIL: sizeof(enum Wide) == %d, want 8\n", sw); return 1; }

    printf("All sizeof checks PASS\n");
    return (int)c - 1; /* GREEN=1, so exit 0 */
}
