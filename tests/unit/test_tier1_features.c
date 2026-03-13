// Test file for Tier 1 features (Task 55)
// Tests: typeof, auto, VLAs, constexpr, va_list

extern int printf(const char* fmt, ...);

// Test counter for assertions
int tests_passed = 0;
int tests_failed = 0;

void assert_eq(int actual, int expected, const char* msg) {
    if (actual == expected) {
        tests_passed = tests_passed + 1;
    } else {
        tests_failed = tests_failed + 1;
        printf("FAIL: %s - expected %d, got %d\n", msg, expected, actual);
    }
}

// =============================================================================
// Test 1: typeof operator
// =============================================================================

int test_typeof(void) {
    int x = 42;
    long y = 100;
    char c = 'A';

    // sizeof(typeof(x)) should equal sizeof(int)
    int size_x = sizeof(typeof(x));
    assert_eq(size_x, 4, "sizeof(typeof(int))");

    // sizeof(typeof(y)) should equal sizeof(long)
    int size_y = sizeof(typeof(y));
    assert_eq(size_y, 8, "sizeof(typeof(long))");

    // sizeof(typeof(c)) should equal sizeof(char)
    int size_c = sizeof(typeof(c));
    assert_eq(size_c, 1, "sizeof(typeof(char))");

    return 0;
}

// =============================================================================
// Test 2: auto type inference
// =============================================================================

int test_auto(void) {
    // auto should infer type from initializer
    auto a = 42;          // should be int
    auto b = 3.14f;       // should be float (but treated as int by our simple impl)
    auto ptr = &a;        // should be int*

    // Verify the auto-inferred variable works
    assert_eq(a, 42, "auto int");

    // Verify pointer works
    *ptr = 100;
    assert_eq(a, 100, "auto pointer dereference");

    return 0;
}

// =============================================================================
// Test 3: Variable-Length Arrays (VLAs)
// =============================================================================

int sum_vla(int n) {
    // VLA: array with runtime-determined size
    int arr[n];

    // Initialize the VLA
    for (int i = 0; i < n; i = i + 1) {
        arr[i] = i + 1;
    }

    // Sum the elements
    int sum = 0;
    for (int i = 0; i < n; i = i + 1) {
        sum = sum + arr[i];
    }

    return sum;
}

int test_vla(void) {
    // Sum of 1..5 = 15
    int result5 = sum_vla(5);
    assert_eq(result5, 15, "VLA sum(1..5)");

    // Sum of 1..10 = 55
    int result10 = sum_vla(10);
    assert_eq(result10, 55, "VLA sum(1..10)");

    return 0;
}

// =============================================================================
// Test 4: constexpr specifier
// =============================================================================

// constexpr values are compile-time constants
constexpr int ARRAY_SIZE = 10;
constexpr int MULTIPLIER = 3;
constexpr int COMPUTED = 5 + 7;  // Should be 12

int test_constexpr(void) {
    // constexpr can be used for array sizes
    int arr[ARRAY_SIZE];

    // Initialize
    for (int i = 0; i < ARRAY_SIZE; i = i + 1) {
        arr[i] = i * MULTIPLIER;
    }

    // Verify computed constexpr
    assert_eq(COMPUTED, 12, "constexpr computed");

    // Verify array element
    assert_eq(arr[3], 9, "constexpr array element");

    // constexpr in expressions
    int x = ARRAY_SIZE * MULTIPLIER;
    assert_eq(x, 30, "constexpr in expression");

    return 0;
}

// =============================================================================
// Test 5: Variadic function support (va_list)
// =============================================================================

// Simple variadic function that sums integers
// Note: RCC's va_arg takes just the va_list, returns next 8-byte value
int sum_ints(int count, ...) {
    va_list args;
    va_start(args, count);

    int total = 0;
    for (int i = 0; i < count; i = i + 1) {
        int val = va_arg(args);
        total = total + val;
    }

    va_end(args);
    return total;
}

// Variadic function to find maximum
int max_ints(int count, ...) {
    va_list args;
    va_start(args, count);

    int max_val = va_arg(args);
    for (int i = 1; i < count; i = i + 1) {
        int val = va_arg(args);
        if (val > max_val) {
            max_val = val;
        }
    }

    va_end(args);
    return max_val;
}

int test_varargs(void) {
    // Test sum_ints
    int sum3 = sum_ints(3, 10, 20, 30);
    assert_eq(sum3, 60, "va_arg sum(10,20,30)");

    int sum5 = sum_ints(5, 1, 2, 3, 4, 5);
    assert_eq(sum5, 15, "va_arg sum(1,2,3,4,5)");

    // Test max_ints
    int max4 = max_ints(4, 5, 12, 8, 3);
    assert_eq(max4, 12, "va_arg max(5,12,8,3)");

    return 0;
}

// =============================================================================
// Main test runner
// =============================================================================

int main(void) {
    printf("=== Tier 1 Feature Tests ===\n\n");

    printf("Test 1: typeof operator\n");
    test_typeof();

    printf("Test 2: auto type inference\n");
    test_auto();

    printf("Test 3: Variable-Length Arrays (VLAs)\n");
    test_vla();

    printf("Test 4: constexpr specifier\n");
    test_constexpr();

    printf("Test 5: Variadic functions (va_list)\n");
    test_varargs();

    printf("\n=== Results ===\n");
    printf("Passed: %d\n", tests_passed);
    printf("Failed: %d\n", tests_failed);

    if (tests_failed == 0) {
        printf("All Tier 1 tests PASSED!\n");
    }

    return tests_failed;
}
