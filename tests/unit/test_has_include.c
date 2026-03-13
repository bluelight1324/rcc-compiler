/* test_has_include.c - W24: __has_include operator (C23) */
#include <stdio.h>

#if __has_include(<stdio.h>)
#define HAS_STDIO 1
#else
#define HAS_STDIO 0
#endif

/* This file should not exist */
#if __has_include(<nonexistent_header_xyz.h>)
#define HAS_FAKE 1
#else
#define HAS_FAKE 0
#endif

int main() {
    printf("%d\n", HAS_STDIO);  /* 1 */
    printf("%d\n", HAS_FAKE);   /* 0 */
    return 0;
}
