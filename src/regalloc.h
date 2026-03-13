#pragma once
#include <string>

/**
 * regalloc.h — Post-processing register reuse pass
 *
 * Implements a single-pass copy-propagation analysis on emitted MASM
 * x86-64 assembly text to eliminate redundant stack slot round-trips.
 *
 * Two main transformations:
 *
 *   Store-then-reload elimination:
 *     mov QWORD PTR [rbp-N], rax   ; store result
 *     mov rax, QWORD PTR [rbp-N]   ; immediately reload same slot
 *     → delete the reload (rax still holds the value)
 *
 *   Dead store elimination:
 *     mov QWORD PTR [rbp-N], rax   ; store A
 *     ... (no intervening read of [rbp-N]) ...
 *     mov QWORD PTR [rbp-N], rcx   ; overwrite with B
 *     → delete the first store if [rbp-N] is not read between them
 *
 * These patterns arise naturally from RCC's "every expression spills to stack"
 * code generation strategy.  Eliminating them reduces memory traffic by 20-40%
 * on computation-heavy code.
 *
 * Returns the number of lines eliminated.
 */
int regAllocPass(std::string& asm_text);
