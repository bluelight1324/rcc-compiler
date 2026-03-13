// Probe test to see which Tier 2 features work
extern int printf(const char* fmt, ...);

// Test 1: Multi-dimensional arrays
int test_multi_array(void) {
    int arr[3][4];
    int i;
    int j;

    // Initialize
    for (i = 0; i < 3; i = i + 1) {
        for (j = 0; j < 4; j = j + 1) {
            arr[i][j] = i * 4 + j;
        }
    }

    // Check values
    int ok = 1;
    ok = ok && (arr[0][0] == 0);
    ok = ok && (arr[1][2] == 6);
    ok = ok && (arr[2][3] == 11);

    printf("Multi-dim array: arr[1][2]=%d (expected 6)\n", arr[1][2]);
    return ok;
}

// Test 2: Nested struct member access
struct Inner {
    int x;
    int y;
};

struct Outer {
    struct Inner in;
    int z;
};

int test_nested_struct(void) {
    struct Outer o;
    o.in.x = 10;
    o.in.y = 20;
    o.z = 30;

    int sum = o.in.x + o.in.y + o.z;
    printf("Nested struct: o.in.x=%d, o.in.y=%d, sum=%d (expected 60)\n", o.in.x, o.in.y, sum);
    return sum == 60;
}

int main(void) {
    int passed = 0;

    printf("=== Tier 2 Probe Tests ===\n\n");

    if (test_multi_array()) passed = passed + 1;
    if (test_nested_struct()) passed = passed + 1;

    printf("\n=== Results ===\n");
    printf("Passed: %d/2\n", passed);

    return 2 - passed;
}
