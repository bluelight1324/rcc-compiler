/* test_pragma_once.c
 * Task 7.43 — D3: #pragma once support
 *
 * Includes the same header three times.  Without #pragma once the typedef
 * and macro would be duplicated; with #pragma once each is processed only
 * once, so the compilation succeeds.
 *
 * Expected: compile → exit 0 (no duplicate-type parse error)
 */
#include <stdio.h>
#include "test_pragma_once_hdr.h"
#include "test_pragma_once_hdr.h"   /* should be skipped by #pragma once */
#include "test_pragma_once_hdr.h"   /* should be skipped by #pragma once */

int main(void) {
    PragmaOnceInt x = PRAGMA_ONCE_DEFINED;   /* uses typedef + macro from header */
    printf("pragma_once: x=%d\n", x);
    return x != 1;   /* exit 0 on pass */
}
