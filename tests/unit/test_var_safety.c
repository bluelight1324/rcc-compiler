/*
 * Test: auto keyword with Rust Safety Analysis
 * This test should trigger safety warnings
 */

#include <stdlib.h>

int main() {
    // Test 1: Use-after-free detection with auto
    auto ptr = malloc(sizeof(int));
    *ptr = 42;
    free(ptr);
    *ptr = 100;  // BUG: Use-after-free - should be detected!

    return 0;
}
