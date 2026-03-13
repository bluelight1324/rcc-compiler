// Test varargs (variadic functions)
extern int printf(const char* fmt, ...);

int sum_args(int count, ...) {
    va_list args;
    va_start(args, count);
    int sum = 0;
    int i;
    for (i = 0; i < count; i = i + 1) {
        int val = va_arg(args);
        sum = sum + val;
    }
    va_end(args);
    return sum;
}

int main(void) {
    int passed = 0;
    printf("=== Varargs Test ===\n");

    int result = sum_args(3, 10, 20, 30);
    printf("sum_args(3, 10, 20, 30) = %d\n", result);
    if (result == 60) passed = passed + 1;

    printf("\nPassed: %d/1\n", passed);
    return 1 - passed;
}
