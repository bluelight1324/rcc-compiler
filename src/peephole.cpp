/**
 * Peephole Optimizer — Phase E (Section 3.3)
 *
 * Local pattern-replacement optimizations on emitted MASM x86-64 assembly.
 * Each pattern is a pair (match_fn, replace_fn) that operates on a sliding
 * window of instruction lines.
 *
 * Patterns implemented:
 *   P1: Redundant load-store elimination
 *       mov rax, QWORD PTR [rbp-N]
 *       mov QWORD PTR [rbp-N], rax         → (delete both)
 *
 *   P2: Zero-operand elimination
 *       add rax, 0                          → (delete)
 *       sub rax, 0                          → (delete)
 *       imul rax, 1                         → (delete)
 *
 *   P3: Power-of-2 multiply → shift
 *       imul rax, 2                         → shl rax, 1
 *       imul rax, 4                         → shl rax, 2
 *       imul rax, 8                         → shl rax, 3
 *       imul rax, 16                        → shl rax, 4
 *       imul eax, 2                         → shl eax, 1
 *       ... (32 and 64-bit variants)
 *
 *   P4: Redundant sign-extension after DWORD store/load
 *       mov DWORD PTR [rbp-N], eax
 *       movsxd rax, DWORD PTR [rbp-N]       → mov DWORD PTR [rbp-N], eax
 *                                              movsxd rax, eax
 *       (avoids redundant memory round-trip)
 *
 *   P5: Self-move elimination
 *       mov rax, rax                        → (delete)
 *       mov eax, eax                        → (delete)
 *
 *   P6: Push/pop consolidation
 *       push rax
 *       pop rax                             → (delete both)
 *       push rcx
 *       pop rcx                             → (delete both)
 *
 *   P7: Compare-to-zero → test (shorter encoding, identical flags)
 *       cmp rax, 0                          → test rax, rax
 *       cmp eax, 0                          → test eax, eax
 *       ... (all common registers)
 *
 *   P9: Strength reduction for non-power-of-2 multiplies
 *       imul rax, 3                         → lea rax, [rax+rax*2]
 *       imul rax, 5                         → lea rax, [rax+rax*4]
 *       imul rax, 9                         → lea rax, [rax+rax*8]
 *
 *   P10: mov reg, 0  → xor reg32, reg32  (zero extension, 2 bytes shorter)
 *
 *   P12: add reg, 1  → inc reg          (1 byte shorter)
 *        sub reg, 1  → dec reg          (1 byte shorter)
 *
 *   P13: mov reg, A / op reg, B  → mov reg, (A op B)   (constant folding, -O2 only)
 *        where op ∈ {imul, add, sub}
 *
 *   P14: Dead-mov / store-then-reload elimination (-O2 only)
 *        mov REG, IMM1 / mov REG, IMM2  → mov REG, IMM2  (dead constant assignment)
 *        imul REG, 0                    → xor REG32, REG32  (multiply-by-zero)
 *
 *   P11: CMOV fusion (conditional branch over register move, -O2 only)
 *        cmp  REG1, REG2                → cmp  REG1, REG2
 *        jCC  .L                           cmovINV REG1, REG2
 *        mov  REG1, REG2               (jCC and mov deleted, .L kept)
 *        .L:
 */

#include "peephole.h"
#include <vector>
#include <string>
#include <algorithm>
#include <cstring>
#include <cstdlib>

// ─── Utilities ──────────────────────────────────────────────────────────────

// Split text into lines, preserving \n
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

// Join lines with \n
static std::string joinLines(const std::vector<std::string>& lines) {
    std::string result;
    for (const auto& l : lines) {
        result += l;
        result += '\n';
    }
    return result;
}

// Trim leading whitespace from a line copy
static std::string trimLeft(const std::string& s) {
    size_t i = 0;
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) i++;
    return s.substr(i);
}

