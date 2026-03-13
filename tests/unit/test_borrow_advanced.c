/* test_borrow_advanced.c
 * Two borrows of the same variable followed by a write through one pointer.
 * W2b mutable-alias detection must fire.
 * Expected: exit 1 with "Cannot write through" in stderr.
 */
#include <stdio.h>

int main() {
    int x = 10;
    int* p = &x;    /* borrow 1 */
    int* q = &x;    /* borrow 2 — x now has two active borrows */
    *p = 20;        /* ERROR: x is also borrowed by q */
    printf("%d\n", *q);
    return 0;
}
