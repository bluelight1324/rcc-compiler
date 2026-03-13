/* test_const_prop_inc.c - W3: ++/-- invalidation in const_locals_ */
#include <stdio.h>

int main() {
    int x = 5;
    /* x is in const_locals_ after initialization */
    printf("%d\n", x);   /* 5 */

    /* ++x should invalidate const_locals_[x] */
    ++x;
    /* After ++, x must be loaded from stack, not folded as 5 */
    printf("%d\n", x);   /* 6 */

    /* x-- should also invalidate */
    x--;
    printf("%d\n", x);   /* 5 */

    /* y is const-propagated; y++ increments it */
    int y = 10;
    y++;
    printf("%d\n", y);   /* 11 */

    return 0;
}
