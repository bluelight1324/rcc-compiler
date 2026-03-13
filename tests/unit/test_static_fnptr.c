/* test_static_fnptr.c
 * Quality 3.1: static function-pointer initializers (RCC task 8.2)
 *
 * Tests that global variables initialized with function addresses
 * correctly emit DQ OFFSET fn in the generated ASM, not DQ 0.
 * This was the root cause of cJSON's global_hooks crash (task 7.59 bug #10).
 *
 * Expected: exit code 0, no errors, "static_fnptr: 4/4 passed"
 */
#include <stdio.h>
#include <stdlib.h>

/* Simple math helpers — not static so they are EXTERN-resolvable globally */
int fn_add(int a, int b) { return a + b; }
int fn_mul(int a, int b) { return a * b; }
int fn_sub(int a, int b) { return a - b; }

/* Struct with function-pointer members */
struct math_ops {
    int (*add)(int, int);
    int (*mul)(int, int);
};

/* Global struct initialized with function addresses.
 * Without fix: emits DQ 0, DQ 0 → NULL fn ptrs → crash
 * With fix:    emits DQ OFFSET fn_add, DQ OFFSET fn_mul → correct */
struct math_ops g_ops = { fn_add, fn_mul };

/* Global scalar function pointer initialized with function address */
int (*g_fn)(int, int) = fn_sub;

int main(void) {
    int pass = 0;

    /* Test 1: struct fn-ptr member [add] initialized correctly */
    if (g_ops.add(3, 7) == 10) {
        pass++;
    } else {
        fprintf(stderr, "FAIL 1: g_ops.add(3,7) = %d (expected 10)\n", g_ops.add(3, 7));
    }

    /* Test 2: struct fn-ptr member [mul] initialized correctly */
    if (g_ops.mul(4, 5) == 20) {
        pass++;
    } else {
        fprintf(stderr, "FAIL 2: g_ops.mul(4,5) = %d (expected 20)\n", g_ops.mul(4, 5));
    }

    /* Test 3: scalar global fn-ptr initialized correctly */
    if (g_fn(10, 3) == 7) {
        pass++;
    } else {
        fprintf(stderr, "FAIL 3: g_fn(10,3) = %d (expected 7)\n", g_fn(10, 3));
    }

    /* Test 4: can reassign and use updated fn-ptr */
    g_ops.add = fn_mul;
    if (g_ops.add(3, 4) == 12) {
        pass++;
    } else {
        fprintf(stderr, "FAIL 4: reassigned g_ops.add(3,4) = %d (expected 12)\n", g_ops.add(3, 4));
    }

    printf("static_fnptr: %d/4 passed\n", pass);
    return (pass == 4) ? 0 : 1;
}
