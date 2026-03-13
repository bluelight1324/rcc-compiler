// Simple test for Tier 1 features
extern int printf(const char* fmt, ...);

// Test constexpr
constexpr int SIZE = 10;
constexpr int VAL = 5 + 3;

// Test auto
int test_auto(void) {
    auto x = 42;
    auto y = 100;
    int sum = x + y;
    printf("auto test: %d + %d = %d\n", x, y, sum);
    return sum == 142;
}

// Test typeof
int test_typeof(void) {
    int x = 123;
    int sz = sizeof(typeof(x));
    printf("typeof test: sizeof(typeof(x)) = %d\n", sz);
    return sz == 4;
}

// Test constexpr
int test_constexpr(void) {
    int arr[SIZE];
    arr[0] = VAL;
    printf("constexpr test: SIZE=%d, VAL=%d, arr[0]=%d\n", SIZE, VAL, arr[0]);
    return arr[0] == 8;
}

int main(void) {
    int passed = 0;

    printf("=== Tier 1 Simple Tests ===\n");

    if (test_auto()) passed = passed + 1;
    if (test_typeof()) passed = passed + 1;
    if (test_constexpr()) passed = passed + 1;

    printf("\nPassed: %d/3\n", passed);
    return 3 - passed;
}
