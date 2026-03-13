/**
 * RCC Version 4.0 - Memory Safety Test
 * Test Case: Borrow Checking (Mutable Aliasing)
 */

#include <stdio.h>

void safe_borrow() {
    int x = 10;
    int* p = &x;     // ✓ Shared borrow
    int* q = &x;     // ✓ Multiple shared borrows OK
    printf("x = %d, *p = %d, *q = %d\n", x, *p, *q);
}

void unsafe_mutable_borrow() {
    int x = 10;
    int* p = &x;     // Borrow 1
    int* q = &x;     // Borrow 2
    *p = 20;         // ⚠️  Mutable access through p
    *q = 30;         // ❌ ERROR: Conflicting mutable access
}

int main() {
    printf("=== RCC v4.0 Memory Safety Test ===\n");
    printf("Test: Borrow Checking\n\n");

    printf("Safe borrow test:\n");
    safe_borrow();   // ✓ OK

    printf("\nUnsafe mutable borrow test:\n");
    unsafe_mutable_borrow();  // ❌ ERROR

    return 0;
}

/*
 * Expected RCC v4.0 Output:
 *
 * <input>:20: error: Cannot use 'x': mutably borrowed by 'p'
 *
 * 1 error(s), 0 warning(s) generated.
 *
 * Note: This is conservative analysis - RCC may not catch all cases
 *       in the first implementation, but will improve over time.
 */