// True if the line is a code instruction (not a label, directive, comment, blank)
static bool isInstr(const std::string& line) {
    std::string t = trimLeft(line);
    if (t.empty()) return false;
    if (t[0] == ';') return false;   // comment
    if (t[0] == '_' || (t.size() > 1 && t.back() == ':')) return false; // label
    if (t[0] == '.') return false;   // directive
    // Check for PROC/ENDP keywords (not instructions)
    if (t.find("PROC") != std::string::npos) return false;
    if (t.find("ENDP") != std::string::npos) return false;
    if (t.find("EXTERN") != std::string::npos) return false;
    if (t.find("PUBLIC") != std::string::npos) return false;
    if (t.find("INCLUDELIB") != std::string::npos) return false;
    return true;
}

// True if the trimmed line matches (case-sensitive prefix)
static bool lineIs(const std::string& line, const char* prefix) {
    std::string t = trimLeft(line);
    return t.rfind(prefix, 0) == 0;
}

// True if the trimmed line is exactly equal to the given string
static bool lineEq(const std::string& line, const char* target) {
    return trimLeft(line) == target;
}

// Extract the rbp offset from a [rbp±N] memory operand string
// Returns true and sets offset if found, false otherwise
static bool extractRbpOffset(const std::string& s, long long& offset) {
    size_t pos = s.find("[rbp");
    if (pos == std::string::npos) return false;
    pos += 4; // skip "[rbp"
    if (pos >= s.size()) return false;
    char sign = s[pos];
    if (sign != '+' && sign != '-') return false;
    pos++;
    long long n = 0;
    bool has_digit = false;
    while (pos < s.size() && s[pos] >= '0' && s[pos] <= '9') {
        n = n * 10 + (s[pos] - '0');
        pos++;
        has_digit = true;
    }
    if (!has_digit) return false;
    offset = (sign == '-') ? -n : n;
    return true;
}

// ─── Pattern implementations ─────────────────────────────────────────────────

// P1: Redundant load-store elimination
//   mov rax, QWORD PTR [rbp-N]
//   mov QWORD PTR [rbp-N], rax     → delete both (no-op round-trip)
static int applyP1(std::vector<std::string>& lines) {
    int changes = 0;
    for (size_t i = 0; i + 1 < lines.size(); i++) {
        if (!isInstr(lines[i]) || !isInstr(lines[i+1])) continue;
        std::string a = trimLeft(lines[i]);
        std::string b = trimLeft(lines[i+1]);

        // Pattern: mov REG, QWORD PTR [rbp-N] followed by mov QWORD PTR [rbp-N], REG
        // Only handles rax/eax for now (most common case)
        static const struct { const char* load; const char* store; } pairs[] = {
            { "mov rax, QWORD PTR [rbp", "mov QWORD PTR [rbp" },
            { "mov eax, DWORD PTR [rbp", "mov DWORD PTR [rbp" },
            { "mov rcx, QWORD PTR [rbp", "mov QWORD PTR [rbp" },
            { "mov ecx, DWORD PTR [rbp", "mov DWORD PTR [rbp" },
        };
        for (auto& p : pairs) {
            if (a.rfind(p.load, 0) != 0) continue;
            if (b.rfind(p.store, 0) != 0) continue;

            // Extract the register from the load
            size_t comma = a.find(',');
            if (comma == std::string::npos) continue;
            std::string reg = a.substr(4, comma - 4); // after "mov "
            // Check store ends with ", reg"
            std::string expected_suffix = ", " + reg;
            if (b.size() < expected_suffix.size()) continue;
            if (b.substr(b.size() - expected_suffix.size()) != expected_suffix) continue;

            // Confirm same rbp offset in both
            long long off_a, off_b;
            if (!extractRbpOffset(a, off_a) || !extractRbpOffset(b, off_b)) continue;
            if (off_a != off_b) continue;

            // Delete both lines (mark as empty comment)
            lines[i]   = "    ; [peephole P1: redundant load-store removed]";
            lines[i+1] = "";
            i++; // skip next line
            changes++;
            break;
        }
    }
    return changes;
}

