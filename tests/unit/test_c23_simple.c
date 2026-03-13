// test_c23_simple.c - Simple C23 feature test without stdio
// Tests binary literals, digit separators, nullptr, bool

int assert_failed = 0;

void assert(int condition) {
    if (!condition) {
        assert_failed = 1;
    }
}

int main() {
    // Test 1: Binary literals
    int bin1 = 0b1010;
    int bin2 = 0b11110000;
    int bin3 = 0B1;
    int bin4 = 0b11111111;

    assert(bin1 == 10);
    assert(bin2 == 240);
    assert(bin3 == 1);
    assert(bin4 == 255);

    // Test 2: Digit separators
    int million = 1'000'000;
    int bin_sep = 0b1111'0000'1010;
    int hex_sep = 0xFF'EE;

    assert(million == 1000000);
    assert(bin_sep == 3850);
    assert(hex_sep == 65518);

    // Test 3: nullptr
    int *p = nullptr;
    assert(p == 0);

    // Test 4: bool, true, false
    bool flag = true;
    bool zero = false;
    assert(flag == 1);
    assert(zero == 0);

    // Test 5: Combined features
    int val1 = 0b1111'1111;
    int val2 = 0x1'2'3'4;
    assert(val1 == 255);
    assert(val2 == 4660);

    // Test 6: Octal with separators
    int oct1 = 0'777;
    int oct2 = 0'1'2'3;
    assert(oct1 == 511);
    assert(oct2 == 83);

    // Return 0 if all tests passed, 1 otherwise
    return assert_failed;
}
