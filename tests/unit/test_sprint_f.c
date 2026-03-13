/* test_sprint_f.c — Sprint F: P0 ABI correctness tests
 * Tests struct arg passing (<=8B and >8B by value), float vararg fix,
 * large struct return (>16B hidden pointer), and #pragma pack.
 */
#include <stdio.h>
#include <stdlib.h>

/* ── Test 1: Struct ≤8 bytes by value ───────────────────────────────────── */
struct Point { int x; int y; };   /* 8 bytes exactly */

static int point_sum(struct Point p) {
    return p.x + p.y;
}

static int point_diff(struct Point p) {
    return p.x - p.y;
}

/* ── Test 2: Struct >8 bytes by value (12 bytes) ────────────────────────── */
struct Vec3 { int x; int y; int z; };   /* 12 bytes */

static int vec3_sum(struct Vec3 v) {
    return v.x + v.y + v.z;
}

static int vec3_dot(struct Vec3 a, struct Vec3 b) {
    return a.x*b.x + a.y*b.y + a.z*b.z;
}

/* ── Test 3: Large struct return >16 bytes ──────────────────────────────── */
struct Big5 { int a; int b; int c; int d; int e; };   /* 20 bytes */

static struct Big5 make_big5(int n) {
    struct Big5 r;
    r.a = n;
    r.b = n + 1;
    r.c = n + 2;
    r.d = n + 3;
    r.e = n + 4;
    return r;
}

/* ── Test 4: #pragma pack(1) — packed struct ────────────────────────────── */
#pragma pack(1)
struct Packed3 { char a; int b; char c; };   /* packed: 6 bytes, no padding */
#pragma pack()

/* ── main ────────────────────────────────────────────────────────────────── */
int main(void) {
    int fails = 0;

    /* --- Test 1a: struct ≤8B by value --- */
    struct Point p;
    p.x = 3;
    p.y = 7;
    int s1 = point_sum(p);
    if (s1 != 10) {
        fprintf(stderr, "FAIL test1a: point_sum=%d (expected 10)\n", s1);
        fails++;
    }

    int d1 = point_diff(p);
    if (d1 != -4) {
        fprintf(stderr, "FAIL test1b: point_diff=%d (expected -4)\n", d1);
        fails++;
    }

    /* --- Test 2a: struct >8B by value --- */
    struct Vec3 v;
    v.x = 1; v.y = 2; v.z = 3;
    int s2 = vec3_sum(v);
    if (s2 != 6) {
        fprintf(stderr, "FAIL test2a: vec3_sum=%d (expected 6)\n", s2);
        fails++;
    }

    /* --- Test 2b: two struct >8B args --- */
    struct Vec3 a2, b2;
    a2.x = 1; a2.y = 2; a2.z = 3;
    b2.x = 4; b2.y = 5; b2.z = 6;
    int dot = vec3_dot(a2, b2);
    if (dot != 32) {   /* 1*4+2*5+3*6 = 4+10+18 = 32 */
        fprintf(stderr, "FAIL test2b: vec3_dot=%d (expected 32)\n", dot);
        fails++;
    }

    /* --- Test 3: large struct return >16B (non-crash) --- */
    /* Discard result — tests that hidden-pointer mechanism doesn't crash */
    make_big5(100);

    /* --- Test 3b: float in fprintf (va_list float fix) --- */
    /* Just test non-crash; output not checked by runner */
    fprintf(stderr, "float_check: %.2f\n", 1.25);

    /* --- Test 4: sizeof packed struct --- */
    int packed_sz = (int)sizeof(struct Packed3);
    if (packed_sz != 6) {
        fprintf(stderr, "FAIL test4: sizeof(Packed3)=%d (expected 6)\n", packed_sz);
        fails++;
    }

    if (fails == 0) {
        fprintf(stderr, "sprint_f: all %d checks passed\n", 4);
        return 0;
    }
    return 1;
}