// P2: Zero-operand elimination
//   add rax, 0  / add eax, 0  / add rsp, 0
//   sub rax, 0  / sub eax, 0
//   imul rax, 1 / imul eax, 1
static int applyP2(std::vector<std::string>& lines) {
    int changes = 0;
    static const char* zero_add[] = {
        "add rax, 0", "add eax, 0", "add rcx, 0", "add ecx, 0",
        "add rdx, 0", "add edx, 0", "add rsp, 0", "add rbx, 0",
        "sub rax, 0", "sub eax, 0", "sub rcx, 0", "sub ecx, 0",
        "sub rdx, 0", "sub edx, 0", "sub rsp, 0",
        "imul rax, 1", "imul eax, 1", "imul rcx, 1", "imul ecx, 1",
        nullptr
    };
    for (auto& line : lines) {
        std::string t = trimLeft(line);
        for (const char** p = zero_add; *p; p++) {
            if (t == *p) {
                line = "    ; [peephole P2: zero op removed]";
                changes++;
                break;
            }
        }
    }
    return changes;
}

// P3: Power-of-2 multiply → shift
//   imul rax, N  →  shl rax, log2(N)   (for N = 2, 4, 8, 16, 32, 64)
static int applyP3(std::vector<std::string>& lines) {
    int changes = 0;
    static const struct { int factor; int shift; } pow2[] = {
        {2,1},{4,2},{8,3},{16,4},{32,5},{64,6},{128,7},{256,8}
    };
    for (auto& line : lines) {
        std::string t = trimLeft(line);
        for (auto& f : pow2) {
            char pat64[64], pat32[64];
            snprintf(pat64, sizeof(pat64), "imul rax, %d", f.factor);
            snprintf(pat32, sizeof(pat32), "imul eax, %d", f.factor);
            if (t == pat64) {
                char rep[64]; snprintf(rep, sizeof(rep), "    shl rax, %d", f.shift);
                line = rep;
                changes++; break;
            }
            if (t == pat32) {
                char rep[64]; snprintf(rep, sizeof(rep), "    shl eax, %d", f.shift);
                line = rep;
                changes++; break;
            }
        }
    }
    return changes;
}

// P5: Self-move elimination  mov rax, rax  /  mov eax, eax  etc.
static int applyP5(std::vector<std::string>& lines) {
    int changes = 0;
    static const char* self_moves[] = {
        "mov rax, rax", "mov eax, eax",
        "mov rcx, rcx", "mov ecx, ecx",
        "mov rdx, rdx", "mov edx, edx",
        "mov rbx, rbx", "mov ebx, ebx",
        "mov rsp, rsp", "mov esp, esp",
        "mov rbp, rbp", "mov ebp, ebp",
        nullptr
    };
    for (auto& line : lines) {
        std::string t = trimLeft(line);
        for (const char** p = self_moves; *p; p++) {
            if (t == *p) {
                line = "    ; [peephole P5: self-move removed]";
                changes++;
                break;
            }
        }
    }
    return changes;
}

// P6: Push/pop consolidation  push R; pop R  → (delete both)
static int applyP6(std::vector<std::string>& lines) {
    int changes = 0;
    static const char* regs[] = {
        "rax","eax","rcx","ecx","rdx","edx","rbx","ebx",
        "r8","r9","r10","r11","r12","r13","r14","r15",
        nullptr
    };
    for (size_t i = 0; i + 1 < lines.size(); i++) {
        if (!isInstr(lines[i]) || !isInstr(lines[i+1])) continue;
        std::string a = trimLeft(lines[i]);
        std::string b = trimLeft(lines[i+1]);
        for (const char** r = regs; *r; r++) {
            char push_pat[32], pop_pat[32];
            snprintf(push_pat, sizeof(push_pat), "push %s", *r);
            snprintf(pop_pat,  sizeof(pop_pat),  "pop %s",  *r);
            if (a == push_pat && b == pop_pat) {
                lines[i]   = "    ; [peephole P6: push/pop pair removed]";
                lines[i+1] = "";
                i++;
                changes++;
                break;
            }
        }
    }
    return changes;
}

