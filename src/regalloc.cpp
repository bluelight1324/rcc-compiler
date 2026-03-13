/**
 * regalloc.cpp — Post-processing register reuse pass for RCC
 *
 * Copy-propagation and dead-store elimination on MASM x86-64 assembly text.
 * Runs after peephole optimization; reduces memory traffic from RCC's
 * "spill everything to stack" codegen strategy.
 *
 * Transformations (working on text lines):
 *
 *  T1 — Store-then-reload:
 *    mov [rbp-N], rax
 *    mov rax, [rbp-N]         → (delete reload; rax already holds value)
 *
 *  T2 — Dead store (write followed by write without intervening read):
 *    mov [rbp-N], rax
 *    ... (no read of [rbp-N] until next write)
 *    mov [rbp-N], rcx         → delete first store
 *
 *  T3 — Copy propagation through registers:
 *    mov rcx, rax             ; copy rax to rcx
 *    mov rax, rcx             ; copy back      → delete second mov (redundant)
 *    (Special case of store-then-reload at register level)
 *
 * Scope: only eliminates simple register-to-slot or slot-to-register patterns.
 * Does NOT eliminate if:
 *   - There is any memory-writing instruction (call, push, pop) between store and reload
 *   - The slot could be addressed via a pointer (conservative: only [rbp-N] slots)
 *   - The elimination would cross a label (control-flow boundary)
 */

#include "regalloc.h"
#include <vector>
#include <string>
#include <map>
#include <set>
#include <cstring>
#include <cstdio>

// ─── Utilities ──────────────────────────────────────────────────────────────

static std::vector<std::string> splitLines(const std::string& text) {
    std::vector<std::string> lines;
    size_t start = 0;
    for (size_t i = 0; i <= text.size(); i++) {
        if (i == text.size() || text[i] == '\n') {
            lines.push_back(text.substr(start, i - start));
            start = i + 1;
        }
    }
    return lines;
}

static std::string joinLines(const std::vector<std::string>& lines) {
    std::string result;
    for (const auto& l : lines) { result += l; result += '\n'; }
    return result;
}

static std::string trimLeft(const std::string& s) {
    size_t i = 0;
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) i++;
    return s.substr(i);
}

// Is this a code instruction (not label/directive/comment/blank)?
static bool isInstr(const std::string& line) {
    std::string t = trimLeft(line);
    if (t.empty()) return false;
    if (t[0] == ';') return false;
    if (t[0] == '.') return false;
    if (t.back() == ':') return false;
    if (t.find("PROC") != std::string::npos) return false;
    if (t.find("ENDP") != std::string::npos) return false;
    if (t.find("EXTERN") != std::string::npos) return false;
    if (t.find("PUBLIC") != std::string::npos) return false;
    if (t.find("INCLUDELIB") != std::string::npos) return false;
    return true;
}

// Is this a label or control-flow boundary?
static bool isLabel(const std::string& line) {
    std::string t = trimLeft(line);
    if (t.empty()) return false;
    // Lines ending in ':' are labels; also PROC/ENDP are function boundaries
    if (!t.empty() && t.back() == ':') return true;
    if (t.find("PROC") != std::string::npos) return true;
    if (t.find("ENDP") != std::string::npos) return true;
    return false;
}

// Extract stack slot offset from "QWORD PTR [rbp-N]" or "DWORD PTR [rbp-N]" etc.
// Returns true and sets offset (negative) if match; false otherwise.
static bool parseSlot(const std::string& operand, int* offset) {
    // operand should look like: "QWORD PTR [rbp-N]" or "DWORD PTR [rbp-N]" etc.
    size_t lbr = operand.find('[');
    if (lbr == std::string::npos) return false;
    size_t rbr = operand.find(']', lbr);
    if (rbr == std::string::npos) return false;
    std::string inner = operand.substr(lbr + 1, rbr - lbr - 1);
    // Expect "rbp-N" (N is positive) or "rbp+N"
    if (inner.substr(0, 3) != "rbp") return false;
    if (inner.size() <= 3) return false;
    char sign = inner[3];
    if (sign != '-' && sign != '+') return false;
    std::string num = inner.substr(4);
    char* ep = nullptr;
    long val = strtol(num.c_str(), &ep, 10);
    if (!ep || *ep != '\0') return false;
    *offset = (sign == '-') ? -(int)val : (int)val;
    return true;
}

