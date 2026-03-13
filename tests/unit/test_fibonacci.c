int printf(char *fmt, ...);

int fibonacci(int n) {
    int a;
    int b;
    int temp;
    int i;

    if (n <= 0) return 0;
    if (n == 1) return 1;

    a = 0;
    b = 1;
    i = 2;
    while (i <= n) {
        temp = a + b;
        a = b;
        b = temp;
        i = i + 1;
    }
    return b;
}

int main() {
    int i;
    printf("Fibonacci sequence (first 20 terms):\n");
    i = 0;
    while (i < 20) {
        printf("fib(%d) = %d\n", i, fibonacci(i));
        i = i + 1;
    }
    return 0;
}
