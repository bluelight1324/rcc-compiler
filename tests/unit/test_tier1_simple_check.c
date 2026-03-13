// Simple test to check which Tier 1 features currently work
extern int printf(const char* fmt, ...);

int main(void) {
    printf("Testing Tier 1 features...\n");

    // Test 1: typeof with sizeof
    int x = 42;
    int size1 = sizeof(typeof(x));
    printf("sizeof(typeof(int)): %d\n", size1);

    // Test 2: auto - may be storage class in C89, not type inference
    auto int y = 100;  // C89 auto (storage class)
    printf("auto int y = 100: %d\n", y);

    // Test 3: constexpr - may not be recognized
    // constexpr int SIZE = 10;  // Comment out if not working

    printf("Done\n");
    return 0;
}