// Parse a store instruction: "mov PTR_SIZE PTR [rbp-N], REG"
// Returns true, sets slot and reg; false if not a stack store.
static bool parseStore(const std::string& line, int* slot, std::string* src_reg) {
    std::string t = trimLeft(line);
    if (t.substr(0, 4) != "mov ") return false;
    // t: "mov QWORD PTR [rbp-N], REG"
    size_t comma = t.rfind(','); // last comma separates dst, src
    if (comma == std::string::npos) return false;
    std::string dst = t.substr(4, comma - 4);
    while (!dst.empty() && dst.back() == ' ') dst.pop_back();
    std::string src = t.substr(comma + 1);
    while (!src.empty() && src.front() == ' ') src.erase(src.begin());
    while (!src.empty() && src.back() == ' ') src.pop_back();
    // dst must contain '[rbp'
    if (dst.find("[rbp") == std::string::npos) return false;
    // src must be a register (no '[', no ' ', no digits-only)
    if (src.find('[') != std::string::npos) return false;
    if (src.find("PTR") != std::string::npos) return false;
    if (src.empty()) return false;
    // Parse slot
    if (!parseSlot(dst, slot)) return false;
    *src_reg = src;
    return true;
}

// Parse a load instruction: "mov REG, PTR_SIZE PTR [rbp-N]"
// Returns true, sets dst_reg and slot; false if not a stack load.
static bool parseLoad(const std::string& line, std::string* dst_reg, int* slot) {
    std::string t = trimLeft(line);
    if (t.substr(0, 4) != "mov ") return false;
    size_t comma = t.find(',');
    if (comma == std::string::npos) return false;
    std::string dst = t.substr(4, comma - 4);
    while (!dst.empty() && dst.back() == ' ') dst.pop_back();
    std::string src = t.substr(comma + 1);
    while (!src.empty() && src.front() == ' ') src.erase(src.begin());
    while (!src.empty() && src.back() == ' ') src.pop_back();
    // dst must be a register (no '[')
    if (dst.find('[') != std::string::npos) return false;
    if (dst.find("PTR") != std::string::npos) return false;
    if (dst.empty()) return false;
    // src must contain '[rbp'
    if (src.find("[rbp") == std::string::npos) return false;
    if (!parseSlot(src, slot)) return false;
    *dst_reg = dst;
    return true;
}

// True if a line might write memory (call, push, pop — conservative)
static bool mayClobberMemory(const std::string& line) {
    std::string t = trimLeft(line);
    if (t.substr(0, 5) == "call ") return true;
    if (t.substr(0, 4) == "push") return true;
    if (t.substr(0, 3) == "pop") return true;
    return false;
}

// True if a line mentions a given stack slot (reads or writes [rbp-N])
static bool mentionsSlot(const std::string& line, int slot) {
    std::string t = trimLeft(line);
    if (t.empty()) return false;
    // Look for [rbp+/-N] in the line
    size_t pos = t.find("[rbp");
    while (pos != std::string::npos) {
        size_t rbr = t.find(']', pos);
        if (rbr != std::string::npos) {
            std::string inner = t.substr(pos + 1, rbr - pos - 1); // e.g. "rbp-16"
            if (inner.size() > 3 && inner.substr(0, 3) == "rbp") {
                char sign = inner[3];
                std::string num = inner.substr(4);
                char* ep = nullptr;
                long val = strtol(num.c_str(), &ep, 10);
                if (ep && *ep == '\0') {
                    int found_slot = (sign == '-') ? -(int)val : (int)val;
                    if (found_slot == slot) return true;
                }
            }
        }
        pos = t.find("[rbp", pos + 1);
    }
    return false;
}

