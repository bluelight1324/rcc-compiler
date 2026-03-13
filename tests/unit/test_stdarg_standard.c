// Test standard stdarg.h implementation
// Inline stdarg.h definitions for testing
typedef char* va_list;
#define va_start(ap, last) ((ap) = (char*)&(last) + 8)
#define va_arg(ap, type) (*(type*)((ap) += 8, (ap) - 8))
#define va_end(ap) ((ap) = (char*)0)

extern int printf(const char* fmt, ...);

// Test 1: Sum integers using standard va_arg syntax
int sum_ints(int count, ...) {
    va_list args;
    va_start(args, count);

    int total = 0;
    for (int i = 0; i < count; i = i + 1) {
        int val = va_arg(args, int);  // Standard syntax with type
        total = total + val;
    }

    va_end(args);
    return total;
}

// Test 2: Find maximum using va_copy
int max_ints(int count, ...) {
    va_list args;
    va_start(args, count);

    // Get first value
    int max_val = va_arg(args, int);

    // Find maximum
    for (int i = 1; i < count; i = i + 1) {
        int val = va_arg(args, int);
        if (val > max_val) {
            max_val = val;
        }
    }

    va_end(args);
    return max_val;
}

// Test 3: Process mixed types (int and long)
long sum_mixed(int count, ...) {
    va_list args;
    va_start(args, count);

    long total = 0;
    for (int i = 0; i < count; i = i + 1) {
        // All arguments are promoted to 8 bytes on x64
        long val = va_arg(args, long);
        total = total + val;
    }

    va_end(args);
    return total;
}

int main(void) {
    printf("=== Standard stdarg.h Tests ===\n\n");

    // Test 1: sum_ints
    int sum1 = sum_ints(3, 10, 20, 30);
    printf("sum_ints(3, 10, 20, 30) = %d (expected 60)\n", sum1);

    int sum2 = sum_ints(5, 1, 2, 3, 4, 5);
    printf("sum_ints(5, 1, 2, 3, 4, 5) = %d (expected 15)\n", sum2);

    // Test 2: max_ints
    int max1 = max_ints(4, 5, 12, 8, 3);
    printf("max_ints(4, 5, 12, 8, 3) = %d (expected 12)\n", max1);

    // Test 3: sum_mixed with long values
    long sum3 = sum_mixed(3, 100L, 200L, 300L);
    printf("sum_mixed(3, 100L, 200L, 300L) = %ld (expected 600)\n", sum3);

    printf("\n=== All stdarg.h Tests Complete ===\n");

    // Return 0 for success
    int success = 1;
    if (sum1 == 60 && sum2 == 15 && max1 == 12 && sum3 == 600) {
        success = 0;
    }
    return success;
}
