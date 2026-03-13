/* test_c11_string_lib.c
 * RCC Task 7.38 — C11 <string.h> library function runtime test.
 *
 * Tests functions present in C11 string.h (§7.24) including:
 *   L1: strnlen (C11 Annex K / POSIX)
 *   L2: strstr
 *   L3: strspn / strcspn
 *   L4: strtok
 *   L5: memchr
 *   L6: memcmp
 *   L7: strerror (basic check)
 *
 * Expected: RCC compile + run → exit 0.
 */
#include <stdio.h>
#include <string.h>

static int g_failures = 0;
static void check(const char* name, int cond) {
    if (!cond) { printf("FAIL: %s\n", name); g_failures++; }
}

int main(void) {
    printf("=== C11 String Library Test ===\n");

    /* ── L1: strnlen ─────────────────────────────────────────────────────── */
    check("strnlen('hello',10)==5", (int)strnlen("hello", 10) == 5);
    check("strnlen('hello',3)==3",  (int)strnlen("hello", 3)  == 3);
    check("strnlen('hello',0)==0",  (int)strnlen("hello", 0)  == 0);
    check("strnlen('',10)==0",      (int)strnlen("", 10)      == 0);

    /* ── L2: strstr ──────────────────────────────────────────────────────── */
    const char* hs = "hello world";
    check("strstr found 'world'",  strstr(hs, "world") != 0);
    check("strstr found 'hello'",  strstr(hs, "hello") != 0);
    check("strstr not found 'xyz'",strstr(hs, "xyz")   == 0);
    check("strstr empty needle",   strstr(hs, "")      == hs);

    /* ── L3: strspn / strcspn ────────────────────────────────────────────── */
    check("strspn  all match ==3", (int)strspn("abc", "abc")     == 3);
    check("strspn  no match  ==0", (int)strspn("xyz", "abc")     == 0);
    check("strspn  partial   ==2", (int)strspn("abXY", "ab")     == 2);
    check("strcspn 'hello '  ==5", (int)strcspn("hello world"," ")== 5);
    check("strcspn no sep    ==3", (int)strcspn("abc", "xyz")    == 3);

    /* ── L4: strtok ──────────────────────────────────────────────────────── */
    char str[] = "one,two,three";
    char* tok = strtok(str, ",");
    check("strtok #1 == 'one'",   tok != 0 && strcmp(tok, "one")   == 0);
    tok = strtok(0, ",");
    check("strtok #2 == 'two'",   tok != 0 && strcmp(tok, "two")   == 0);
    tok = strtok(0, ",");
    check("strtok #3 == 'three'", tok != 0 && strcmp(tok, "three") == 0);
    tok = strtok(0, ",");
    check("strtok #4 == NULL",    tok == 0);

    /* ── L5: memchr ──────────────────────────────────────────────────────── */
    const char* s = "abcdef";
    check("memchr finds 'c'", memchr(s, 'c', 6) != 0);
    check("memchr no 'z'",    memchr(s, 'z', 6) == 0);

    /* ── L6: memcmp ──────────────────────────────────────────────────────── */
    check("memcmp eq  == 0", memcmp("abc", "abc", 3) == 0);
    check("memcmp lt  <  0", memcmp("abc", "abd", 3) <  0);
    check("memcmp gt  >  0", memcmp("abd", "abc", 3) >  0);

    /* ── L7: strerror (basic — just check non-null) ──────────────────────── */
    char* err = strerror(0);  /* 0 = no error, typically "No error" or similar */
    check("strerror(0) != NULL", err != 0);

    if (g_failures == 0) printf("ALL STRING CHECKS PASSED\n");
    else printf("%d STRING CHECK(S) FAILED\n", g_failures);
    return g_failures;
}
