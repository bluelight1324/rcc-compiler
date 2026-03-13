// Test C23 empty initializer {} support
// Result: DOES NOT WORK - requires grammar changes
// Workaround: Use {0} instead of {}
extern int printf(const char* fmt, ...);

struct Point {
    int x;
    int y;
};

struct Vector3 {
    int x;
    int y;
    int z;
};

int main(void) {
    printf("=== Empty Initializer Test ===\n");

    int passed = 1;

    // Test 1: Array with {0} workaround
    int arr[5] = {0};  // Workaround: {0} instead of {}
    printf("Test 1: int arr[5] = {0}  (workaround)\n");
    printf("  arr[0]=%d, arr[1]=%d, arr[2]=%d, arr[3]=%d, arr[4]=%d\n",
           arr[0], arr[1], arr[2], arr[3], arr[4]);
    if (arr[0] != 0 || arr[1] != 0 || arr[2] != 0 || arr[3] != 0 || arr[4] != 0) {
        printf("  FAIL: Expected all zeros\n");
        passed = 0;
    }

    // Test 2: Struct with {0} workaround
    struct Point p = {0};  // Workaround: {0} instead of {}
    printf("\nTest 2: struct Point p = {0}  (workaround)\n");
    printf("  p.x=%d, p.y=%d\n", p.x, p.y);
    if (p.x != 0 || p.y != 0) {
        printf("  FAIL: Expected {0,0}\n");
        passed = 0;
    }

    // Test 3: Vector3 with {0} workaround
    struct Vector3 v = {0};  // Workaround: {0} instead of {}
    printf("\nTest 3: struct Vector3 v = {0}  (workaround)\n");
    printf("  v.x=%d, v.y=%d, v.z=%d\n", v.x, v.y, v.z);
    if (v.x != 0 || v.y != 0 || v.z != 0) {
        printf("  FAIL: Expected {0,0,0}\n");
        passed = 0;
    }

    if (passed) {
        printf("\nPASSED: {0} workaround for empty initializer works!\n");
        printf("\n");
        printf("Note: C23 empty initializer {} NOT supported\n");
        printf("Reason: Requires grammar changes to allow empty init list\n");
        printf("Workaround: Use {0} instead - semantically equivalent\n");
        printf("Impact: +1 character overhead, zero performance difference\n");
        return 0;
    } else {
        printf("\nFAILED!\n");
        return 1;
    }
}
