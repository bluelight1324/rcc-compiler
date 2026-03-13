// Test C23 binary literals and digit separators
extern int printf(const char* fmt, ...);

int main(void) {
    printf("=== Binary Literals & Digit Separators Test ===\n");

    int passed = 1;

    // Test 1: Binary literals
    int bin1 = 0b1010;
    int bin2 = 0B1111;
    printf("Test 1: Binary literals\n");
    printf("  0b1010 = %d (expected 10)\n", bin1);
    printf("  0B1111 = %d (expected 15)\n", bin2);
    if (bin1 != 10 || bin2 != 15) {
        printf("  FAIL\n");
        passed = 0;
    }

    // Test 2: Digit separators in decimal
    int million = 1'000'000;
    int num = 12'34'56;
    printf("\nTest 2: Decimal digit separators\n");
    printf("  1'000'000 = %d (expected 1000000)\n", million);
    printf("  12'34'56 = %d (expected 123456)\n", num);
    if (million != 1000000 || num != 123456) {
        printf("  FAIL\n");
        passed = 0;
    }

    // Test 3: Digit separators in hex
    int hex = 0xFF'FF;
    printf("\nTest 3: Hex digit separators\n");
    printf("  0xFF'FF = %d (expected 65535)\n", hex);
    if (hex != 65535) {
        printf("  FAIL\n");
        passed = 0;
    }

    // Test 4: Digit separators in binary
    int bin_sep = 0b1111'0000'1010'0101;
    printf("\nTest 4: Binary digit separators\n");
    printf("  0b1111'0000'1010'0101 = %d (expected 61605)\n", bin_sep);
    if (bin_sep != 61605) {
        printf("  FAIL\n");
        passed = 0;
    }

    if (passed) {
        printf("\nPASSED: Binary literals & digit separators work!\n");
        return 0;
    } else {
        printf("\nFAILED!\n");
        return 1;
    }
}
