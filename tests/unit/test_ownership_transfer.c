/**
 * test_ownership_transfer.c - Ownership Transfer Tests
 * Verifies that RCC does NOT free() pointers that are returned to callers.
 *
 * Previously: functions returning malloc'd pointers would get a free()
 * inserted before the return AND at scope exit -> double free / crash!
 *
 * After fix: returning an OWNER pointer marks it as "transferred" and
 * suppresses auto-free for that variable.
 */

#include <stdlib.h>
#include <stdio.h>

// ============================================================
// Case 1: Simple malloc-and-return (must NOT be freed!)
// ============================================================
int* make_int(int value) {
    int* p = malloc(sizeof(int));
    *p = value;
    return p;  // ownership transferred - p must NOT be freed here
}

// ============================================================
// Case 2: Multiple allocations, one returned
// ============================================================
char* make_string(int size) {
    char* buf = malloc(size);
    buf[0] = 'R';
    buf[1] = 'C';
    buf[2] = 'C';
    buf[3] = '\0';
    return buf;  // buf transferred; must not be freed
}

// ============================================================
// Case 3: Conditional return (only one path transfers ownership)
// ============================================================
int* make_positive(int value) {
    int* p = malloc(sizeof(int));
    *p = value;

    if (value <= 0) {
        // NOT returning p - it should be freed here
        return NULL;
    }

    return p;  // ownership transferred
}

// ============================================================
// Main: call the functions and verify they work
// ============================================================
int main() {
    printf("Test 1: make_int\n");
    int* a = make_int(42);
    printf("  value = %d\n", *a);
    free(a);  // caller owns it and is responsible for freeing
    printf("  OK\n");

    printf("Test 2: make_string\n");
    char* s = make_string(32);
    printf("  string = %s\n", s);
    free(s);
    printf("  OK\n");

    printf("Test 3: make_positive (positive value)\n");
    int* b = make_positive(10);
    if (b) {
        printf("  value = %d\n", *b);
        free(b);
        printf("  OK\n");
    }

    printf("Test 4: make_positive (negative value - returns NULL)\n");
    int* c = make_positive(-5);
    if (c == NULL) {
        printf("  returned NULL as expected\n");
        printf("  OK\n");
    }

    printf("\nAll ownership transfer tests PASSED!\n");
    return 0;
}
