/* test_c11_phase3.c
 * RCC Task 72.24 — C11 Phase 3 compliance (compile-only test).
 *
 * Tests:
 *   G9: Anonymous struct/union field promotion + duplicate-field detection
 *   G10: _Thread_local keyword; <threads.h> include
 *
 * Expected: RCC exit 0 (compile success).
 * G10 _Thread_local emits a warning containing "not supported" to stderr.
 */
#include <stdio.h>
#include <threads.h>   /* C11 §7.26 — stub types */

/* G9: Anonymous struct — y and z promoted into outer struct namespace */
struct Point3D {
    int x;
    struct {
        int y;
        int z;
    };
};

/* G9: Anonymous union inside a struct — fields promoted */
struct Variant {
    int tag;
    union {
        int   as_int;
        float as_float;
    };
};

/* G9: Anonymous struct inside a union */
union Rect {
    struct {
        int left;
        int top;
        int right;
        int bottom;
    };
    int raw[4];
};

/* G10: _Thread_local — should parse (warning emitted but no error) */
_Thread_local int tls_counter = 0;

/* G10: threads.h types — mtx_t, once_flag, ONCE_FLAG_INIT */
static mtx_t   g_mutex;
static once_flag g_once = ONCE_FLAG_INIT;

int main(void) {
    /* G9: Access promoted fields from anonymous struct */
    struct Point3D p;
    p.x = 1;
    p.y = 2;
    p.z = 3;
    printf("Point3D: x=%d y=%d z=%d\n", p.x, p.y, p.z);

    /* G9: Access promoted union fields */
    struct Variant v;
    v.tag    = 1;
    v.as_int = 42;
    printf("Variant int: %d\n", v.as_int);

    /* G9: Anonymous struct in union */
    union Rect r;
    r.left = 10;  r.top = 20;  r.right = 310;  r.bottom = 220;
    printf("Rect: %d %d %d %d\n", r.left, r.top, r.right, r.bottom);

    /* G10: Use _Thread_local variable */
    tls_counter = 7;
    printf("TLS counter: %d\n", tls_counter);

    /* G10: types from threads.h compile OK */
    (void)g_mutex;
    (void)g_once;

    return 0;
}