// ─── T1: Store-then-reload elimination ──────────────────────────────────────
// Pattern: mov [rbp-N], REG  followed immediately (no labels/calls) by
//          mov REG2, [rbp-N]  → delete the load; REG2 already has the value
//          (only when REG == REG2, or when REG and REG2 are the same register family)
static int applyT1(std::vector<std::string>& lines) {
    int changes = 0;
    for (size_t i = 0; i + 1 < lines.size(); i++) {
        if (!isInstr(lines[i])) continue;
        int slot_store; std::string store_reg;
        if (!parseStore(lines[i], &slot_store, &store_reg)) continue;

        // Find the very next instruction (skip blank/comment lines, but stop at labels)
        size_t j = i + 1;
        while (j < lines.size() && !isInstr(lines[j]) && !isLabel(lines[j]) &&
               trimLeft(lines[j]).empty()) j++;
        if (j >= lines.size() || !isInstr(lines[j])) continue;
        if (isLabel(lines[j])) continue; // control-flow boundary

        int slot_load; std::string load_reg;
        if (!parseLoad(lines[j], &load_reg, &slot_load)) continue;
        if (slot_store != slot_load) continue;
        if (load_reg != store_reg) continue; // only eliminate if same register

        // Delete the load (lines[j]) — the register already holds the value
        lines[j] = "";
        changes++;
        i = j; // advance
    }
    return changes;
}

// ─── T2: Dead store elimination ─────────────────────────────────────────────
// Pattern: mov [rbp-N], REG_A followed by mov [rbp-N], REG_B with no read
//          of [rbp-N] between them → delete the first store.
// Conservative: abort if any call/push/pop or label appears between them.
static int applyT2(std::vector<std::string>& lines) {
    int changes = 0;
    for (size_t i = 0; i < lines.size(); i++) {
        if (!isInstr(lines[i])) continue;
        int slot1; std::string reg1;
        if (!parseStore(lines[i], &slot1, &reg1)) continue;

        // Scan forward for a second store to the same slot
        bool ok = true;
        size_t j = i + 1;
        for (; j < lines.size(); j++) {
            if (isLabel(lines[j])) { ok = false; break; } // control flow — unsafe
            if (!isInstr(lines[j])) continue;
            if (mayClobberMemory(lines[j])) { ok = false; break; }

            // Check if this line READS the slot (as load destination)
            int ls; std::string lr;
            if (parseLoad(lines[j], &lr, &ls) && ls == slot1) { ok = false; break; }
            // Also bail if slot is referenced in any other way
            if (mentionsSlot(lines[j], slot1)) {
                // Is this line a store to slot1?
                int ss; std::string sr;
                if (parseStore(lines[j], &ss, &sr) && ss == slot1) {
                    // Found second store — first is dead
                    break;
                }
                ok = false; break;
            }
        }
        if (!ok || j >= lines.size()) continue;
        // Verify j is a second store to slot1
        int ss; std::string sr;
        if (!parseStore(lines[j], &ss, &sr) || ss != slot1) continue;

        // Delete the first (dead) store
        lines[i] = "";
        changes++;
    }
    return changes;
}

// ─── T3: Register copy round-trip elimination ────────────────────────────────
// Pattern: mov rcx, rax  /  mov rax, rcx  → delete second (rax already = value)
// Only handles adjacent pairs with no intervening instructions.
static int applyT3(std::vector<std::string>& lines) {
    int changes = 0;
    for (size_t i = 0; i + 1 < lines.size(); i++) {
        if (!isInstr(lines[i]) || !isInstr(lines[i+1])) continue;
        std::string a = trimLeft(lines[i]);
        std::string b = trimLeft(lines[i+1]);
        if (a.substr(0, 4) != "mov " || b.substr(0, 4) != "mov ") continue;
        size_t ac = a.find(','), bc = b.find(',');
        if (ac == std::string::npos || bc == std::string::npos) continue;
        std::string da = a.substr(4, ac - 4); // dst of first mov
        std::string sa = a.substr(ac + 1);    // src of first mov
        std::string db = b.substr(4, bc - 4); // dst of second mov
        std::string sb = b.substr(bc + 1);    // src of second mov
        // Trim
        auto trim = [](std::string& s) {
            while (!s.empty() && (s.front()==' '||s.front()=='\t')) s.erase(s.begin());
            while (!s.empty() && (s.back()==' '||s.back()=='\t')) s.pop_back();
        };
        trim(da); trim(sa); trim(db); trim(sb);
        // Must be register-to-register (no brackets or PTR)
        for (auto* s : {&da,&sa,&db,&sb}) {
            if (s->find('[') != std::string::npos || s->find("PTR") != std::string::npos)
                goto next_pair;
        }
        // Pattern: "mov DA, SA" followed by "mov SA, DA" (copy back)
        if (da == sb && sa == db) {
            // Second mov copies first back — delete it (SA already has the value)
            lines[i+1] = "";
            changes++;
            i++; // skip
        }
        next_pair:;
    }
    return changes;
}

