/* test_borrow_write_ok.c
 * Single borrow + write through it — no conflict, should compile clean (exit 0).
 */
#include <stdio.h>

int main() {
    int x = 10;
    int* p = &x;    /* single borrow of x */
    *p = 20;        /* only one borrow active — no conflict */
    printf("%d\n", *p);

    /* Borrow expires, new borrow, write again */
    {
        int y = 5;
        int* q = &y;
        *q = 99;
        printf("%d\n", *q);
    }

    return 0;
}
