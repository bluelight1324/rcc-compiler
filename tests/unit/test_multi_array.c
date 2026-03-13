// Test multi-dimensional arrays
extern int printf(const char* fmt, ...);

int main(void) {
    int arr[3][4];
    int i;
    int j;

    printf("Testing multi-dim arrays...\n");

    // Initialize
    for (i = 0; i < 3; i = i + 1) {
        for (j = 0; j < 4; j = j + 1) {
            arr[i][j] = i * 4 + j;
        }
    }

    printf("arr[1][2] = %d (expected 6)\n", arr[1][2]);
    printf("arr[2][3] = %d (expected 11)\n", arr[2][3]);

    return 0;
}
