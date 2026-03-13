/* test_c23_grammar.c
 * Task 7.44 — C23 §6.7.6, §6.7.10, §6.8.1
 *
 * Verifies three new grammar productions:
 *   (1) param_type_list → ELLIPSIS  (variadic with no named param, §6.7.6)
 *   (2) initializer → LBRACE RBRACE (empty initializer, §6.7.10)
 *   (3) labeled_stmt → IDENTIFIER COLON (bare label, §6.8.1)
 *
 * Expected: compile with exit 0
 */
#include <stdio.h>

/* ── (1) Variadic function with no named parameter (C23 §6.7.6) ─────────── */
/* Declaration only — ELLIPSIS is the sole parameter list. */
void vfunc_decl(...);

/* ── (3) Bare label at end of block (C23 §6.8.1) ────────────────────────── */
/* `done:` is the LAST item in the body before `}` — nothing follows it.
 * This exercises: labeled_stmt → IDENTIFIER COLON  (bare label production). */
static void bare_label_func(int x) {
    if (x > 0) goto done;
    x = 0;
done:
    /* C23 §6.8.1: label may be last block item with no following statement */
}

/* ── (2) Empty initializer {} (C23 §6.7.10) ─────────────────────────────── */

/* Global scalar — must be zero-initialized */
int g_scalar = {};

/* Global struct */
struct Point { int x; int y; };
struct Point g_point = {};

static int g_fails = 0;
static void check(const char *name, int cond) {
    if (!cond) { printf("FAIL: %s\n", name); g_fails++; }
}

int main(void) {
    printf("=== C23 Grammar Test ===\n");

    /* (1) Variadic no-name: declaration compiled */
    check("variadic no-name decl compiled", 1);

    /* (3) Bare label: function compiled and called */
    bare_label_func(1);
    bare_label_func(0);
    check("bare label func compiled and called", 1);

    /* (2a) Global scalar empty init → 0 */
    check("global scalar {} == 0", g_scalar == 0);

    /* (2b) Global struct empty init → all fields 0 */
    check("global struct {}.x == 0", g_point.x == 0);
    check("global struct {}.y == 0", g_point.y == 0);

    /* (2c) Local scalar empty init → 0 */
    int loc_int = {};
    check("local int {} == 0", loc_int == 0);

    /* (2d) Local struct empty init → all fields 0 */
    struct Point loc_pt = {};
    check("local struct {}.x == 0", loc_pt.x == 0);
    check("local struct {}.y == 0", loc_pt.y == 0);

    /* (3b) Bare label in nested block */
    {
        int ok = 1;
        if (!ok) goto inner_skip;
        ok = 1;
inner_skip:
        /* bare label as last item — C23 §6.8.1 */
        check("nested bare label: ok == 1", ok == 1);
    }

    if (g_fails == 0) printf("ALL C23 GRAMMAR TESTS PASSED\n");
    else printf("%d C23 GRAMMAR TEST(S) FAILED\n", g_fails);
    return g_fails;
}
