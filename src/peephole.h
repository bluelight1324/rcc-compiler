#pragma once
#include <string>

/**
 * Peephole Optimizer — Phase E (Section 3.3)
 *
 * Reads the emitted MASM assembly text and applies local pattern-replacement
 * optimizations. Runs as a post-processing pass after CodeGen completes.
 *
 * Enabled when optimization is requested (future -O flag) or always for
 * trivially-safe patterns (zero-add elimination, etc.).
 */

// Apply all peephole patterns to the assembly text in-place.
// opt_level: 1 = standard patterns; 2+ = additional constant-folding passes.
// Returns the number of changes made (0 = already optimal).
int peepholeOptimize(std::string& asm_text, int opt_level = 1);
