// Test comma operator as expression
extern int printf(const char* fmt, ...);

int sideEffect = 0;

int incAndReturn(int val) {
    sideEffect = sideEffect + 1;
    return val;
}

int main(void) {
    printf("=== Comma Operator Test ===\n");

    int passed = 1;
    int result;

    // Test 1: Simple comma expression
    result = (incAndReturn(1), incAndReturn(2), 42);
    printf("Test 1: (inc(1), inc(2), 42) = %d (expected 42)\n", result);
    printf("  sideEffect = %d (expected 2)\n", sideEffect);
    if (result != 42 || sideEffect != 2) {
        printf("  FAIL\n");
        passed = 0;
    }

    // Test 2: Comma in for loop
    sideEffect = 0;
    int i;
    int j;
    for (i = 0, j = 10; i < 3; i++, j--) {
        sideEffect = sideEffect + i + j;
    }
    printf("\nTest 2: for(i=0,j=10; i<3; i++,j--) sum=%d\n", sideEffect);
    printf("  Expected: (0+10)+(1+9)+(2+8) = 30\n");
    if (sideEffect != 30) {
        printf("  FAIL\n");
        passed = 0;
    }

    // Test 3: Comma with assignment
    sideEffect = 0;
    int a;
    int b;
    result = (a = 5, b = 10, a + b);
    printf("\nTest 3: (a=5, b=10, a+b) = %d (expected 15)\n", result);
    if (result != 15 || a != 5 || b != 10) {
        printf("  FAIL\n");
        passed = 0;
    }

    if (passed) {
        printf("\nPASSED: Comma operator works!\n");
        return 0;
    } else {
        printf("\nFAILED!\n");
        return 1;
    }
}
