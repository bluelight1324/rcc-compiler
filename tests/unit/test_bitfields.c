#include <stdio.h>

struct Flags {
    int ready:1;
    int error:3;
    int count:4;
};

int main(void) {
    struct Flags f;
    f.ready = 1;
    f.error = 5;
    f.count = 9;
    printf("%d %d %d\n", f.ready, f.error, f.count);
    return 0;
}