// ─── T4: Dead load elimination ───────────────────────────────────────────────
// Pattern: two loads into the same register from the same slot, no intervening
// modification of the slot or the register between them → delete the second load.
//   mov rax, [rbp-N]   ; first load
//   ... (no write to rax or [rbp-N]) ...
//   mov rax, [rbp-N]   ; second load — rax already has [rbp-N]'s value → DELETE
//
// Conservative: abort if any call, push, pop, or label appears between them.
static int applyT4(std::vector<std::string>& lines) {
    int changes = 0;
    for (size_t i = 0; i < lines.size(); i++) {
        if (!isInstr(lines[i])) continue;
        std::string dst_reg1; int slot1;
        if (!parseLoad(lines[i], &dst_reg1, &slot1)) continue;

        // Scan forward: find a second load from same slot into same register
        bool ok = true;
        size_t j = i + 1;
        for (; j < lines.size(); j++) {
            if (isLabel(lines[j])) { ok = false; break; }
            if (!isInstr(lines[j])) continue;
            if (mayClobberMemory(lines[j])) { ok = false; break; }
            // If the register is used as a destination (overwritten), abort
            std::string t = trimLeft(lines[j]);
            if (t.substr(0, 4) == "mov ") {
                size_t cm = t.find(',');
                if (cm != std::string::npos) {
                    std::string ddst = t.substr(4, cm - 4);
                    while (!ddst.empty() && (ddst.back()==' '||ddst.back()=='\t')) ddst.pop_back();
                    while (!ddst.empty() && (ddst.front()==' '||ddst.front()=='\t')) ddst.erase(ddst.begin());
                    if (ddst == dst_reg1) { ok = false; break; } // register overwritten
                }
            }
            // If slot is written, abort
            int ss; std::string sr;
            if (parseStore(lines[j], &ss, &sr) && ss == slot1) { ok = false; break; }
            // Check for second load from same slot into same register
            std::string dst_reg2; int slot2;
            if (parseLoad(lines[j], &dst_reg2, &slot2) && slot2 == slot1 && dst_reg2 == dst_reg1) {
                // Found duplicate load — delete the second one
                lines[j] = "";
                changes++;
                break;
            }
        }
        if (!ok) continue;
    }
    return changes;
}

// ─── T5: movsxd redundancy: movsxd r64, r32 where r32 came from a DWORD load ─
// Pattern:
//   mov eax, DWORD PTR [rbp-N]    ; eax = 32-bit value (zero-extends rax)
//   movsxd rax, eax               ; sign-extend to rax (redundant if we use eax as-is)
//
// Actually, the pattern RCC commonly emits is:
//   movsxd rax, DWORD PTR [rbp-N]
//   push rax
//   movsxd rax, DWORD PTR [rbp-N]  ← redundant duplicate if no write between
//
// This is a special case of T4 for movsxd loads.  Handle it by detecting
// consecutive identical movsxd instructions.
static int applyT5(std::vector<std::string>& lines) {
    int changes = 0;
    for (size_t i = 0; i + 1 < lines.size(); i++) {
        if (!isInstr(lines[i]) || !isInstr(lines[i+1])) continue;
        std::string a = trimLeft(lines[i]);
        std::string b = trimLeft(lines[i+1]);
        // Skip blank (already deleted) lines
        if (a.empty() || b.empty()) continue;
        // Both must be movsxd with identical operands
        if (a.substr(0, 7) == "movsxd " && a == b) {
            lines[i+1] = "";
            changes++;
            i++;
        }
    }
    return changes;
}

