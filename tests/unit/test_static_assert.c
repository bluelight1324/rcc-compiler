// Test C11 _Static_assert support
// Note: RCC implements _Static_assert as no-op preprocessor macro
// This allows code using _Static_assert to compile
extern int printf(const char* fmt, ...);

int main(void) {
    printf("=== Static Assert Test ===\n");

    // Test 1: Basic static assertion (always true)
    _Static_assert(1, "This should always pass");

    // Test 2: sizeof check
    _Static_assert(sizeof(int) >= 4, "int must be at least 4 bytes");

    // Test 3: Pointer size check
    _Static_assert(sizeof(void*) == 8, "pointer must be 8 bytes on x64");

    // Test 4: Math expressions
    _Static_assert(1 + 1 == 2, "Math works");
    _Static_assert(sizeof(char) == 1, "char is 1 byte");

    printf("All _Static_assert statements compiled successfully!\n");
    printf("\n");
    printf("Note: RCC implements _Static_assert as no-op macro ((void)0)\n");
    printf("Real implementation would:\n");
    printf("  - Require grammar changes (declaration context)\n");
    printf("  - Evaluate expressions at compile-time\n");
    printf("  - Emit error if assertion fails\n");
    printf("\n");
    printf("Current implementation:\n");
    printf("  - Works in function scope only\n");
    printf("  - Allows code to compile without errors\n");
    printf("  - Does not actually verify assertions\n");

    printf("\nPASSED: _Static_assert macro works (compilation succeeds)\n");
    return 0;
}
