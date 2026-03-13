/* test_embed.c - W23: #embed directive (C23)
 * embed_data.bin contains 'A','B','C' = 65,66,67
 */
#include <stdio.h>

int main() {
    /* #embed expands to comma-separated byte values */
    unsigned char data[] = {
        #embed "embed_data.bin"
    };
    int n = (int)sizeof(data);
    printf("%d\n", n);          /* 3 (or 4 if the file has a trailing newline) */
    /* Print first byte: 65 = 'A' */
    printf("%d\n", (int)data[0]); /* 65 */
    return 0;
}