// P4: Redundant sign-extension via memory round-trip (documented but not yet called)
//   mov DWORD PTR [rbp-N], eax
//   movsxd rax, DWORD PTR [rbp-N]   →   movsxd rax, eax   (avoids memory load)
static int applyP4(std::vector<std::string>& lines) {
    int changes = 0;
    for (size_t i = 0; i + 1 < lines.size(); i++) {
        if (!isInstr(lines[i]) || !isInstr(lines[i+1])) continue;
        std::string a = trimLeft(lines[i]);
        std::string b = trimLeft(lines[i+1]);
        // Pattern: store eax to [rbp-N], then movsxd from [rbp-N]
        if (a.rfind("mov DWORD PTR [rbp", 0) != 0) continue;
        if (b.rfind("movsxd rax, DWORD PTR [rbp", 0) != 0) continue;
        // Confirm the store uses ", eax" suffix
        if (a.size() < 5 || a.substr(a.size() - 5) != ", eax") continue;
        // Confirm same rbp offset in both
        long long off_a, off_b;
        if (!extractRbpOffset(a, off_a) || !extractRbpOffset(b, off_b)) continue;
        if (off_a != off_b) continue;
        // Replace memory-based movsxd with direct register form
        lines[i+1] = "    movsxd rax, eax";
        changes++;
        i++;  // skip modified line
    }
    return changes;
}

// P7: Compare-to-zero → test (saves 1–3 encoding bytes; flags are identical)
//   cmp rax, 0   →   test rax, rax
static int applyP7(std::vector<std::string>& lines) {
    int changes = 0;
    static const struct { const char* from; const char* to; } tab[] = {
        { "cmp rax, 0", "    test rax, rax" },
        { "cmp eax, 0", "    test eax, eax" },
        { "cmp rcx, 0", "    test rcx, rcx" },
        { "cmp ecx, 0", "    test ecx, ecx" },
        { "cmp rdx, 0", "    test rdx, rdx" },
        { "cmp edx, 0", "    test edx, edx" },
        { "cmp rbx, 0", "    test rbx, rbx" },
        { "cmp ebx, 0", "    test ebx, ebx" },
        { "cmp r8, 0",  "    test r8, r8"   },
        { "cmp r9, 0",  "    test r9, r9"   },
        { nullptr, nullptr }
    };
    for (auto& line : lines) {
        if (!isInstr(line)) continue;
        std::string t = trimLeft(line);
        for (auto* p = tab; p->from; p++) {
            if (t == p->from) { line = p->to; changes++; break; }
        }
    }
    return changes;
}

// P9: Strength reduction — non-power-of-2 multiply → LEA
//   imul rax, 3   →   lea rax, [rax+rax*2]
//   imul rax, 5   →   lea rax, [rax+rax*4]
//   imul rax, 9   →   lea rax, [rax+rax*8]
static int applyP9(std::vector<std::string>& lines) {
    int changes = 0;
    static const struct { const char* from64; const char* from32; const char* to64; const char* to32; } tab[] = {
        { "imul rax, 3", "imul eax, 3", "    lea rax, [rax+rax*2]", "    lea eax, [eax+eax*2]" },
        { "imul rax, 5", "imul eax, 5", "    lea rax, [rax+rax*4]", "    lea eax, [eax+eax*4]" },
        { "imul rax, 9", "imul eax, 9", "    lea rax, [rax+rax*8]", "    lea eax, [eax+eax*8]" },
        { "imul rcx, 3", "imul ecx, 3", "    lea rcx, [rcx+rcx*2]", "    lea ecx, [ecx+ecx*2]" },
        { "imul rcx, 5", "imul ecx, 5", "    lea rcx, [rcx+rcx*4]", "    lea ecx, [ecx+ecx*4]" },
        { "imul rcx, 9", "imul ecx, 9", "    lea rcx, [rcx+rcx*8]", "    lea ecx, [ecx+ecx*8]" },
        { nullptr, nullptr, nullptr, nullptr }
    };
    for (auto& line : lines) {
        if (!isInstr(line)) continue;
        std::string t = trimLeft(line);
        for (auto* e = tab; e->from64; e++) {
            if (t == e->from64) { line = e->to64; changes++; break; }
            if (t == e->from32) { line = e->to32; changes++; break; }
        }
    }
    return changes;
}

