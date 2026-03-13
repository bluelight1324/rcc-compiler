// Debug test - add VLA and varargs
extern int printf(const char* fmt, ...);

int test_vla(int n) {
    int arr[n];
    int i;
    for (i = 0; i < n; i = i + 1) {
        arr[i] = i + 1;
    }
    int sum = 0;
    for (i = 0; i < n; i = i + 1) {
        sum = sum + arr[i];
    }
    return sum;
}

int sum_ints(int count, ...) {
    va_list args;
    va_start(args, count);
    int total = 0;
    int i;
    for (i = 0; i < count; i = i + 1) {
        int val = va_arg(args);
        total = total + val;
    }
    va_end(args);
    return total;
}

int main(void) {
    printf("Starting debug test...\n");

    // Test typeof
    int x = 42;
    int sx = sizeof(typeof(x));
    printf("typeof test: sizeof(typeof(x)) = %d\n", sx);

    // Test auto
    auto a = 100;
    printf("auto test: a = %d\n", a);

    // Test auto with pointer
    auto ptr = &a;
    *ptr = 200;
    printf("auto ptr test: a = %d\n", a);

    // Test constexpr
    constexpr int SIZE = 5;
    int arr[SIZE];
    arr[0] = SIZE;
    printf("constexpr test: SIZE = %d, arr[0] = %d\n", SIZE, arr[0]);

    // Test VLA
    printf("Testing VLA...\n");
    int result = test_vla(5);
    printf("VLA test: sum(1..5) = %d\n", result);

    // Test varargs
    printf("Testing varargs...\n");
    int sum = sum_ints(3, 10, 20, 30);
    printf("varargs test: sum(10,20,30) = %d\n", sum);

    printf("Debug test done!\n");
    return 0;
}
