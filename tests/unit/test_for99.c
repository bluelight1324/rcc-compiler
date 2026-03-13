// Test C99 for-loop declaration
extern int printf(const char* fmt, ...);

int main(void) {
    printf("Testing C99 for-loop...\n");

    int sum = 0;
    for (int i = 0; i < 5; i = i + 1) {
        sum = sum + i;
    }
    printf("sum = %d\n", sum);

    return 0;
}
