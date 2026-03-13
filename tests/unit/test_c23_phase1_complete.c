// test_c23_phase1_complete.c
// Comprehensive C23 Phase 1 Feature Test
// Tests all 8 implemented features (72% of Phase 1)

// Test #elifdef and #elifndef preprocessor directives
#define FEATURE_A
#undef FEATURE_B

#ifdef UNKNOWN
    int elifdef_test = 1;
#elifdef FEATURE_A
    int elifdef_test = 2;  // This should be selected
#else
    int elifdef_test = 3;
#endif

#ifdef UNKNOWN
    int elifndef_test = 1;
#elifndef FEATURE_B  // FEATURE_B is not defined, so this should be selected
    int elifndef_test = 2;
#else
    int elifndef_test = 3;
#endif

// Test #warning directive
#warning "This is a C23 warning - compilation should continue"

// Test unnamed function parameters (C23 feature)
int add_first_only(int x, int) {
    // Second parameter is unnamed and not accessible
    return x + 10;
}

// Another unnamed parameter test
void multi_unnamed(int a, int, int, int d) {
    // Only a and d are named
}

int assert_count = 0;
int failed = 0;

void assert(int condition) {
    assert_count++;
    if (!condition) {
        failed = 1;
    }
}

int main() {
    // Test 1: Binary literals (0b/0B)
    int bin1 = 0b1010;
    int bin2 = 0b11110000;
    int bin3 = 0B1;
    int bin4 = 0b11111111;

    assert(bin1 == 10);
    assert(bin2 == 240);
    assert(bin3 == 1);
    assert(bin4 == 255);

    // Test 2: Digit separators in decimal
    int million = 1'000'000;
    int thousand = 1'000;
    assert(million == 1000000);
    assert(thousand == 1000);

    // Test 3: Digit separators in hex
    int hex1 = 0xFF'EE;
    int hex2 = 0x1'2'3'4;
    assert(hex1 == 65518);
    assert(hex2 == 4660);

    // Test 4: Digit separators in binary
    int bin_sep1 = 0b1111'0000;
    int bin_sep2 = 0b1111'0000'1010;
    assert(bin_sep1 == 240);
    assert(bin_sep2 == 3850);

    // Test 5: Digit separators in octal
    int oct1 = 0'777;
    int oct2 = 0'1'2'3;
    assert(oct1 == 511);
    assert(oct2 == 83);

    // Test 6: nullptr keyword
    int *p1 = nullptr;
    int *p2 = nullptr;
    assert(p1 == 0);
    assert(p2 == nullptr);
    assert(p1 == p2);

    // Test 7: bool, true, false keywords
    bool flag1 = true;
    bool flag2 = false;
    assert(flag1 == 1);
    assert(flag2 == 0);
    assert(flag1 != flag2);

    // Test 8: Combined binary and digit separators
    int combo1 = 0b1111'1111;
    int combo2 = 0b1010'1010;
    assert(combo1 == 255);
    assert(combo2 == 170);

    // Test 9: Preprocessor conditionals (#elifdef/#elifndef)
    assert(elifdef_test == 2);   // Should be 2 (FEATURE_A is defined)
    assert(elifndef_test == 2);  // Should be 2 (FEATURE_B is not defined)

    // Test 10: Unnamed function parameters
    int result1 = add_first_only(5, 999);  // Second arg ignored
    assert(result1 == 15);

    // Call function with multiple unnamed parameters
    multi_unnamed(1, 2, 3, 4);  // Should compile without errors

    // Test 11: Large numbers with separators
    int large = 1'234'567;
    assert(large == 1234567);

    // Test 12: Hex with many separators
    int hex_multi = 0xA'B'C'D;
    assert(hex_multi == 43981);

    // Test 13: Float with digit separators
    float f = 1'000.5;
    assert((int)f == 1000);

    // Return 0 if all tests passed, 1 otherwise
    // Total assertions: 31
    return failed;
}
