// Test multi-dimensional array access
extern int printf(const char* fmt, ...);

int main(void) {
    int matrix[3][4];
    int i;
    int j;
    int val;
    int passed;

    printf("=== Multi-Dimensional Array Test ===\n");

    // Initialize 3x4 matrix: matrix[i][j] = i*10 + j
    val = 0;
    for (i = 0; i < 3; i++) {
        for (j = 0; j < 4; j++) {
            matrix[i][j] = i * 10 + j;
        }
    }

    // Print and verify
    passed = 1;
    for (i = 0; i < 3; i++) {
        for (j = 0; j < 4; j++) {
            printf("matrix[%d][%d] = %d", i, j, matrix[i][j]);
            if (matrix[i][j] != i * 10 + j) {
                printf(" WRONG (expected %d)", i * 10 + j);
                passed = 0;
            }
            printf("\n");
        }
    }

    if (passed) {
        printf("\nPASSED: Multi-dim arrays work!\n");
    } else {
        printf("\nFAILED!\n");
    }
    return passed ? 0 : 1;
}
