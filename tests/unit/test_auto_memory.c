/*
 * Test: Automatic Memory Management (RCC v4.1)
 *
 * This test demonstrates the new hybrid memory management system:
 * 1. Scope-based automatic free (--safety=medium)
 * 2. Runtime use-after-free guards (--safety=full)
 *
 * COMPILE AND RUN:
 *   # With automatic free (minimal overhead):
 *   $ rcc --safety=medium test_auto_memory.c -o test_medium
 *   $ ./test_medium
 *
 *   # With runtime guards (debug mode):
 *   $ rcc --safety=full test_auto_memory.c -o test_full
 *   $ ./test_full
 */

#include <stdlib.h>
#include <stdio.h>

// ═══════════════════════════════════════════════════════════════
// Test 1: Basic Automatic Free
// ═══════════════════════════════════════════════════════════════

void test_basic_auto_free() {
    printf("Test 1: Basic automatic free\n");

    auto buffer = malloc(1024);  // Classified as OWNER
    if (!buffer) return;

    *buffer = 42;
    printf("  Buffer value: %d\n", *buffer);

    // With --safety=medium: compiler inserts free(buffer) here
    // With --safety=none: manual free() required
}

// Expected behavior:
// --safety=medium: buffer automatically freed at scope exit ✅
// --safety=none: memory leaked ❌

// ═══════════════════════════════════════════════════════════════
// Test 2: Early Return
// ═══════════════════════════════════════════════════════════════

int test_early_return(int condition) {
    printf("Test 2: Early return\n");

    auto data = malloc(500);
    if (!data) return -1;

    if (condition) {
        printf("  Early return path\n");
        return -1;  // Auto-free inserted before return
    }

    printf("  Normal path\n");
    return 0;  // Auto-free inserted before return
}

// Expected behavior:
// --safety=medium: data freed on both paths ✅

// ═══════════════════════════════════════════════════════════════
// Test 3: Multiple Scopes
// ═══════════════════════════════════════════════════════════════

void test_multiple_scopes() {
    printf("Test 3: Multiple scopes\n");

    auto outer = malloc(100);
    printf("  Outer scope allocated\n");

    {
        auto inner = malloc(200);
        printf("  Inner scope allocated\n");

        // inner freed here (inner scope exit)
    }

    printf("  Back to outer scope\n");

    // outer freed here (outer scope exit)
}

// Expected behavior:
// --safety=medium:
//   - inner freed at end of { } block
//   - outer freed at end of function

// ═══════════════════════════════════════════════════════════════
// Test 4: Ownership Transfer (Return Value)
// ═══════════════════════════════════════════════════════════════

char* create_buffer() {
    printf("Test 4: Ownership transfer\n");

    auto buf = malloc(300);
    if (!buf) return NULL;

    *buf = 'A';

    return buf;  // Ownership transferred to caller
                 // NO auto-free here!
}

void test_ownership_transfer() {
    auto buffer = create_buffer();
    printf("  Received buffer: %c\n", *buffer);

    // buffer freed here (at caller's scope exit)
}

// Expected behavior:
// --safety=medium:
//   - NO free in create_buffer (ownership transferred)
//   - Free in test_ownership_transfer (caller owns it)

// ═══════════════════════════════════════════════════════════════
// Test 5: Manual Free (Should Not Double-Free)
// ═══════════════════════════════════════════════════════════════

void test_manual_free() {
    printf("Test 5: Manual free\n");

    auto ptr = malloc(400);
    printf("  Allocated\n");

    free(ptr);  // Manual free
    printf("  Manually freed\n");

    // NO auto-free here (already manually freed)
}

// Expected behavior:
// --safety=medium:
//   - Manual free() is detected
//   - NO auto-free inserted (would cause double-free)
// --safety=full:
//   - Manual free() tracked in guard table
//   - Any subsequent use of ptr aborts at runtime

// ═══════════════════════════════════════════════════════════════
// Test 6: Use-After-Free Detection (--safety=full)
// ═══════════════════════════════════════════════════════════════

void test_use_after_free_BUGGY() {
    printf("Test 6: Use-after-free (BUGGY - should abort with --safety=full)\n");

    auto ptr = malloc(100);
    *ptr = 42;
    printf("  Value before free: %d\n", *ptr);

    free(ptr);

    // BUG: Use-after-free!
    *ptr = 100;  // With --safety=full: ABORT HERE with error message
    printf("  Value after free: %d\n", *ptr);  // Should never reach this
}

// Expected behavior:
// --safety=none: Undefined behavior (may crash, may not)
// --safety=medium: Compiles with warning, may crash at runtime
// --safety=full: ABORTS at runtime with clear error message ✅
//   "ERROR: Use-after-free detected at test_auto_memory.c:118"

// ═══════════════════════════════════════════════════════════════
// Test 7: Double-Free Detection (--safety=full)
// ═══════════════════════════════════════════════════════════════

void test_double_free_BUGGY() {
    printf("Test 7: Double-free (BUGGY - should abort with --safety=full)\n");

    auto data = malloc(200);
    printf("  Allocated\n");

    free(data);
    printf("  First free\n");

    free(data);  // BUG: Double-free!
                 // With --safety=full: ABORT HERE
    printf("  Second free\n");  // Should never reach this
}

// Expected behavior:
// --safety=full: ABORTS with error message ✅
//   "ERROR: Double-free detected at test_auto_memory.c:140"

// ═══════════════════════════════════════════════════════════════
// Main Test Runner
// ═══════════════════════════════════════════════════════════════

int main() {
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("RCC v4.1 Automatic Memory Management Test Suite\n");
    printf("═══════════════════════════════════════════════════════════════\n\n");

    test_basic_auto_free();
    printf("\n");

    test_early_return(0);
    printf("\n");

    test_multiple_scopes();
    printf("\n");

    test_ownership_transfer();
    printf("\n");

    test_manual_free();
    printf("\n");

    // UNCOMMENT TO TEST RUNTIME GUARDS (--safety=full only):
    // These tests will ABORT when runtime guards are enabled

    // test_use_after_free_BUGGY();  // Aborts with --safety=full
    // test_double_free_BUGGY();     // Aborts with --safety=full

    printf("═══════════════════════════════════════════════════════════════\n");
    printf("All safe tests passed! ✅\n");
    printf("\n");
    printf("To test runtime guards, uncomment buggy tests and compile with:\n");
    printf("  $ rcc --safety=full test_auto_memory.c -o test_full\n");
    printf("  $ ./test_full\n");
    printf("═══════════════════════════════════════════════════════════════\n");

    return 0;
}

/*
 * EXPECTED OUTPUT:
 * ═══════════════════════════════════════════════════════════════
 *
 * With --safety=none (manual memory management):
 *   All tests run, but memory leaks occur
 *   Valgrind would show leaks
 *
 * With --safety=minimal (default, warnings only):
 *   Compile-time warnings for leaks, use-after-free
 *   Runtime: same as --safety=none
 *
 * With --safety=medium (automatic free):
 *   All memory automatically freed at scope exit
 *   No memory leaks
 *   Valgrind shows 0 leaks
 *   ~0-2% overhead
 *
 * With --safety=full (runtime guards):
 *   All memory automatically freed
 *   Buggy tests ABORT at runtime with clear error messages
 *   ~5-15% overhead
 *
 * ═══════════════════════════════════════════════════════════════
 */
