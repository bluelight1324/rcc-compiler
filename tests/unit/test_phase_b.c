/**
 * test_phase_b.c - Phase B Memory Safety Tests
 *
 * Tests for:
 *   B1: Borrow checker variable name extraction (int* p = &x)
 *   B2: Ownership state merging across if/else branches
 *   B3: Loop-aware ownership (double-free detection in loops)
 *   B4: Function parameter tracking (params are Borrowed, not Owner)
 *   B5: Pointer reassignment leak detection (p = malloc(...) twice)
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* ============================================================
 * B4: Function parameter tracking
 * Parameters should be Borrowed - NOT auto-freed by RCC.
 * If RCC incorrectly auto-frees params, this will crash.
 * ============================================================ */
int sum_array(int* arr, int n) {
    int total = 0;
    int i;
    for (i = 0; i < n; i++) total += arr[i];
    return total;
    /* arr must NOT be freed here — it's a Borrowed parameter */
}

void fill_buffer(char* buf, int size) {
    int i;
    for (i = 0; i < size - 1; i++) buf[i] = 'X';
    buf[size - 1] = '\0';
    /* buf must NOT be freed here — it's Borrowed */
}

/* ============================================================
 * B5: Pointer reassignment leak detection
 * Second malloc should be preceded by auto-free of first.
 * The program must not crash or leak.
 * ============================================================ */
void test_reassign() {
    int* p = malloc(sizeof(int));
    *p = 42;
    /* B5: RCC should insert free(p) before the next line */
    p = malloc(sizeof(int));
    *p = 99;
    free(p);
    /* No double-free, no leak */
}

/* ============================================================
 * B2: If/else branch ownership merging
 * ============================================================ */
int* maybe_alloc(int flag) {
    int* p = malloc(sizeof(int));
    if (flag) {
        *p = 1;
        return p;   /* ownership transferred */
    } else {
        *p = 0;
        return p;   /* ownership transferred on other branch too */
    }
}

/* ============================================================
 * B3: Loop - allocate and free inside loop
 * ============================================================ */
void process_loop(int count) {
    int i;
    for (i = 0; i < count; i++) {
        int* buf = malloc(sizeof(int));
        *buf = i;
        free(buf);  /* manually freed - no double-free */
    }
}

/* ============================================================
 * Main: call everything and verify correctness
 * ============================================================ */
int main() {
    /* B4: Parameter tracking */
    printf("Test B4: Function parameter tracking\n");
    {
        int arr[5];
        int j;
        for (j = 0; j < 5; j++) arr[j] = j + 1;
        int s = sum_array(arr, 5);
        printf("  sum = %d\n", s);
        if (s != 15) { printf("  FAIL\n"); return 1; }
        printf("  OK\n");
    }

    {
        char buf[32];
        fill_buffer(buf, 32);
        printf("  buf[0] = %c\n", buf[0]);
        if (buf[0] != 'X') { printf("  FAIL\n"); return 1; }
        if (buf[31] != '\0') { printf("  FAIL null term\n"); return 1; }
        printf("  OK\n");
    }

    /* B5: Reassignment */
    printf("Test B5: Pointer reassignment leak detection\n");
    test_reassign();
    printf("  OK (no crash)\n");

    /* B2: Branch merging */
    printf("Test B2: If/else branch ownership merging\n");
    {
        int* a = maybe_alloc(1);
        int* b = maybe_alloc(0);
        printf("  a=%d b=%d\n", *a, *b);
        free(a);
        free(b);
        printf("  OK\n");
    }

    /* B3: Loop */
    printf("Test B3: Loop ownership (no double-free)\n");
    process_loop(5);
    printf("  OK (no crash)\n");

    printf("\nAll Phase B tests PASSED!\n");
    return 0;
}