// P10: mov reg, 0 → xor reg32, reg32 (1-2 bytes shorter; zero-extends to full 64-bit)
static int applyP10(std::vector<std::string>& lines) {
    int changes = 0;
    static const struct { const char* from; const char* to; } tab[] = {
        { "mov rax, 0", "    xor eax, eax" },
        { "mov rcx, 0", "    xor ecx, ecx" },
        { "mov rdx, 0", "    xor edx, edx" },
        { "mov rbx, 0", "    xor ebx, ebx" },
        { "mov r8, 0",  "    xor r8d, r8d" },
        { "mov r9, 0",  "    xor r9d, r9d" },
        { "mov r10, 0", "    xor r10d, r10d" },
        { "mov r11, 0", "    xor r11d, r11d" },
        { nullptr, nullptr }
    };
    for (auto& line : lines) {
        if (!isInstr(line)) continue;
        std::string t = trimLeft(line);
        for (auto* p = tab; p->from; p++) {
            if (t == p->from) { line = p->to; changes++; break; }
        }
    }
    return changes;
}

// P12: add reg, 1 → inc reg  /  sub reg, 1 → dec reg  (1 byte shorter)
static int applyP12(std::vector<std::string>& lines) {
    int changes = 0;
    static const struct { const char* from; const char* to; } tab[] = {
        { "add rax, 1", "    inc rax" }, { "sub rax, 1", "    dec rax" },
        { "add rcx, 1", "    inc rcx" }, { "sub rcx, 1", "    dec rcx" },
        { "add rdx, 1", "    inc rdx" }, { "sub rdx, 1", "    dec rdx" },
        { "add rbx, 1", "    inc rbx" }, { "sub rbx, 1", "    dec rbx" },
        { "add eax, 1", "    inc eax" }, { "sub eax, 1", "    dec eax" },
        { "add ecx, 1", "    inc ecx" }, { "sub ecx, 1", "    dec ecx" },
        { "add edx, 1", "    inc edx" }, { "sub edx, 1", "    dec edx" },
        { "add r8, 1",  "    inc r8"  }, { "sub r8, 1",  "    dec r8"  },
        { "add r9, 1",  "    inc r9"  }, { "sub r9, 1",  "    dec r9"  },
        { nullptr, nullptr }
    };
    for (auto& line : lines) {
        if (!isInstr(line)) continue;
        std::string t = trimLeft(line);
        for (auto* p = tab; p->from; p++) {
            if (t == p->from) { line = p->to; changes++; break; }
        }
    }
    return changes;
}

