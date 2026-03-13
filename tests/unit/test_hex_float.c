/* test_hex_float.c - W18: Hex float literal support (C99) */
#include <stdio.h>

int main() {
    /* 0x1.fp3 = 1.9375 * 2^3 = 15.5 */
    double d1 = 0x1.fp3;
    /* 0x1p0 = 1.0 */
    double d2 = 0x1p0;
    /* 0x1.8p1 = 1.5 * 2 = 3.0 */
    double d3 = 0x1.8p1;

    printf("%.1f\n", d1);   /* 15.5 */
    printf("%.1f\n", d2);   /* 1.0  */
    printf("%.1f\n", d3);   /* 3.0  */

    return 0;
}
