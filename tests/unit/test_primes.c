int printf(char *fmt, ...);

int is_prime(int n) {
    int i;
    if (n < 2) return 0;
    if (n == 2) return 1;
    if (n % 2 == 0) return 0;
    i = 3;
    while (i * i <= n) {
        if (n % i == 0) return 0;
        i = i + 2;
    }
    return 1;
}

int main() {
    int n;
    int count;

    printf("Prime numbers up to 100:\n");
    n = 2;
    count = 0;
    while (n <= 100) {
        if (is_prime(n)) {
            printf("%d ", n);
            count = count + 1;
        }
        n = n + 1;
    }
    printf("\nTotal primes found: %d\n", count);
    return 0;
}
