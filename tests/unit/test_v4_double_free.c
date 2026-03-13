/**
 * RCC Version 4.0 - Memory Safety Test
 * Test Case: Double-Free Detection
 */

#include <stdlib.h>
#include <stdio.h>

int main() {
    printf("=== RCC v4.0 Memory Safety Test ===\n");
    printf("Test: Double-Free Detection\n\n");

    int* ptr = (int*)malloc(sizeof(int));
    *ptr = 42;
    printf("✓ Allocated memory: ptr = %p\n", ptr);

    free(ptr);
    printf("✓ Freed memory (first time)\n");

    // Double-free (should be detected)
    printf("\n⚠️  Attempting double-free...\n");
    free(ptr);  // ❌ ERROR: Double-free!
    printf("✗ This line should not execute\n");

    return 0;
}

/*
 * Expected RCC v4.0 Output:
 *
 * <input>:20: error: Double-free detected: variable 'ptr' already freed
 *
 * 1 error(s), 0 warning(s) generated.
 */