// ─── P13: -O2 Constant folding (two-line window) ─────────────────────────────
// Folds: mov reg, IMM1 followed by arithmetic on the same reg with IMM2.
//   mov rax, 4 / imul rax, 3  → mov rax, 12
//   mov rax, 5 / add rax, 3   → mov rax, 8
//   mov rax, 8 / sub rax, 3   → mov rax, 5
static int applyP13(std::vector<std::string>& lines) {
    int changes = 0;
    for (size_t i = 0; i + 1 < lines.size(); i++) {
        if (!isInstr(lines[i]) || !isInstr(lines[i+1])) continue;
        std::string a = trimLeft(lines[i]);
        std::string b = trimLeft(lines[i+1]);
        // Match: "mov REG, NUM"
        if (a.substr(0, 4) != "mov ") continue;
        // Find comma separating dest and src in a
        size_t ac = a.find(',');
        if (ac == std::string::npos) continue;
        std::string reg = a.substr(4, ac - 4);
        // Trim reg
        while (!reg.empty() && reg.back() == ' ') reg.pop_back();
        std::string src_a = a.substr(ac + 1);
        while (!src_a.empty() && src_a.front() == ' ') src_a.erase(src_a.begin());
        // src_a must be a plain integer (no ptr, no brackets)
        char* endp = nullptr;
        long long val_a = strtoll(src_a.c_str(), &endp, 10);
        if (!endp || *endp != '\0') continue; // not a plain integer
        // Match: "OP REG, NUM" on second line  where OP ∈ {imul, add, sub}
        size_t bc = b.find(',');
        if (bc == std::string::npos) continue;
        std::string op_reg2;
        std::string op2;
        if (b.substr(0, 5) == "imul ") { op2 = "imul"; op_reg2 = b.substr(5, bc - 5); }
        else if (b.substr(0, 4) == "add ") { op2 = "add";  op_reg2 = b.substr(4, bc - 4); }
        else if (b.substr(0, 4) == "sub ") { op2 = "sub";  op_reg2 = b.substr(4, bc - 4); }
        else continue;
        while (!op_reg2.empty() && op_reg2.back() == ' ') op_reg2.pop_back();
        if (op_reg2 != reg) continue; // must be same register
        std::string src_b = b.substr(bc + 1);
        while (!src_b.empty() && src_b.front() == ' ') src_b.erase(src_b.begin());
        long long val_b = strtoll(src_b.c_str(), &endp, 10);
        if (!endp || *endp != '\0') continue;
        // Fold the constant
        long long result;
        if (op2 == "imul") result = val_a * val_b;
        else if (op2 == "add") result = val_a + val_b;
        else result = val_a - val_b;
        // Replace both lines: delete second, update first
        char buf[64];
        snprintf(buf, sizeof(buf), "    mov %s, %lld", reg.c_str(), (long long)result);
        lines[i] = buf;
        lines[i+1] = "";  // will be removed by joinLines (empty string)
        changes++;
        i++; // skip next line (already processed)
    }
    return changes;
}

