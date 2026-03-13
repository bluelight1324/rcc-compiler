// Test VLA (Variable Length Arrays)
extern int printf(const char* fmt, ...);

int test_vla(int n) {
    int arr[n];
    int i;
    for (i = 0; i < n; i = i + 1) {
        arr[i] = i * 10;
    }
    int sum = 0;
    for (i = 0; i < n; i = i + 1) {
        sum = sum + arr[i];
    }
    printf("VLA test: n=%d, sum=%d\n", n, sum);
    return sum == (n-1)*n*5; // sum of 0+10+20+...+(n-1)*10 = 10*(n-1)*n/2 = 5*n*(n-1)
}

int main(void) {
    int passed = 0;
    printf("=== VLA Test ===\n");
    if (test_vla(5)) passed = passed + 1;
    printf("\nPassed: %d/1\n", passed);
    return 1 - passed;
}
