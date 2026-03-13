// Test preprocessor # and ## operators
extern int printf(const char* fmt, ...);

// Test #: stringification
#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

// Test ##: token pasting
#define CONCAT(a, b) a ## b
#define MAKE_VAR(n) var_ ## n

int main(void) {
    int var_1 = 10;
    int var_2 = 20;
    int var_3 = 30;

    printf("=== Preprocessor Operator Tests ===\n");

    // Test ##: token pasting
    printf("var_1 = %d\n", MAKE_VAR(1));
    printf("var_2 = %d\n", MAKE_VAR(2));
    printf("var_3 = %d\n", MAKE_VAR(3));
    printf("CONCAT(10, 20) = %d\n", CONCAT(10, 20));

    // Test #: stringification
    printf("STRINGIFY(hello) = %s\n", STRINGIFY(hello));
    printf("STRINGIFY(42) = %s\n", STRINGIFY(42));

    int passed = 1;
    if (MAKE_VAR(1) != 10) passed = 0;
    if (MAKE_VAR(2) != 20) passed = 0;
    if (MAKE_VAR(3) != 30) passed = 0;
    if (CONCAT(10, 20) != 1020) passed = 0;

    if (passed) {
        printf("\nPASSED: All preprocessor operator tests!\n");
    } else {
        printf("\nFAILED!\n");
    }
    return passed ? 0 : 1;
}