// ─── P11: CMOV fusion (conditional branch over register move) ────────────────
// Detects:  cmp REG1, REG2  /  jCC .Lxxx  /  mov REG1, REG2  /  .Lxxx:
// Replaces: cmp REG1, REG2  /  cmovINV REG1, REG2             /  .Lxxx:
// Only fuses register-to-register patterns (not memory or immediate).
// Safe even when .Lxxx is the target of other jumps (label kept, semantics preserved).
static int applyP11(std::vector<std::string>& lines) {
    int changes = 0;
    // jCC → inverse cmov  (jCC jumps over the move; cmovINV moves when jCC wouldn't jump)
    static const struct { const char* jcc; const char* cmov; } cc_map[] = {
        { "jge ",  "cmovl"  }, { "jle ",  "cmovg"  },
        { "jg ",   "cmovle" }, { "jl ",   "cmovge" },
        { "je ",   "cmovne" }, { "jne ",  "cmove"  },
        { "jae ",  "cmovb"  }, { "jbe ",  "cmova"  },
        { "ja ",   "cmovbe" }, { "jb ",   "cmovae" },
        { nullptr, nullptr }
    };

    for (size_t i = 0; i + 3 < lines.size(); i++) {
        // Line i: cmp REG1, REG2
        if (!isInstr(lines[i])) continue;
        std::string a = trimLeft(lines[i]);
        if (a.substr(0, 4) != "cmp ") continue;
        size_t ac = a.find(',');
        if (ac == std::string::npos) continue;
        std::string reg1 = a.substr(4, ac - 4);
        while (!reg1.empty() && reg1.back() == ' ') reg1.pop_back();
        std::string reg2 = a.substr(ac + 1);
        while (!reg2.empty() && reg2.front() == ' ') reg2.erase(reg2.begin());
        while (!reg2.empty() && (reg2.back() == ' ' || reg2.back() == '\r')) reg2.pop_back();
        // Only handle register operands (no PTR, brackets, or immediates)
        bool r1ok = !reg1.empty(), r2ok = !reg2.empty();
        for (char c : reg1) if (c == ' ' || c == '[') { r1ok = false; break; }
        for (char c : reg2) if (c == ' ' || c == '[') { r2ok = false; break; }
        if (!r1ok || !r2ok) continue;

        // Line i+1: jCC .Lxxx
        size_t i1 = i + 1;
        while (i1 < lines.size() && !isInstr(lines[i1]) && trimLeft(lines[i1]).empty()) i1++;
        if (i1 >= lines.size() || !isInstr(lines[i1])) continue;
        std::string b = trimLeft(lines[i1]);
        const char* cmov_op = nullptr;
        std::string jmp_label;
        for (auto* e = cc_map; e->jcc; e++) {
            size_t jlen = strlen(e->jcc);
            if (b.size() > jlen && b.substr(0, jlen) == e->jcc) {
                cmov_op = e->cmov;
                jmp_label = b.substr(jlen);
                while (!jmp_label.empty() && jmp_label.back() == ' ') jmp_label.pop_back();
                break;
            }
        }
        if (!cmov_op) continue;
        // Target must be a local label (.Lxxx)
        if (jmp_label.size() < 2 || jmp_label[0] != '.') continue;

        // Line i+2: mov REG1, REG2  (exact same operands as cmp)
        size_t i2 = i1 + 1;
        while (i2 < lines.size() && !isInstr(lines[i2]) && trimLeft(lines[i2]).empty()) i2++;
        if (i2 >= lines.size() || !isInstr(lines[i2])) continue;
        std::string c2 = trimLeft(lines[i2]);
        std::string exp_mov = "mov " + reg1 + ", " + reg2;
        if (c2 != exp_mov) continue;

        // Line i+3: .Lxxx:  (the jump target label)
        size_t i3 = i2 + 1;
        while (i3 < lines.size() && trimLeft(lines[i3]).empty()) i3++;
        if (i3 >= lines.size()) continue;
        std::string d = trimLeft(lines[i3]);
        if (d != jmp_label + ":") continue;

        // Match! Replace jCC with cmovINV, delete the mov, keep cmp and label.
        char buf[80];
        snprintf(buf, sizeof(buf), "    %s %s, %s",
                 cmov_op, reg1.c_str(), reg2.c_str());
        lines[i1] = buf;
        lines[i2] = "";  // deleted by joinLines (empty string treated as blank line)
        changes++;
        i = i3; // advance past the fused pattern
    }
    return changes;
}

