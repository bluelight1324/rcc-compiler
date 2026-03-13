/* test_fallthrough_attr.c - W5: [[fallthrough]] attribute suppresses warning */
#include <stdio.h>

int main() {
    int x = 1;
    int result = 0;

    switch (x) {
        case 1:
            result = 10;
            [[fallthrough]];   /* W5: suppresses fall-through warning */
        case 2:
            result += 5;
            break;
        case 3:
            result = 99;
            break;
    }

    printf("%d\n", result);   /* 15 */
    return 0;
}
