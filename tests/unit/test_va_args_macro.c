// Test __VA_ARGS__ preprocessor support
#define LOG(fmt, ...) printf(fmt, __VA_ARGS__)
#define DEBUG(...) printf("DEBUG: " __VA_ARGS__)
#define WARN(msg, ...) printf("WARNING: " msg "\n", __VA_ARGS__)

extern int printf(const char* fmt, ...);

int main(void) {
    printf("=== __VA_ARGS__ Preprocessor Test ===\n");

    // Test 1: Basic __VA_ARGS__
    LOG("Test %d: %s\n", 1, "basic");

    // Test 2: All variadic
    DEBUG("Test %d\n", 2);

    // Test 3: Named + variadic
    WARN("Test %d", 3);

    printf("\nPASSED: __VA_ARGS__ macros work!\n");
    return 0;
}
