/*
 * Test: auto Keyword with Type Inference and Rust Safety Integration
 *
 * This test demonstrates the C#-style 'auto' keyword in RCC v4.0+
 *
 * Features tested:
 * 1. Type inference from literal values
 * 2. Type inference from expressions
 * 3. Pointer type inference
 * 4. Integration with Rust-style memory safety analysis
 */

#include <stdlib.h>
#include <stdio.h>

int main() {
    // ═══════════════════════════════════════════════════════════════
    // Test 1: Basic type inference from literals
    // ═══════════════════════════════════════════════════════════════

    auto x = 42;              // Inferred as int
    auto y = 3.14;            // Inferred as float/double
    auto c = 'A';             // Inferred as char
    auto msg = "Hello";       // Inferred as char*

    printf("Basic inference: x=%d, y=%f, c=%c, msg=%s\n", x, y, c, msg);

    // ═══════════════════════════════════════════════════════════════
    // Test 2: Type inference from expressions
    // ═══════════════════════════════════════════════════════════════

    auto sum = x + 10;        // int + int = int
    auto product = x * y;     // int * double = double
    auto result = sum > 50;   // Comparison = int (bool)

    printf("Expression inference: sum=%d, product=%f, result=%d\n",
           sum, product, result);

    // ═══════════════════════════════════════════════════════════════
    // Test 3: Pointer type inference (integrates with safety analysis!)
    // ═══════════════════════════════════════════════════════════════

    // Inferred as int* - and classified as OWNER by safety analysis
    auto ptr = (int*)malloc(sizeof(int));
    if (ptr) {
        *ptr = 100;
        printf("Pointer value: %d\n", *ptr);
    }

    // Inferred as int* - and classified as BORROWED by safety analysis
    auto borrowed = &x;
    printf("Borrowed value: %d\n", *borrowed);

    // Safety analysis should track ownership of 'ptr'
    free(ptr);
    // ptr is now in FREED state - safety analysis detects use-after-free
    // *ptr = 200;  // UNCOMMENT to test: Should trigger warning!

    // ═══════════════════════════════════════════════════════════════
    // Test 4: Complex type inference
    // ═══════════════════════════════════════════════════════════════

    int arr[5] = {1, 2, 3, 4, 5};
    auto arr_ptr = arr;       // Inferred as int*
    auto elem = arr[2];       // Inferred as int

    printf("Array inference: elem=%d, first=%d\n", elem, *arr_ptr);

    // ═══════════════════════════════════════════════════════════════
    // Test 5: Function call type inference
    // ═══════════════════════════════════════════════════════════════

    auto heap = malloc(64);   // Inferred as void* (or int* depending on cast)
    if (heap) {
        printf("Allocated 64 bytes\n");
        free(heap);
    }

    // ═══════════════════════════════════════════════════════════════
    // Test 6: auto with Rust safety - ownership tracking
    // ═══════════════════════════════════════════════════════════════

    auto owner1 = malloc(sizeof(int));  // OWNER
    auto owner2 = owner1;                // OWNER transferred (move semantics)

    // Safety analysis tracks that owner1 is MOVED
    // free(owner1);  // UNCOMMENT to test: Should warn about use-after-move!

    free(owner2);  // OK - owner2 has ownership

    return 0;
}

/*
 * EXPECTED BEHAVIOR:
 * ═══════════════════════════════════════════════════════════════
 *
 * Compilation:
 *   $ rcc test_var_keyword.c -c
 *   Running memory safety analysis (level: minimal)...
 *   ✓ No memory safety issues detected
 *   Generated: test_var_keyword.asm
 *
 * Safety Integration:
 *   - 'ptr' classified as OWNER (malloc result)
 *   - 'borrowed' classified as BORROWED (address-of)
 *   - Ownership state tracked through program
 *   - Use-after-free detected if uncommented
 *   - Use-after-move detected if uncommented
 *
 * Type Inference:
 *   - All 'auto' declarations get correct types
 *   - Works with literals, expressions, pointers
 *   - Integrates seamlessly with existing codegen
 *
 * NOTES:
 * ═══════════════════════════════════════════════════════════════
 *
 * 1. 'auto' is syntactic sugar for C23 'auto' keyword
 * 2. Requires initializer (can't infer type without one)
 * 3. Type inference happens at compile-time (zero runtime cost)
 * 4. Fully integrated with RCC v4.0 Rust-style safety analysis
 * 5. Ownership classification happens automatically:
 *    - malloc() result → OWNER
 *    - &variable → BORROWED
 *    - Stack variables → TEMP
 *    - Assignment → MOVE (ownership transfer)
 */
