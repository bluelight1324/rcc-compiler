// Test 3-dimensional array access
extern int printf(const char* fmt, ...);

int main(void) {
    char grid[4][3][2];  // 3D array: 4 planes, 3 rows, 2 cols
    int i;
    int j;
    int k;
    int passed;

    printf("=== 3D Array Test ===\n");

    // Initialize: grid[i][j][k] = i*100 + j*10 + k
    for (i = 0; i < 4; i++) {
        for (j = 0; j < 3; j++) {
            for (k = 0; k < 2; k++) {
                grid[i][j][k] = (char)(i * 100 + j * 10 + k);
            }
        }
    }

    // Verify specific elements
    passed = 1;

    if (grid[0][0][0] != 0) {
        printf("FAIL: grid[0][0][0] = %d, expected 0\n", grid[0][0][0]);
        passed = 0;
    }

    if (grid[1][2][1] != 121) {
        printf("FAIL: grid[1][2][1] = %d, expected 121\n", grid[1][2][1]);
        passed = 0;
    }

    if (grid[3][2][1] != 65) {  // 321 % 256 = 65
        printf("FAIL: grid[3][2][1] = %d, expected 65\n", grid[3][2][1]);
        passed = 0;
    }

    if (passed) {
        printf("PASSED: 3D arrays work correctly!\n");
        printf("  grid[0][0][0] = %d\n", grid[0][0][0]);
        printf("  grid[1][2][1] = %d\n", grid[1][2][1]);
        printf("  grid[3][2][1] = %d\n", grid[3][2][1]);
        return 0;
    } else {
        printf("FAILED: 3D array access broken\n");
        return 1;
    }
}
