// Test: # (stringify) operator correctly escapes double-quotes
// and __LINE__/__FILE__ predefined macros work correctly.
// C11 ss6.10.3.2 and ss6.10.9.1
extern int printf(const char* fmt, ...);
extern int strcmp(const char *s1, const char *s2);
extern unsigned long strlen(const char *s);

#define STRINGIFY(x) #x

/* check_contains_dquote: returns 1 if string contains a double-quote (ASCII 34).
   This verifies that STRINGIFY properly embedded the " from the macro argument.
   The preprocessor must escape it as \", and after C lexing the runtime
   string will contain the actual " character (ASCII 34). */
int check_contains_dquote(const char *s) {
    int i = 0;
    while (s[i]) {
        if (s[i] == 34) return 1;
        i++;
    }
    return 0;
}

int main(void) {
    int fail = 0;
    printf("=== Stringify Quotes Test ===\n");

    /* Test 1: basic stringify */
    if (strcmp(STRINGIFY(hello), "hello") != 0) {
        printf("FAIL: basic stringify\n");
        fail++;
    }

    /* Test 2: stringify of expression containing double-quote.
       STRINGIFY(strcmp(x,"m")==0) must produce a valid string.
       At runtime the result should contain a double-quote (ASCII 34),
       proving the preprocessor escaped it correctly as \" in the token. */
    const char *s2 = STRINGIFY(strcmp(x,"m")==0);
    if (!check_contains_dquote(s2)) {
        printf("FAIL: embedded quote not in stringified result\n");
        fail++;
    }

    /* Test 3: __FILE__ is defined and non-empty */
    if (strlen(__FILE__) == 0) {
        printf("FAIL: __FILE__ empty\n");
        fail++;
    }

    /* Test 4: __LINE__ is a positive number */
    int ln = __LINE__;
    if (ln <= 0) {
        printf("FAIL: __LINE__ not positive\n");
        fail++;
    }

    if (fail == 0) {
        printf("__FILE__ = %s\n", __FILE__);
        printf("__LINE__ = %d\n", ln);
        printf("ALL STRINGIFY TESTS PASSED\n");
    }
    return fail;
}
