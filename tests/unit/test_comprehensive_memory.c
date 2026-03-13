/**
 * test_comprehensive_memory.c - Comprehensive Memory Safety Tests
 * RCC v4.1.0 - Automatic Memory Management
 *
 * Tests all major memory management scenarios to identify safety issues
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ============================================================================
// TEST 1: Basic malloc/free
// ============================================================================
void test_1_basic_malloc() {
    printf("Test 1: Basic malloc\n");
    int* p = malloc(sizeof(int));
    *p = 42;
    printf("  Value: %d\n", *p);
    // Should auto-free
}

// ============================================================================
// TEST 2: Cast malloc
// ============================================================================
void test_2_cast_malloc() {
    printf("Test 2: Cast malloc\n");
    int* p = (int*)malloc(sizeof(int));
    *p = 100;
    printf("  Value: %d\n", *p);
    // Should auto-free
}

// ============================================================================
// TEST 3: Multiple allocations
// ============================================================================
void test_3_multiple_allocs() {
    printf("Test 3: Multiple allocations\n");
    int* a = malloc(sizeof(int));
    int* b = malloc(sizeof(int));
    int* c = malloc(sizeof(int));
    *a = 1;
    *b = 2;
    *c = 3;
    printf("  a=%d, b=%d, c=%d\n", *a, *b, *c);
    // Should auto-free c, b, a in LIFO order
}

// ============================================================================
// TEST 4: Nested scopes
// ============================================================================
void test_4_nested_scopes() {
    printf("Test 4: Nested scopes\n");
    int* outer = malloc(sizeof(int));
    *outer = 100;
    {
        int* inner = malloc(sizeof(int));
        *inner = 200;
        printf("  inner=%d\n", *inner);
        // Should free inner here
    }
    printf("  outer=%d\n", *outer);
    // Should free outer here
}

// ============================================================================
// TEST 5: Manual free (should not double-free)
// ============================================================================
void test_5_manual_free() {
    printf("Test 5: Manual free\n");
    int* p = malloc(sizeof(int));
    *p = 99;
    free(p);
    printf("  Manually freed\n");
    // Should NOT auto-free (already freed)
}

// ============================================================================
// TEST 6: NULL pointer
// ============================================================================
void test_6_null_pointer() {
    printf("Test 6: NULL pointer\n");
    int* p = NULL;
    printf("  p is NULL\n");
    // Should not insert free(p)
}

// ============================================================================
// TEST 7: Conditional allocation
// ============================================================================
void test_7_conditional() {
    printf("Test 7: Conditional allocation\n");
    int* p = NULL;
    int flag = 1;
    if (flag) {
        p = malloc(sizeof(int));
        *p = 777;
    }
    if (p) {
        printf("  Value: %d\n", *p);
    }
    // Should auto-free with NULL check
}

// ============================================================================
// TEST 8: Loop with allocation
// ============================================================================
void test_8_loop() {
    printf("Test 8: Loop with allocation\n");
    for (int i = 0; i < 3; i++) {
        int* p = malloc(sizeof(int));
        *p = i * 10;
        printf("  Loop %d: %d\n", i, *p);
        // Should auto-free at each iteration
    }
}

// ============================================================================
// TEST 9: Early return (CRITICAL TEST)
// ============================================================================
void test_9_early_return() {
    printf("Test 9: Early return\n");
    int* p = malloc(sizeof(int));
    *p = 42;

    if (*p == 42) {
        printf("  Early return with value %d\n", *p);
        return;  // MUST auto-free p before return!
    }

    printf("  This should not print\n");
}

// ============================================================================
// TEST 10: Multiple early returns
// ============================================================================
void test_10_multiple_returns() {
    printf("Test 10: Multiple returns\n");
    int* p = malloc(sizeof(int));
    int* q = malloc(sizeof(int));
    *p = 10;
    *q = 20;

    if (*p == 10) {
        printf("  First path: p=%d\n", *p);
        return;  // MUST free both p and q
    }

    if (*q == 20) {
        printf("  Second path: q=%d\n", *q);
        return;  // MUST free both p and q
    }
}

// ============================================================================
// TEST 11: calloc
// ============================================================================
void test_11_calloc() {
    printf("Test 11: calloc\n");
    int* p = (int*)calloc(10, sizeof(int));
    p[5] = 999;
    printf("  Value: %d\n", p[5]);
    // Should auto-free
}

// ============================================================================
// TEST 12: realloc
// ============================================================================
void test_12_realloc() {
    printf("Test 12: realloc\n");
    int* p = malloc(sizeof(int));
    *p = 42;
    p = (int*)realloc(p, 2 * sizeof(int));
    p[1] = 100;
    printf("  Values: %d, %d\n", p[0], p[1]);
    // Should auto-free
}

// ============================================================================
// TEST 13: Large allocation
// ============================================================================
void test_13_large_alloc() {
    printf("Test 13: Large allocation\n");
    int* arr = (int*)malloc(1000 * sizeof(int));
    arr[0] = 1;
    arr[999] = 999;
    printf("  First: %d, Last: %d\n", arr[0], arr[999]);
    // Should auto-free
}

// ============================================================================
// TEST 14: String allocation
// ============================================================================
void test_14_string() {
    printf("Test 14: String allocation\n");
    char* str = (char*)malloc(100);
    strcpy(str, "Hello, RCC!");
    printf("  String: %s\n", str);
    // Should auto-free
}

// ============================================================================
// TEST 15: Pointer reassignment
// ============================================================================
void test_15_reassignment() {
    printf("Test 15: Pointer reassignment\n");
    int* p = malloc(sizeof(int));
    *p = 10;
    printf("  First alloc: %d\n", *p);

    // Reassign - old allocation lost (memory leak if not freed)
    p = malloc(sizeof(int));
    *p = 20;
    printf("  Second alloc: %d\n", *p);
    // Should auto-free second allocation
    // ISSUE: First allocation leaked!
}

// ============================================================================
// TEST 16: Switch statement
// ============================================================================
void test_16_switch() {
    printf("Test 16: Switch statement\n");
    int* p = malloc(sizeof(int));
    *p = 2;

    switch (*p) {
        case 1:
            printf("  Case 1\n");
            break;
        case 2:
            printf("  Case 2\n");
            break;
        default:
            printf("  Default\n");
            break;
    }
    // Should auto-free
}

// ============================================================================
// TEST 17: While loop
// ============================================================================
void test_17_while() {
    printf("Test 17: While loop\n");
    int count = 0;
    while (count < 3) {
        int* p = malloc(sizeof(int));
        *p = count;
        printf("  Iteration %d\n", *p);
        count++;
        // Should auto-free at each iteration
    }
}

// ============================================================================
// TEST 18: Do-while loop
// ============================================================================
void test_18_do_while() {
    printf("Test 18: Do-while loop\n");
    int count = 0;
    do {
        int* p = malloc(sizeof(int));
        *p = count * 100;
        printf("  Value: %d\n", *p);
        count++;
        // Should auto-free at each iteration
    } while (count < 2);
}

// ============================================================================
// TEST 19: Malloc in if-else branches
// ============================================================================
void test_19_if_else_branches() {
    printf("Test 19: If-else branches\n");
    int flag = 1;
    int* p;

    if (flag) {
        p = malloc(sizeof(int));
        *p = 100;
    } else {
        p = malloc(sizeof(int));
        *p = 200;
    }

    printf("  Value: %d\n", *p);
    // Should auto-free
}

// ============================================================================
// TEST 20: Goto statement (CRITICAL TEST)
// ============================================================================
void test_20_goto() {
    printf("Test 20: Goto statement\n");
    int* p = malloc(sizeof(int));
    *p = 42;

    if (*p == 42) {
        goto cleanup;
    }

    printf("  This should not print\n");

cleanup:
    printf("  Cleanup label reached\n");
    // Should auto-free p before function exit
    // ISSUE: p might not be freed before goto!
}

// ============================================================================
// Main test runner
// ============================================================================
int main() {
    printf("═══════════════════════════════════════════════════════════\n");
    printf("RCC v4.1.0 - Comprehensive Memory Safety Tests\n");
    printf("═══════════════════════════════════════════════════════════\n\n");

    test_1_basic_malloc();
    test_2_cast_malloc();
    test_3_multiple_allocs();
    test_4_nested_scopes();
    test_5_manual_free();
    test_6_null_pointer();
    test_7_conditional();
    test_8_loop();
    test_9_early_return();
    test_10_multiple_returns();
    test_11_calloc();
    test_12_realloc();
    test_13_large_alloc();
    test_14_string();
    test_15_reassignment();
    test_16_switch();
    test_17_while();
    test_18_do_while();
    test_19_if_else_branches();
    test_20_goto();

    printf("\n═══════════════════════════════════════════════════════════\n");
    printf("All tests completed!\n");
    printf("═══════════════════════════════════════════════════════════\n");

    return 0;
}
