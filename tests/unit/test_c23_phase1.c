// test_c23_phase1.c - C23 Phase 1 Core Language Features Test
// Tests: binary literals, digit separators, nullptr, bool/true/false,
//        #elifdef, #elifndef, #warning preprocessor directives

#include <stdio.h>

// Test 1: Binary literals (C23 feature)
void test_binary_literals() {
    int a = 0b1010;         // 10 in decimal
    int b = 0b11110000;     // 240 in decimal
    int c = 0B1;            // 1 in decimal
    int d = 0b11111111;     // 255 in decimal

    printf("Test 1 - Binary literals:\n");
    printf("  0b1010 = %d (expected 10)\n", a);
    printf("  0b11110000 = %d (expected 240)\n", b);
    printf("  0B1 = %d (expected 1)\n", c);
    printf("  0b11111111 = %d (expected 255)\n", d);

    if (a == 10 && b == 240 && c == 1 && d == 255) {
        printf("  PASS: All binary literals correct\n\n");
    } else {
        printf("  FAIL: Binary literal mismatch\n\n");
    }
}

// Test 2: Digit separators (C23 feature)
void test_digit_separators() {
    int million = 1'000'000;           // decimal with separators
    int hex_val = 0xFF'EE'DD'CC;       // hex with separators
    int bin_val = 0b1111'0000'1010;    // binary with separators

    printf("Test 2 - Digit separators:\n");
    printf("  1'000'000 = %d (expected 1000000)\n", million);
    printf("  0xFF'EE'DD'CC = %d\n", hex_val);
    printf("  0b1111'0000'1010 = %d (expected 3850)\n", bin_val);

    if (million == 1000000 && bin_val == 3850) {
        printf("  PASS: Digit separators work correctly\n\n");
    } else {
        printf("  FAIL: Digit separator error\n\n");
    }
}

// Test 3: nullptr and bool (C23 keywords)
void test_nullptr_and_bool() {
    int *p = nullptr;
    bool flag = true;
    bool zero = false;

    printf("Test 3 - nullptr and bool:\n");
    printf("  nullptr = %p (expected 0)\n", p);
    printf("  true = %d (expected 1)\n", flag);
    printf("  false = %d (expected 0)\n", zero);

    if (p == nullptr && flag == 1 && zero == 0) {
        printf("  PASS: nullptr and bool work correctly\n\n");
    } else {
        printf("  FAIL: nullptr or bool error\n\n");
    }
}

// Test 4: Preprocessor #elifdef and #elifndef (C23 feature)
#define FEATURE_A
#undef FEATURE_B
#define FEATURE_C

void test_elifdef_elifndef() {
    printf("Test 4 - #elifdef and #elifndef:\n");

    // Test #elifdef
    #ifdef FEATURE_X
        int x = 1;
    #elifdef FEATURE_A
        int x = 2;
    #elifdef FEATURE_B
        int x = 3;
    #else
        int x = 4;
    #endif

    printf("  #elifdef result: x = %d (expected 2)\n", x);

    // Test #elifndef
    #ifdef FEATURE_Y
        int y = 1;
    #elifndef FEATURE_B
        int y = 2;
    #elifndef FEATURE_C
        int y = 3;
    #else
        int y = 4;
    #endif

    printf("  #elifndef result: y = %d (expected 2)\n", y);

    if (x == 2 && y == 2) {
        printf("  PASS: #elifdef and #elifndef work correctly\n\n");
    } else {
        printf("  FAIL: #elifdef or #elifndef error\n\n");
    }
}

// Test 5: Combined binary and digit separator
void test_combined_features() {
    int val1 = 0b1111'1111;           // 255 with separator
    int val2 = 0x1'2'3'4;             // hex with multiple separators
    int val3 = 1'0'0'0;               // decimal with multiple separators

    printf("Test 5 - Combined features:\n");
    printf("  0b1111'1111 = %d (expected 255)\n", val1);
    printf("  0x1'2'3'4 = %d (expected 4660)\n", val2);
    printf("  1'0'0'0 = %d (expected 1000)\n", val3);

    if (val1 == 255 && val2 == 4660 && val3 == 1000) {
        printf("  PASS: Combined features work\n\n");
    } else {
        printf("  FAIL: Combined feature error\n\n");
    }
}

// Test 6: Octal with digit separators
void test_octal_with_separators() {
    int octal1 = 0'777;      // octal with separator
    int octal2 = 0'1'2'3;    // octal with multiple separators

    printf("Test 6 - Octal with digit separators:\n");
    printf("  0'777 = %d (expected 511)\n", octal1);
    printf("  0'1'2'3 = %d (expected 83)\n", octal2);

    if (octal1 == 511 && octal2 == 83) {
        printf("  PASS: Octal with separators works\n\n");
    } else {
        printf("  FAIL: Octal separator error\n\n");
    }
}

// Test 7: Float with digit separators
void test_float_with_separators() {
    float f1 = 1'000.5;
    float f2 = 3.141'592'653;

    printf("Test 7 - Float with digit separators:\n");
    printf("  1'000.5 = %f (expected 1000.5)\n", f1);
    printf("  3.141'592'653 = %f\n", f2);

    if (f1 > 1000.4 && f1 < 1000.6) {
        printf("  PASS: Float with separators works\n\n");
    } else {
        printf("  FAIL: Float separator error\n\n");
    }
}

int main() {
    printf("=== C23 Phase 1 Feature Tests ===\n\n");

    test_binary_literals();
    test_digit_separators();
    test_nullptr_and_bool();
    test_elifdef_elifndef();
    test_combined_features();
    test_octal_with_separators();
    test_float_with_separators();

    printf("=== All C23 Phase 1 Tests Complete ===\n");
    return 0;
}
