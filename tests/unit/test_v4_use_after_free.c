/**
 * RCC Version 4.0 - Memory Safety Test
 * Test Case: Use-After-Free Detection
 *
 * This test demonstrates RCC's ability to detect use-after-free bugs
 * automatically without any syntax changes to C code.
 */

#include <stdlib.h>
#include <stdio.h>

int main() {
    printf("=== RCC v4.0 Memory Safety Test ===\n");
    printf("Test: Use-After-Free Detection\n\n");

    // Allocate memory
    int* ptr = (int*)malloc(sizeof(int));
    *ptr = 42;
    printf("✓ Allocated memory: ptr = %p, value = %d\n", ptr, *ptr);

    // Free the memory
    free(ptr);
    printf("✓ Freed memory\n");

    // Use-after-free (should be detected by RCC v4.0)
    printf("\n⚠️  Attempting use-after-free...\n");
    *ptr = 100;  // ❌ ERROR: Use-after-free!
    printf("✗ This line should not execute (use-after-free)\n");

    return 0;
}

/*
 * Expected RCC v4.0 Behavior:
 *
 * Compilation with safety enabled (default):
 *   $ rcc test_v4_use_after_free.c -c
 *
 * Output:
 *   Running memory safety analysis (level: minimal)...
 *
 *   Memory Safety Analysis Results:
 *   ────────────────────────────────
 *   <input>:24: error: Use-after-free: variable 'ptr' used after free()
 *
 *   1 error(s), 0 warning(s) generated.
 *
 *   Compilation aborted due to safety errors.
 *
 * Compilation with safety disabled:
 *   $ rcc --no-safety test_v4_use_after_free.c -c
 *
 * Output:
 *   Generated: test_v4_use_after_free.asm
 *   (compiles successfully, but program has undefined behavior)
 */