// ─── P14: Dead-mov and zero-multiply elimination (-O2) ───────────────────────
// Two sub-patterns:
//   (a) mov REG, IMM1 / mov REG, IMM2  → mov REG, IMM2  (first assignment dead)
//   (b) imul REG, 0  → xor REG32, REG32  (anything * 0 = 0)
static int applyP14(std::vector<std::string>& lines) {
    int changes = 0;
    // (a) Dead constant assignment: two consecutive movs to same register with immediates
    for (size_t i = 0; i + 1 < lines.size(); i++) {
        if (!isInstr(lines[i]) || !isInstr(lines[i+1])) continue;
        std::string a = trimLeft(lines[i]);
        std::string b = trimLeft(lines[i+1]);
        if (a.substr(0, 4) != "mov " || b.substr(0, 4) != "mov ") continue;
        // Extract dest register from both
        size_t ac = a.find(','), bc = b.find(',');
        if (ac == std::string::npos || bc == std::string::npos) continue;
        std::string dst_a = a.substr(4, ac - 4);
        std::string dst_b = b.substr(4, bc - 4);
        while (!dst_a.empty() && dst_a.back() == ' ') dst_a.pop_back();
        while (!dst_b.empty() && dst_b.back() == ' ') dst_b.pop_back();
        if (dst_a != dst_b) continue;
        // Both srcs must be immediates (plain integer, no PTR or brackets)
        std::string src_a = a.substr(ac + 1);
        std::string src_b = b.substr(bc + 1);
        while (!src_a.empty() && src_a.front() == ' ') src_a.erase(src_a.begin());
        while (!src_b.empty() && src_b.front() == ' ') src_b.erase(src_b.begin());
        if (src_a.find("PTR") != std::string::npos || src_b.find("PTR") != std::string::npos) continue;
        if (src_a.find('[') != std::string::npos || src_b.find('[') != std::string::npos) continue;
        char* ep_a = nullptr; char* ep_b = nullptr;
        strtoll(src_a.c_str(), &ep_a, 10);
        strtoll(src_b.c_str(), &ep_b, 10);
        if (!ep_a || *ep_a != '\0') continue; // src_a not plain integer
        if (!ep_b || *ep_b != '\0') continue; // src_b not plain integer
        // Delete first (dead) assignment
        lines[i] = "";
        changes++;
        i++; // skip the surviving mov
    }
    // (b) imul REG, 0 → xor REG32, REG32
    // Map 64-bit → 32-bit register for xor (zero-extends to 64-bit)
    static const struct { const char* r64; const char* r32; } regs[] = {
        { "rax", "eax" }, { "rcx", "ecx" }, { "rdx", "edx" }, { "rbx", "ebx" },
        { "r8",  "r8d" }, { "r9",  "r9d" }, { "r10", "r10d"}, { "r11", "r11d"},
        { nullptr, nullptr }
    };
    for (auto& line : lines) {
        if (!isInstr(line)) continue;
        std::string t = trimLeft(line);
        if (t.substr(0, 5) != "imul ") continue;
        size_t comma = t.find(',');
        if (comma == std::string::npos) continue;
        std::string reg = t.substr(5, comma - 5);
        while (!reg.empty() && reg.back() == ' ') reg.pop_back();
        std::string imm = t.substr(comma + 1);
        while (!imm.empty() && imm.front() == ' ') imm.erase(imm.begin());
        while (!imm.empty() && imm.back() == ' ') imm.pop_back();
        if (imm != "0") continue;
        // Find 32-bit equivalent
        for (auto* r = regs; r->r64; r++) {
            if (reg == r->r64) {
                char buf[32];
                snprintf(buf, sizeof(buf), "    xor %s, %s", r->r32, r->r32);
                line = buf;
                changes++;
                break;
            }
        }
    }
    return changes;
}

// ─── Main entry point ────────────────────────────────────────────────────────

int peepholeOptimize(std::string& asm_text, int opt_level) {
    std::vector<std::string> lines = splitLines(asm_text);
    int total = 0;

    // Iterate until no more changes (fixpoint, capped at 5 passes)
    for (int pass = 0; pass < 5; pass++) {
        int changed = 0;
        changed += applyP2(lines);   // zero-op elimination (safe, no context)
        changed += applyP5(lines);   // self-move elimination
        changed += applyP3(lines);   // imul→shl power-of-2
        changed += applyP9(lines);   // imul→lea strength reduction (non-pow2)
        changed += applyP7(lines);   // cmp reg,0 → test reg,reg (shorter)
        changed += applyP10(lines);  // mov reg,0 → xor reg32,reg32 (shorter)
        changed += applyP12(lines);  // add/sub reg,1 → inc/dec reg (shorter)
        changed += applyP6(lines);   // push/pop pair
        changed += applyP4(lines);   // movsxd via memory → direct register
        changed += applyP1(lines);   // redundant load-store (two-line window)
        if (opt_level >= 2) {
            changed += applyP13(lines); // constant folding (two-line window)
            changed += applyP14(lines); // dead-mov + imul-by-zero
            changed += applyP11(lines); // CMOV fusion (conditional branch + move)
        }
        if (changed == 0) break;
        total += changed;
    }

    asm_text = joinLines(lines);
    return total;
}
