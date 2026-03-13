// Test deeply nested struct member access (3+ levels)
extern int printf(const char* fmt, ...);

struct Point {
    int x;
    int y;
};

struct Box {
    struct Point topLeft;
    struct Point bottomRight;
};

struct Scene {
    struct Box viewport;
    int frameCount;
};

int main(void) {
    struct Scene s;

    printf("=== Deep Nested Struct Test ===\n");

    // Test 3-level nesting: s.viewport.topLeft.x
    s.viewport.topLeft.x = 10;
    s.viewport.topLeft.y = 20;
    s.viewport.bottomRight.x = 100;
    s.viewport.bottomRight.y = 200;
    s.frameCount = 42;

    printf("s.viewport.topLeft.x = %d (expected 10)\n", s.viewport.topLeft.x);
    printf("s.viewport.topLeft.y = %d (expected 20)\n", s.viewport.topLeft.y);
    printf("s.viewport.bottomRight.x = %d (expected 100)\n", s.viewport.bottomRight.x);
    printf("s.viewport.bottomRight.y = %d (expected 200)\n", s.viewport.bottomRight.y);
    printf("s.frameCount = %d (expected 42)\n", s.frameCount);

    int passed = 1;
    if (s.viewport.topLeft.x != 10) passed = 0;
    if (s.viewport.topLeft.y != 20) passed = 0;
    if (s.viewport.bottomRight.x != 100) passed = 0;
    if (s.viewport.bottomRight.y != 200) passed = 0;
    if (s.frameCount != 42) passed = 0;

    if (passed) {
        printf("\nPASSED: Deep nested struct access works!\n");
        return 0;
    } else {
        printf("\nFAILED!\n");
        return 1;
    }
}
