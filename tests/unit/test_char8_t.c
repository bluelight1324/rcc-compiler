/* test_char8_t.c - W25: char8_t type and u8"..." prefix (C23) */
#include <stdio.h>
#include <string.h>

int main() {
    /* char8_t is an alias for char — should compile without error */
    char8_t ch = 'A';
    printf("%c\n", (char)ch);   /* A */

    /* u8"..." string prefix — should compile as a regular string */
    const char* s = u8"hello";
    printf("%s\n", s);          /* hello */
    printf("%d\n", (int)strlen(s)); /* 5 */

    return 0;
}
