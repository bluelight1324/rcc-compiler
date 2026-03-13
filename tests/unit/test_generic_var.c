/* test_generic_var.c
 * Tests _Generic dispatch using variable controlling expressions.
 * This exercises the var_types_ map added in task 7.53, which tracks
 * simple-type variable declarations so _Generic can dispatch on them
 * correctly (not just on literals/casts).
 *
 * Expected: RCC exit 0, no warnings.
 */
#include <stdio.h>
#include <string.h>

#define TNAME(x)  _Generic((x), int:"int", double:"double", float:"float", default:"other")

int main(void) {
    double d = 3.14;
    float  f = 1.5f;
    int    n = 42;

    /* Variable dispatch — requires var_types_ map (added in task 7.53) */
    if (strcmp(TNAME(d), "double") != 0) {
        printf("FAIL: double dispatch: got '%s', expected 'double'\n", TNAME(d));
        return 1;
    }
    if (strcmp(TNAME(f), "float") != 0) {
        printf("FAIL: float dispatch: got '%s', expected 'float'\n", TNAME(f));
        return 2;
    }
    if (strcmp(TNAME(n), "int") != 0) {
        printf("FAIL: int dispatch: got '%s', expected 'int'\n", TNAME(n));
        return 3;
    }

    /* Also verify literal dispatch still works */
    if (strcmp(TNAME(1.0),  "double") != 0) { printf("FAIL: literal double\n"); return 4; }
    if (strcmp(TNAME(1.0f), "float")  != 0) { printf("FAIL: literal float\n");  return 5; }
    if (strcmp(TNAME(42),   "int")    != 0) { printf("FAIL: literal int\n");    return 6; }

    printf("_Generic variable dispatch PASS\n");
    return 0;
}
