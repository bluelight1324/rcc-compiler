// Test nested struct member access
extern int printf(const char* fmt, ...);

struct Inner {
    int x;
    int y;
};

struct Outer {
    struct Inner in;
    int z;
};

int main(void) {
    struct Outer o;

    printf("Testing nested structs...\n");

    o.in.x = 10;
    o.in.y = 20;
    o.z = 30;

    printf("o.in.x = %d (expected 10)\n", o.in.x);
    printf("o.in.y = %d (expected 20)\n", o.in.y);
    printf("o.z = %d (expected 30)\n", o.z);

    int sum = o.in.x + o.in.y + o.z;
    printf("sum = %d (expected 60)\n", sum);

    return sum == 60 ? 0 : 1;
}
