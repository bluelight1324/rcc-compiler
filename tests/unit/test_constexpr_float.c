/* test_constexpr_float.c
 * C23 §6.7.1 — constexpr with floating-point initializer.
 *
 * C23 mandates that 'constexpr' works with floating-point constants.
 * RCC treats float constexprs as read-only local variables (correct
 * semantics for float arithmetic; no integer truncation).
 *
 * Expected: RCC exit 0, no "must have constant initializer" warnings.
 */
#include <stdio.h>

/* Integer constexpr — should still fold as compile-time constant */
constexpr int MAX = 100;
constexpr int HALF = MAX / 2;

int main(void) {
    constexpr double Pi = 3.14159;
    constexpr float Scale = 2.5f;

    /* Use float constexprs in arithmetic */
    double area = Pi * 4.0;
    float x = Scale * 2.0f;

    /* Use integer constexprs */
    int limit = MAX + HALF; /* 150 */

    printf("Pi=%.5f area=%.3f Scale=%.1f x=%.1f limit=%d\n",
           Pi, area, Scale, x, limit);

    return 0;
}
