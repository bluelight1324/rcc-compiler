// Simple variadic test using __builtin functions directly
extern int printf(const char* fmt, ...);

int sum_ints(int count, ...) {
    char* args;  // va_list
    __builtin_va_start(args, count);

    int total = 0;
    for (int i = 0; i < count; i = i + 1) {
        // Direct cast and dereference to test pointer type tracking
        int val = *(int*)(args);
        args = args + 8;  // Manual advance by 8 bytes
        total = total + val;
    }

    __builtin_va_end(args);
    return total;
}

int main(void) {
    printf("=== Simple Variadic Test ===\n");

    int result = sum_ints(3, 10, 20, 30);
    printf("sum_ints(3, 10, 20, 30) = %d (expected 60)\n", result);

    if (result == 60) {
        printf("PASS\n");
        return 0;
    } else {
        printf("FAIL\n");
        return 1;
    }
}
