extern int printf(char *fmt, ...);

int main() {
    int x, y, result;

    printf("=== Shift Operators ===\n");
    x = 1;
    printf("1 << 0 = %d\n", x << 0);
    printf("1 << 1 = %d\n", x << 1);
    printf("1 << 4 = %d\n", x << 4);
    printf("1 << 10 = %d\n", x << 10);

    x = 256;
    printf("256 >> 1 = %d\n", x >> 1);
    printf("256 >> 4 = %d\n", x >> 4);
    printf("256 >> 8 = %d\n", x >> 8);

    printf("\n=== Compound Assignment ===\n");
    x = 100;
    x += 50;
    printf("100 += 50 = %d\n", x);
    x -= 30;
    printf("150 -= 30 = %d\n", x);
    x *= 3;
    printf("120 *= 3 = %d\n", x);
    x /= 6;
    printf("360 /= 6 = %d\n", x);
    x %= 7;
    printf("60 %%= 7 = %d\n", x);

    printf("\n=== Bitwise Compound Assignment ===\n");
    x = 255;
    x &= 0xF0;
    printf("255 &= 0xF0 = %d\n", x);
    x |= 0x0F;
    printf("240 |= 0x0F = %d\n", x);
    x ^= 0xFF;
    printf("255 ^= 0xFF = %d\n", x);

    printf("\n=== Shift Compound Assignment ===\n");
    x = 1;
    x <<= 8;
    printf("1 <<= 8 = %d\n", x);
    x >>= 3;
    printf("256 >>= 3 = %d\n", x);

    return 0;
}
