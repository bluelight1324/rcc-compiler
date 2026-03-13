/**
 * RCC Version 4.0 - Memory Safety Test
 * Test Case: Memory Leak Detection
 */

#include <stdlib.h>
#include <stdio.h>

void leak() {
    int* ptr = (int*)malloc(sizeof(int));
    *ptr = 42;
    printf("Allocated memory in leak()\n");
    // ⚠️  Memory leak: ptr goes out of scope without free()
    return;
}

void no_leak() {
    int* ptr = (int*)malloc(sizeof(int));
    *ptr = 100;
    printf("Allocated memory in no_leak()\n");
    free(ptr);  // ✓ Properly freed
    return;
}

int main() {
    printf("=== RCC v4.0 Memory Safety Test ===\n");
    printf("Test: Memory Leak Detection\n\n");

    leak();     // ⚠️  WARNING: Memory leak
    no_leak();  // ✓ No leak

    return 0;
}

/*
 * Expected RCC v4.0 Output:
 *
 * <input>:13: warning: Memory leak: variable 'ptr' goes out of scope without being freed
 *
 * 0 error(s), 1 warning(s) generated.
 */
