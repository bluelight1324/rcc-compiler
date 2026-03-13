// Test struct pass and return by value
extern int printf(const char* fmt, ...);

// Small struct (≤8 bytes) - should pass/return in register
struct Point {
    int x;
    int y;
};

// Medium struct (9-16 bytes) - should use RAX:RDX
struct Vector3 {
    int x;
    int y;
    int z;
};

// Large struct (>16 bytes) - should use hidden pointer
struct Box {
    int x1;
    int y1;
    int x2;
    int y2;
    int color;
};

// Test small struct (8 bytes)
struct Point addPoints(struct Point a, struct Point b) {
    struct Point result;
    result.x = a.x + b.x;
    result.y = a.y + b.y;
    return result;
}

// Test medium struct (12 bytes)
struct Vector3 addVectors(struct Vector3 a, struct Vector3 b) {
    struct Vector3 result;
    result.x = a.x + b.x;
    result.y = a.y + b.y;
    result.z = a.z + b.z;
    return result;
}

int main(void) {
    printf("=== Struct By-Value Test ===\n");

    // Test small struct
    struct Point p1;
    struct Point p2;
    struct Point p3;

    p1.x = 10;
    p1.y = 20;
    p2.x = 5;
    p2.y = 15;

    p3 = addPoints(p1, p2);
    printf("Small struct: addPoints({10,20}, {5,15}) = {%d,%d}\n", p3.x, p3.y);

    int passed = 1;
    if (p3.x != 15 || p3.y != 35) {
        printf("FAIL: Expected {15,35}\n");
        passed = 0;
    }

    // Test medium struct
    struct Vector3 v1;
    struct Vector3 v2;
    struct Vector3 v3;

    v1.x = 1;
    v1.y = 2;
    v1.z = 3;
    v2.x = 4;
    v2.y = 5;
    v2.z = 6;

    v3 = addVectors(v1, v2);
    printf("Medium struct: addVectors({1,2,3}, {4,5,6}) = {%d,%d,%d}\n", v3.x, v3.y, v3.z);

    if (v3.x != 5 || v3.y != 7 || v3.z != 9) {
        printf("FAIL: Expected {5,7,9}\n");
        passed = 0;
    }

    if (passed) {
        printf("\nPASSED: Struct by-value works!\n");
        return 0;
    } else {
        printf("\nFAILED!\n");
        return 1;
    }
}