// ─── T6: imul small-constant → LEA / SHL ─────────────────────────────────────
// Pattern: imul REG, N where N ∈ {2,3,4,5,8,9} → equivalent LEA or SHL
//   N=2: lea reg,[reg+reg]    N=3: lea reg,[reg+reg*2]  N=4: shl reg,2
//   N=5: lea reg,[reg+reg*4]  N=8: shl reg,3            N=9: lea reg,[reg+reg*8]
// LEA avoids the multiplier latency penalty on most x64 microarchitectures.
static int applyT6(std::vector<std::string>& lines) {
    int changes = 0;
    for (size_t i = 0; i < lines.size(); i++) {
        if (!isInstr(lines[i])) continue;
        std::string t = trimLeft(lines[i]);
        if (t.size() < 8 || t.substr(0, 5) != "imul ") continue;
        // Parse "imul DST, N"
        size_t comma = t.find(',');
        if (comma == std::string::npos) continue;
        std::string dst = t.substr(5, comma - 5);
        while (!dst.empty() && (dst.back() == ' ' || dst.back() == '\t')) dst.pop_back();
        while (!dst.empty() && (dst.front() == ' ' || dst.front() == '\t')) dst.erase(dst.begin());
        std::string src = t.substr(comma + 1);
        while (!src.empty() && (src.front() == ' ' || src.front() == '\t')) src.erase(src.begin());
        while (!src.empty() && (src.back() == ' ' || src.back() == '\t')) src.pop_back();
        // Src must be a plain integer literal (no brackets, no spaces, no registers)
        if (src.find('[') != std::string::npos || src.find(' ') != std::string::npos) continue;
        char* ep = nullptr;
        long n = strtol(src.c_str(), &ep, 10);
        if (!ep || *ep != '\0') continue;
        // Extract leading indentation from original line
        std::string indent;
        for (size_t ci = 0; ci < lines[i].size() && (lines[i][ci]==' '||lines[i][ci]=='\t'); ci++)
            indent += lines[i][ci];
        std::string repl;
        switch (n) {
            case 2: repl = indent + "lea " + dst + ", [" + dst + "+" + dst + "]"; break;
            case 3: repl = indent + "lea " + dst + ", [" + dst + "+" + dst + "*2]"; break;
            case 4: repl = indent + "shl " + dst + ", 2"; break;
            case 5: repl = indent + "lea " + dst + ", [" + dst + "+" + dst + "*4]"; break;
            case 8: repl = indent + "shl " + dst + ", 3"; break;
            case 9: repl = indent + "lea " + dst + ", [" + dst + "+" + dst + "*8]"; break;
            default: continue;
        }
        lines[i] = repl;
        changes++;
    }
    return changes;
}

// ─── T7: Dead add/sub 0 elimination ──────────────────────────────────────────
// Pattern: `add REG, 0` or `sub REG, 0` → delete (arithmetic no-ops)
static int applyT7(std::vector<std::string>& lines) {
    int changes = 0;
    for (size_t i = 0; i < lines.size(); i++) {
        if (!isInstr(lines[i])) continue;
        std::string t = trimLeft(lines[i]);
        if (t.size() < 8) continue;
        bool is_add_sub = (t.substr(0, 4) == "add " || t.substr(0, 4) == "sub ");
        if (!is_add_sub) continue;
        size_t comma = t.find(',');
        if (comma == std::string::npos) continue;
        std::string src = t.substr(comma + 1);
        while (!src.empty() && (src.front() == ' ' || src.front() == '\t')) src.erase(src.begin());
        while (!src.empty() && (src.back() == ' ' || src.back() == '\t')) src.pop_back();
        if (src == "0") { lines[i] = ""; changes++; }
    }
    return changes;
}

// ─── Main entry point ────────────────────────────────────────────────────────

int regAllocPass(std::string& asm_text) {
    std::vector<std::string> lines = splitLines(asm_text);
    int total = 0;

    // Apply transformations in fixpoint loop (capped at 4 passes)
    for (int pass = 0; pass < 4; pass++) {
        int changed = 0;
        changed += applyT1(lines); // store-then-reload
        changed += applyT3(lines); // register copy round-trip
        changed += applyT2(lines); // dead store
        changed += applyT4(lines); // duplicate load elimination (3.1)
        changed += applyT5(lines); // consecutive identical movsxd elimination (3.2)
        changed += applyT6(lines); // imul small-constant -> LEA/SHL (T6)
        changed += applyT7(lines); // dead add/sub 0 (T7)
        if (changed == 0) break;
        total += changed;
    }

    asm_text = joinLines(lines);
    return total;
}
