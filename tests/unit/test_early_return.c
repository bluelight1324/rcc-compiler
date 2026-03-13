/**
 * test_early_return.c - Critical Test for Early Return Handling
 * This is a CRITICAL memory safety test
 */

#include <stdlib.h>

// CRITICAL: Early return must free() BEFORE returning
void test_early_return() {
    int* p = malloc(100);
    *p = 42;

    if (*p == 42) {
        return;  // MUST insert free(p) HERE, not at end of function!
    }

    // This line should not be reached
    *p = 99;
}

int main() {
    test_early_return();
    return 0;
}
