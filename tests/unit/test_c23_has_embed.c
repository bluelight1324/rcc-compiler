/* test_c23_has_embed.c
 * Task 7.44 — C23 §6.10.9.3: __has_embed predicate
 *
 * __has_embed(resource) returns non-zero if the resource can be embedded,
 * zero if it cannot be found.
 *
 * Expected: compile with exit 0
 */
#include <stdio.h>

/* An existing system header can be embedded as raw bytes */
#if __has_embed(<stdio.h>)
#define HAS_STDIO_EMBED 1
#else
#define HAS_STDIO_EMBED 0
#endif

/* A non-existent resource should return 0 */
#if __has_embed(<nonexistent_binary_xyz.bin>)
#define HAS_FAKE_EMBED 1
#else
#define HAS_FAKE_EMBED 0
#endif

int main(void) {
    printf("=== __has_embed Test ===\n");
    printf("__has_embed(<stdio.h>)                  = %d\n", HAS_STDIO_EMBED);
    printf("__has_embed(<nonexistent_binary_xyz.bin>) = %d\n", HAS_FAKE_EMBED);

    int fail = 0;
    if (HAS_STDIO_EMBED == 0) {
        printf("FAIL: stdio.h should be embeddable\n");
        fail = 1;
    }
    if (HAS_FAKE_EMBED != 0) {
        printf("FAIL: nonexistent file should not be embeddable\n");
        fail = 1;
    }
    if (!fail) printf("ALL __has_embed TESTS PASSED\n");
    return fail;
}
