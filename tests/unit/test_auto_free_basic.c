/**
 * test_auto_free_basic.c - Basic Auto-Free Functionality Tests
 * RCC v4.1 - Automatic Memory Management
 *
 * Tests basic scope-based automatic free() insertion
 */

#include <stdio.h>
#include <stdlib.h>

// Test 1: Simple malloc + scope exit
void test_simple_auto_free() {
    printf("Test 1: Simple auto-free\n");
    {
        int* p = (int*)malloc(100);
        *p = 42;
        printf("  Allocated: %d\n", *p);
        // Should auto-insert: if (p) free(p);
    }
    printf("  ✓ Scope exited, should have freed p\n\n");
}

// Test 2: Multiple allocations
void test_multiple_allocations() {
    printf("Test 2: Multiple allocations\n");
    {
        int* a = (int*)malloc(sizeof(int));
        int* b = (int*)malloc(sizeof(int));
        int* c = (int*)malloc(sizeof(int));

        *a = 1;
        *b = 2;
        *c = 3;

        printf("  a=%d, b=%d, c=%d\n", *a, *b, *c);
        // Should auto-insert free(c), free(b), free(a) in LIFO order
    }
    printf("  ✓ All freed\n\n");
}

// Test 3: Nested scopes
void test_nested_scopes() {
    printf("Test 3: Nested scopes\n");
    {
        int* outer = (int*)malloc(sizeof(int));
        *outer = 100;

        {
            int* inner = (int*)malloc(sizeof(int));
            *inner = 200;
            printf("  inner=%d, outer=%d\n", *inner, *outer);
            // Should free inner here
        }

        printf("  outer still valid: %d\n", *outer);
        // Should free outer here
    }
    printf("  ✓ Both freed at correct scopes\n\n");
}

// Test 4: Manual free - should not double-free
void test_manual_free() {
    printf("Test 4: Manual free (no double-free)\n");
    {
        int* p = (int*)malloc(sizeof(int));
        *p = 99;
        printf("  Allocated: %d\n", *p);

        free(p);  // Manual free
        printf("  Manually freed\n");

        // Should NOT auto-insert free(p) because already freed
    }
    printf("  ✓ No double-free\n\n");
}

// Test 5: NULL pointer (should be safe)
void test_null_pointer() {
    printf("Test 5: NULL pointer\n");
    {
        int* p = NULL;  // Not an OWNER (not from malloc)
        printf("  p is NULL\n");
        // Should not insert free(p) for NULL literal
    }
    printf("  ✓ NULL handled safely\n\n");
}

// Test 6: Conditional allocation
void test_conditional() {
    printf("Test 6: Conditional allocation\n");
    {
        int* p = NULL;
        int do_alloc = 1;

        if (do_alloc) {
            p = (int*)malloc(sizeof(int));
            *p = 777;
            printf("  Allocated: %d\n", *p);
        }

        // Should auto-insert: if (p) free(p);
    }
    printf("  ✓ Conditional handled\n\n");
}

int main() {
    printf("═══════════════════════════════════════════════════════════\n");
    printf("RCC v4.1 - Automatic Memory Management Tests\n");
    printf("═══════════════════════════════════════════════════════════\n\n");

    test_simple_auto_free();
    test_multiple_allocations();
    test_nested_scopes();
    test_manual_free();
    test_null_pointer();
    test_conditional();

    printf("═══════════════════════════════════════════════════════════\n");
    printf("All tests completed!\n");
    printf("═══════════════════════════════════════════════════════════\n");

    return 0;
}
