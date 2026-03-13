#include "cpp.h"
#include "version.h"
#include <fstream>
#include <sstream>
#include <cctype>
#include <cstdlib>
#include <cstdio>
#include <cstring>

// C23 typed enum underlying size table (declared extern in cpp.h)
std::map<std::string, int> g_enum_underlying_sizes;

// 5.1: #pragma pack level — 0=default/natural, 1=packed (no padding)
int g_pack_level = 0;

Preprocessor::Preprocessor() : include_depth_(0), current_line_(1), in_block_comment_(false) {
    macros_["__STDC__"] = {"__STDC__", false, {}, false, "1", true};
    macros_["__STDC_VERSION__"] = {"__STDC_VERSION__", false, {}, false, "201710L", true};
    macros_["__STDC_HOSTED__"] = {"__STDC_HOSTED__", false, {}, false, "1", true};
    macros_["__NCC__"] = {"__NCC__", false, {}, false, "1", true};
    macros_["__RCC__"] = {"__RCC__", false, {}, false, "1", true};
    macros_["__RCC_VERSION__"] = {"__RCC_VERSION__", false, {}, false, RCC_STR(RCC_VERSION_NUM), true};
    macros_["NULL"] = {"NULL", false, {}, false, "((void*)0)", true};
    // C11 §6.10.9: If the implementation does not support _Atomic / <threads.h> /
    // complex arithmetic, it must define these macros so user code can provide fallbacks.
    macros_["__STDC_NO_ATOMICS__"] = {"__STDC_NO_ATOMICS__", false, {}, false, "0", true};
    // runtime/threads_win32.c provides Win32 implementations of C11 thrd_*/mtx_*/cnd_*/tss_*
    macros_["__STDC_NO_THREADS__"] = {"__STDC_NO_THREADS__", false, {}, false, "0", true};
    macros_["__STDC_NO_COMPLEX__"] = {"__STDC_NO_COMPLEX__", false, {}, false, "1", true};
    // GAP-A: C23 §6.10.9.2 — IEEE 754 / IEC 60559 conformance macros.
    // x86-64 MSVC uses IEEE 754 binary floats for float/double/long double.
    macros_["__STDC_IEC_60559_BFP__"] = {"__STDC_IEC_60559_BFP__", false, {}, false, "1", true};
    // C23 §6.10.9.3: source and execution character sets are UTF-8
    macros_["__STDC_UTF_8__"] = {"__STDC_UTF_8__", false, {}, false, "1", true};
    // C23 §6.10.4: #embed support macros — allow code like:
    //   #if __has_embed("f.bin") == __STDC_EMBED_FOUND__
    macros_["__STDC_EMBED_NOT_FOUND__"] = {"__STDC_EMBED_NOT_FOUND__", false, {}, false, "0", true};
    macros_["__STDC_EMBED_FOUND__"]     = {"__STDC_EMBED_FOUND__",     false, {}, false, "1", true};
    macros_["__STDC_EMBED_EMPTY__"]     = {"__STDC_EMBED_EMPTY__",     false, {}, false, "2", true};
    // GAP-2: C23 §6.2.5 — char8_t is defined as unsigned char (not plain char which may be signed)
    macros_["char8_t"] = {"char8_t", false, {}, false, "unsigned char", true};
    // N6: _Alignas(n) — C11/C23 alignment specifier.
    // Handled as a special case in expandMacros (like _Alignof/_Generic) so we
    // can emit a warning when alignment > 8 cannot be enforced. See expandMacros.
    // _Thread_local is also handled there.


    // C11/C23: _Static_assert is handled as a real compile-time check in codegen.
    // Do NOT define it as a preprocessor macro — leave it as an identifier so the
    // parser sees it as a CallExpr and codegen can evaluate the constant condition.

    // MSVC calling-convention keywords — expand to empty so RCC's parser ignores them.
    // These appear in Windows headers and library code (e.g. __declspec(dllexport) f __stdcall).
    macros_["__stdcall"]   = {"__stdcall",   false, {}, false, "", true};
    macros_["__cdecl"]     = {"__cdecl",     false, {}, false, "", true};
    macros_["__fastcall"]  = {"__fastcall",  false, {}, false, "", true};
    macros_["__thiscall"]  = {"__thiscall",  false, {}, false, "", true};
    macros_["__forceinline"] = {"__forceinline", false, {}, false, "", true};
    // __declspec(x) — MSVC attribute syntax; function-like, takes one argument, expands to empty.
    macros_["__declspec"]  = {"__declspec",  true,  {"x"}, false, "", true};
    // __volatile__ / __const__ / __signed__ — GCC extension spelling of qualifiers
    macros_["__volatile__"] = {"__volatile__", false, {}, false, "volatile", true};
    macros_["__const__"]    = {"__const__",    false, {}, false, "const",    true};
    macros_["__signed__"]   = {"__signed__",   false, {}, false, "signed",   true};

    // 5.1: GNU extension compatibility stubs — allow compilation of GCC-targeted code
    // __attribute__((x)) — GCC attribute syntax; no-op (RCC ignores all attributes)
    macros_["__attribute__"]  = {"__attribute__",  true, {"x"}, false, "", true};
    // __builtin_expect(x, y) — branch-prediction hint; expand to just the expression x
    macros_["__builtin_expect"] = {"__builtin_expect", true, {"x", "y"}, false, "x", true};
    // __builtin_unreachable() — marks unreachable code; expand to (void)0 (no-op in RCC)
    macros_["__builtin_unreachable"] = {"__builtin_unreachable", true, {}, false, "((void)0)", true};
    // __builtin_offsetof(type, member) — standard offsetof spelled as builtin
    macros_["__builtin_offsetof"] = {"__builtin_offsetof", true, {"type", "member"}, false, "offsetof(type, member)", true};
    // __builtin_popcount / popcountl / popcountll — map to C23 stdc_count_ones
    macros_["__builtin_popcount"]   = {"__builtin_popcount",   true, {"x"}, false, "stdc_count_ones(x)", true};
    macros_["__builtin_popcountl"]  = {"__builtin_popcountl",  true, {"x"}, false, "stdc_count_ones(x)", true};
    macros_["__builtin_popcountll"] = {"__builtin_popcountll", true, {"x"}, false, "stdc_count_ones(x)", true};
    // __extension__ — GCC pedantry suppressor; no-op
    macros_["__extension__"] = {"__extension__", false, {}, false, "", true};
    // __asm__ / __asm — GCC inline assembly keyword; no-op (RCC does not support inline asm)
    macros_["__asm__"]  = {"__asm__",  true, {"x"}, false, "", true};
    macros_["__asm"]    = {"__asm",    true, {"x"}, false, "", true};
    // __restrict__ — GCC spelling of restrict
    macros_["__restrict__"] = {"__restrict__", false, {}, false, "restrict", true};
    macros_["__restrict"]   = {"__restrict",   false, {}, false, "restrict", true};
    // __inline__ — GCC spelling of inline
    macros_["__inline"]  = {"__inline",  false, {}, false, "inline", true};
    macros_["__inline__"] = {"__inline__", false, {}, false, "inline", true};

    // Windows x64 target platform macros.
    // Required for real-world Windows libraries (e.g. sqlite3.c uses _WIN32 to select
    // Win32 vs POSIX code paths; without this it falls through to pthreads/Unix VFS).
    macros_["_WIN32"]     = {"_WIN32",     false, {}, false, "1", true};
    macros_["_WIN64"]     = {"_WIN64",     false, {}, false, "1", true};
    macros_["__x86_64__"] = {"__x86_64__", false, {}, false, "1", true};
    macros_["_M_AMD64"]   = {"_M_AMD64",   false, {}, false, "1", true};
    macros_["_M_X64"]     = {"_M_X64",     false, {}, false, "1", true};
    // __int64 — MSVC built-in 64-bit integer type; map to long long
    macros_["__int64"]    = {"__int64",    false, {}, false, "long long", true};
    // __int32, __int16, __int8 — MSVC built-in integer types
    macros_["__int32"]    = {"__int32",    false, {}, false, "int",       true};
    macros_["__int16"]    = {"__int16",    false, {}, false, "short",     true};
    macros_["__int8"]     = {"__int8",     false, {}, false, "char",      true};
}

void Preprocessor::addIncludePath(const std::string& path) { user_include_paths_.push_back(path); }
void Preprocessor::addSystemIncludePath(const std::string& path) { sys_include_paths_.push_back(path); }
void Preprocessor::defineMacro(const std::string& name, const std::string& value) {
    macros_[name] = {name, false, {}, false, value, false};
}
void Preprocessor::undefineMacro(const std::string& name) { macros_.erase(name); }

bool Preprocessor::isActive() {
    for (auto& cs : cond_stack_) if (!cs.active) return false;
    return true;
}

std::string Preprocessor::trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r");
    if (a == std::string::npos) return "";
    return s.substr(a, s.find_last_not_of(" \t\r\n") - a + 1);
}

bool Preprocessor::isIdentStart(char c) { return std::isalpha((unsigned char)c) || c == '_'; }
bool Preprocessor::isIdentChar(char c) { return std::isalnum((unsigned char)c) || c == '_'; }

// C11 §6.7.2.1 anonymous struct/union support:
// The LALR parser requires every struct member to have a declarator.
// Anonymous struct/union members (no declarator) are post-processed here:
// each "struct/union { ... };" without a following declarator gets a synthetic
// member name "__rcc_anon_N" injected before the semicolon.  The codegen then
// detects the "__rcc_anon_" prefix and promotes the inner fields (G9).
static std::string injectAnonymousStructNames(const std::string& src) {
    std::string out;
    out.reserve(src.size() + 128);
    struct AnonEntry { int depth; bool is_anon; };
    std::vector<AnonEntry> stack;
    int brace_depth = 0;
    int anon_count  = 0;
    bool in_str  = false;
    bool in_char = false;
    bool in_blk  = false;

    // Lookahead: skip whitespace and comments starting at j
    auto skip_ws = [&](size_t j) -> size_t {
        while (j < src.size()) {
            char sc = src[j];
            if (sc==' '||sc=='\t'||sc=='\n'||sc=='\r') { ++j; continue; }
            if (j+1<src.size() && sc=='/'&&src[j+1]=='*') {
                j += 2;
                while (j+1<src.size() && !(src[j]=='*'&&src[j+1]=='/')) ++j;
                if (j+1<src.size()) j += 2;
                continue;
            }
            if (j+1<src.size() && sc=='/'&&src[j+1]=='/') {
                while (j<src.size()&&src[j]!='\n') ++j; continue;
            }
            break;
        }
        return j;
    };

    size_t i = 0, n = src.size();
    while (i < n) {
        char c = src[i];

        // Block comment — track state, emit verbatim
        if (!in_str && !in_char && !in_blk && i+1<n && c=='/'&&src[i+1]=='*') {
            in_blk = true; out+=c; out+=src[i+1]; i+=2;
            while (i<n) {
                if (i+1<n && src[i]=='*'&&src[i+1]=='/') {
                    out+=src[i]; out+=src[i+1]; i+=2; in_blk=false; break;
                }
                out+=src[i++];
            }
            continue;
        }
        if (in_blk) { out+=src[i++]; continue; }

        // Line comment — emit verbatim
        if (!in_str && !in_char && i+1<n && c=='/'&&src[i+1]=='/') {
            while (i<n && src[i]!='\n') out+=src[i++];
            if (i<n) out+=src[i++];
            continue;
        }

        // String literal
        if (!in_char && c=='"') { in_str=!in_str; out+=c; i++; continue; }
        if (in_str) {
            if (c=='\\'&&i+1<n) { out+=c; out+=src[i+1]; i+=2; continue; }
            out+=c; i++; continue;
        }

        // Char literal
        if (c=='\'') { in_char=!in_char; out+=c; i++; continue; }
        if (in_char) {
            if (c=='\\'&&i+1<n) { out+=c; out+=src[i+1]; i+=2; continue; }
            out+=c; i++; continue;
        }

        // Open brace
        if (c=='{') { brace_depth++; out+=c; i++; continue; }

        // Close brace — check for anonymous struct member injection
        if (c=='}') {
            bool inject = false;
            if (!stack.empty() && stack.back().depth==brace_depth) {
                if (stack.back().is_anon) {
                    size_t j = skip_ws(i+1);
                    if (j<n && src[j]==';') inject = true;
                }
                stack.pop_back();
            }
            brace_depth--;
            out+=c; i++;
            if (inject) {
                char nm[32];
                snprintf(nm, sizeof(nm), " __rcc_anon_%d", anon_count++);
                out += nm;
            }
            continue;
        }

        // 'struct' / 'union' keyword detection (word-boundary check)
        if ((c=='s'||c=='u') &&
                (i==0 || (!std::isalnum((unsigned char)src[i-1])&&src[i-1]!='_'))) {
            bool is_struct = (i+6<=n && src.compare(i,6,"struct")==0 &&
                    (i+6>=n || (!std::isalnum((unsigned char)src[i+6])&&src[i+6]!='_')));
            bool is_union = !is_struct && (i+5<=n && src.compare(i,5,"union")==0 &&
                    (i+5>=n || (!std::isalnum((unsigned char)src[i+5])&&src[i+5]!='_')));
            if (is_struct || is_union) {
                size_t kw = is_struct ? 6u : 5u;
                out += src.substr(i, kw);
                i += kw;
                size_t j = skip_ws(i);
                if (j<n && src[j]=='{') {
                    // Anonymous: no tag → will need injection
                    stack.push_back({brace_depth+1, true});
                } else if (j<n && (std::isalpha((unsigned char)src[j])||src[j]=='_')) {
                    // Named: skip tag identifier, then check for '{'
                    size_t k = j;
                    while (k<n && (std::isalnum((unsigned char)src[k])||src[k]=='_')) ++k;
                    k = skip_ws(k);
                    if (k<n && src[k]=='{')
                        stack.push_back({brace_depth+1, false});
                }
                continue;
            }
        }

        out+=c; i++;
    }
    return out;
}

std::string Preprocessor::preprocess(const char* source, const char* filename) {
    current_file_ = filename ? filename : "<stdin>";
    current_line_ = 1;
    // Anonymous struct members are now handled by the LALR grammar (G-A):
    // struct_decl -> spec_qualifier_list SEMICOLON
    // No preprocessor injection needed.
    return processSource(source, current_file_.c_str());
}

std::string Preprocessor::processSource(const char* src, const char* filename) {
    std::string output;
    std::string prev_file = current_file_;
    int prev_line = current_line_;
    bool prev_in_comment = in_block_comment_;
    current_file_ = filename;
    current_line_ = 1;
    in_block_comment_ = false;  // Reset comment state for each file

    // #pragma once: if this file was already included with #pragma once, skip it
    if (!current_file_.empty() && pragma_once_files_.count(current_file_)) {
        current_file_ = prev_file;
        current_line_ = prev_line;
        in_block_comment_ = prev_in_comment;
        return "";
    }

    std::istringstream stream(src);
    std::string line;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        while (!line.empty() && line.back() == '\\' && stream) {
            line.pop_back();
            std::string next;
            if (std::getline(stream, next)) {
                if (!next.empty() && next.back() == '\r') next.pop_back();
                line += next;
                current_line_++;
            }
        }
        // Multi-line macro call support: if the line has unbalanced open parens
        // (outside string literals and line comments) and is not a directive,
        // join subsequent physical lines until the parens are balanced.
        // This handles patterns like: ASSERT(cond ? f()\n             : g())
        int extra_lines = 0;
        if (!line.empty() && line[0] != '#') {
            int paren_net = 0;
            bool in_s = false; char s_ch = 0;
            bool in_bc = in_block_comment_;  // track /* */ block comments starting from current state
            for (size_t ci = 0; ci < line.size(); ci++) {
                char cc = line[ci];
                if (in_bc) {
                    if (cc == '*' && ci + 1 < line.size() && line[ci+1] == '/') { in_bc = false; ci++; }
                } else if (in_s) {
                    if (cc == '\\' && ci + 1 < line.size()) { ci++; continue; }
                    if (cc == s_ch) in_s = false;
                } else if (cc == '"' || cc == '\'') { in_s = true; s_ch = cc; }
                else if (cc == '/' && ci + 1 < line.size() && line[ci+1] == '/') break;
                else if (cc == '/' && ci + 1 < line.size() && line[ci+1] == '*') { in_bc = true; ci++; }
                else if (cc == '(') paren_net++;
                else if (cc == ')') paren_net--;
            }
            // Mini conditional stack: track #ifdef/#ifndef/#else/#endif in multi-line expressions
            std::vector<bool> jstack;
            auto jactive = [&]() -> bool {
                for (bool b : jstack) if (!b) return false;
                return true;
            };
            while (paren_net > 0 && stream.good()) {
                std::string cont_line;
                if (!std::getline(stream, cont_line)) break;
                if (!cont_line.empty() && cont_line.back() == '\r') cont_line.pop_back();
                current_line_++; extra_lines++;
                // Handle preprocessor directives embedded in multi-line expressions
                { size_t di = 0;
                  while (di < cont_line.size() && (cont_line[di]==' '||cont_line[di]=='\t')) di++;
                  if (di < cont_line.size() && cont_line[di] == '#') {
                      // Parse directive name and argument
                      std::string drest = cont_line.substr(di + 1);
                      while (!drest.empty() && (drest[0]==' '||drest[0]=='\t')) drest = drest.substr(1);
                      size_t dn = 0;
                      while (dn < drest.size() && (isalpha((unsigned char)drest[dn]) || drest[dn]=='_')) dn++;
                      std::string dname = drest.substr(0, dn);
                      std::string darg  = drest.substr(dn);
                      while (!darg.empty() && (darg[0]==' '||darg[0]=='\t')) darg = darg.substr(1);
                      if (dname == "ifdef" || dname == "ifndef") {
                          size_t mi = 0;
                          while (mi < darg.size() && (isalnum((unsigned char)darg[mi]) || darg[mi]=='_')) mi++;
                          std::string mname = darg.substr(0, mi);
                          bool is_def = macros_.count(mname) > 0;
                          jstack.push_back((dname == "ifdef") ? is_def : !is_def);
                      } else if (dname == "if") {
                          // Simplified eval: handle 0, 1, defined(X), !X, X
                          bool cond = true;
                          if (darg == "0") cond = false;
                          else if (darg == "1") cond = true;
                          else if (darg.rfind("defined(", 0) == 0) {
                              size_t ep = darg.find(')', 8);
                              if (ep != std::string::npos) cond = macros_.count(darg.substr(8, ep-8)) > 0;
                          } else if (!darg.empty() && darg[0] == '!') {
                              std::string m = darg.substr(1);
                              while (!m.empty() && (m[0]==' '||m[0]=='\t')) m = m.substr(1);
                              size_t mi = 0; while (mi < m.size() && (isalnum((unsigned char)m[mi])||m[mi]=='_')) mi++;
                              cond = !macros_.count(m.substr(0, mi));
                          }
                          jstack.push_back(cond);
                      } else if (dname == "else") {
                          if (!jstack.empty()) jstack.back() = !jstack.back();
                      } else if (dname == "elif") {
                          if (!jstack.empty()) jstack.back() = false; // simplified: deactivate
                      } else if (dname == "endif") {
                          if (!jstack.empty()) jstack.pop_back();
                      }
                      continue; // Never add directive lines to mega-line
                  }
                }
                if (!jactive()) continue; // Skip content from inactive conditional blocks
                while (!cont_line.empty() && cont_line.back() == '\\' && stream.good()) {
                    cont_line.pop_back();
                    std::string cont2;
                    if (std::getline(stream, cont2)) {
                        if (!cont2.empty() && cont2.back() == '\r') cont2.pop_back();
                        cont_line += cont2;
                        current_line_++; extra_lines++;
                    }
                }
                line += " " + cont_line;
                bool in_s2 = false; char s_ch2 = 0;
                for (size_t ci = 0; ci < cont_line.size(); ci++) {
                    char cc = cont_line[ci];
                    if (in_bc) {
                        if (cc == '*' && ci + 1 < cont_line.size() && cont_line[ci+1] == '/') { in_bc = false; ci++; }
                    } else if (in_s2) {
                        if (cc == '\\' && ci + 1 < cont_line.size()) { ci++; continue; }
                        if (cc == s_ch2) in_s2 = false;
                    } else if (cc == '"' || cc == '\'') { in_s2 = true; s_ch2 = cc; }
                    else if (cc == '/' && ci + 1 < cont_line.size() && cont_line[ci+1] == '/') break;
                    else if (cc == '/' && ci + 1 < cont_line.size() && cont_line[ci+1] == '*') { in_bc = true; ci++; }
                    else if (cc == '(') paren_net++;
                    else if (cc == ')') paren_net--;
                }
            }
        }
        // Emit blank lines for consumed continuation lines to preserve line counts
        for (int el = 0; el < extra_lines; el++) output += "\n";
        processLine(line, output);
        current_line_++;
    }
    current_file_ = prev_file;
    current_line_ = prev_line;
    in_block_comment_ = prev_in_comment;  // Restore comment state
    return output;
}

void Preprocessor::processLine(const std::string& line, std::string& output) {
    // Strip block and line comments, but not /* or // inside string/char literals.
    std::string processed_line;
    size_t i = 0;
    bool in_string = false;   // inside "..."
    bool in_char   = false;   // inside '...'

    while (i < line.size()) {
        if (in_block_comment_) {
            // We're inside a block comment, look for */
            while (i < line.size() && !(line[i] == '*' && i + 1 < line.size() && line[i + 1] == '/')) {
                i++;
            }
            if (i < line.size()) {
                // Found */
                in_block_comment_ = false;
                i += 2;  // Skip */
            }
            // If we didn't find */, the rest of the line is comment
        } else if (in_string) {
            // Inside a string literal — copy verbatim, handle \ escapes and closing "
            char c = line[i];
            processed_line += c;
            if (c == '\\') {
                // Escape sequence: copy next char too (don't let \" close the string)
                i++;
                if (i < line.size()) { processed_line += line[i]; i++; }
            } else if (c == '"') {
                in_string = false;
                i++;
            } else {
                i++;
            }
        } else if (in_char) {
            // Inside a char literal — copy verbatim, handle \ escapes and closing '
            char c = line[i];
            processed_line += c;
            if (c == '\\') {
                i++;
                if (i < line.size()) { processed_line += line[i]; i++; }
            } else if (c == '\'') {
                in_char = false;
                i++;
            } else {
                i++;
            }
        } else {
            // Normal code — watch for string/char starts and comment starts
            char c = line[i];
            if (c == '"') {
                // Start of string literal
                in_string = true;
                processed_line += c;
                i++;
            } else if (c == '\'') {
                // Start of char literal
                in_char = true;
                processed_line += c;
                i++;
            } else if (c == '/' && i + 1 < line.size() && line[i + 1] == '*') {
                // Start of block comment
                in_block_comment_ = true;
                i += 2;  // Skip /*
                // Look for */ on same line
                while (i < line.size() && !(line[i] == '*' && i + 1 < line.size() && line[i + 1] == '/')) {
                    i++;
                }
                if (i < line.size()) {
                    // Found */ on same line
                    in_block_comment_ = false;
                    i += 2;  // Skip */
                    processed_line += ' ';  // Replace comment with space
                }
                // If not found, comment continues to next line
            } else if (c == '/' && i + 1 < line.size() && line[i + 1] == '/') {
                // Start of line comment — discard rest of line
                break;
            } else {
                // Regular character, copy it
                processed_line += c;
                i++;
            }
        }
    }

    // Now process the line without block comments
    std::string trimmed = trim(processed_line);
    if (trimmed.empty()) { output += '\n'; return; }
    if (trimmed[0] == '#') {
        processDirective(trimmed, output);
        output += '\n';
        return;
    }
    if (!isActive()) { output += '\n'; return; }
    // Track simple variable type declarations for _Generic dispatch improvement.
    // P4.1: Multi-word types (long long, unsigned long, etc.) checked FIRST to prevent
    //       single-word "long" from overwriting "long long x" with type "long".
    // Also fixed: "TYPE VAR = val" (space before '=') is now recognized as a declaration.
    {
        // Pass 1: two-word primitive types (higher priority — checked before single-word)
        static const char* multi_stypes[] = {
            "long long", "unsigned long", "long double",
            "unsigned int", "signed char", nullptr
        };
        // Pass 2: single-word types — skip if already recorded from pass 1
        static const char* single_stypes[] = {
            "double", "float", "long", "unsigned", "short", "int", "char", nullptr
        };

        auto scanTypes = [&](const char** stypes, bool skip_if_known) {
            for (const char** st = stypes; *st; ++st) {
                std::string stype(*st);
                size_t pos = 0;
                while (pos < processed_line.size()) {
                    size_t found = processed_line.find(stype, pos);
                    if (found == std::string::npos) break;
                    bool lb = (found == 0 || !isIdentChar(processed_line[found - 1]));
                    size_t end_type = found + stype.size();
                    bool rb = (end_type < processed_line.size() &&
                               processed_line[end_type] == ' ');
                    if (lb && rb) {
                        size_t np = end_type;
                        while (np < processed_line.size() && processed_line[np] == ' ') np++;
                        size_t ns = np;
                        while (np < processed_line.size() && isIdentChar(processed_line[np])) np++;
                        if (np > ns) {
                            std::string varname = processed_line.substr(ns, np - ns);
                            if (skip_if_known && var_types_.count(varname)) {
                                pos = found + 1; continue; // already recorded with higher-priority type
                            }
                            // Recognize: "TYPE VAR;" | "TYPE VAR=" | "TYPE VAR[" |
                            //            "TYPE VAR," | "TYPE VAR)" | "TYPE VAR = ..."
                            bool decl_follows = (np >= processed_line.size() ||
                                processed_line[np] == ';' || processed_line[np] == '=' ||
                                processed_line[np] == '[' || processed_line[np] == ',' ||
                                processed_line[np] == ')');
                            if (!decl_follows && np < processed_line.size() &&
                                processed_line[np] == ' ') {
                                // Check for "VAR = " pattern (space(s) then '=')
                                size_t peek = np;
                                while (peek < processed_line.size() &&
                                       processed_line[peek] == ' ') peek++;
                                if (peek < processed_line.size() &&
                                    processed_line[peek] == '=') decl_follows = true;
                            }
                            if (decl_follows) {
                                var_types_[varname] = stype;
                                // C99/C23: optional 3.7 — track array declarations so
                                // typeof(arr) correctly returns a pointer type (decay).
                                if (np < processed_line.size() && processed_line[np] == '[')
                                    var_array_names_.insert(varname);
                            }
                        }
                    }
                    pos = found + 1;
                }
            }
        };
        scanTypes(multi_stypes, false);   // pass 1: two-word types (unconditional)
        scanTypes(single_stypes, true);   // pass 2: single-word types (skip if known)
    }
    std::set<std::string> expanding;
    output += expandMacros(processed_line, expanding);
    output += '\n';
}

void Preprocessor::processDirective(const std::string& line, std::string& output) {
    size_t pos = 1;
    while (pos < line.size() && (line[pos] == ' ' || line[pos] == '\t')) pos++;
    if (pos >= line.size()) return;
    size_t start = pos;
    while (pos < line.size() && isIdentChar(line[pos])) pos++;
    std::string dir = line.substr(start, pos - start);
    while (pos < line.size() && (line[pos] == ' ' || line[pos] == '\t')) pos++;
    std::string rest = pos < line.size() ? line.substr(pos) : "";

    if (dir == "ifdef") { handleIfdef(rest, false); return; }
    if (dir == "ifndef") { handleIfdef(rest, true); return; }
    if (dir == "if") { handleIf(rest); return; }
    if (dir == "elif") { handleElif(rest); return; }
    // C23: #elifdef and #elifndef
    if (dir == "elifdef") { handleElifdef(rest, false); return; }
    if (dir == "elifndef") { handleElifdef(rest, true); return; }
    if (dir == "else") { handleElse(); return; }
    if (dir == "endif") { handleEndif(); return; }
    if (!isActive()) return;
    if (dir == "define") { handleDefine(rest); return; }
    if (dir == "undef") { handleUndef(rest); return; }
    if (dir == "include") { handleInclude(rest, output); return; }
    if (dir == "embed") { handleEmbed(rest, output); return; }  // W23: C23 #embed
    if (dir == "error") { handleError(rest); return; }
    // C23: #warning directive
    if (dir == "warning") { handleWarning(rest); return; }
    if (dir == "line") { handleLine(rest); return; }
    if (dir == "pragma") {
        if (trim(rest) == "once") pragma_once_files_.insert(current_file_);
        // 5.1: #pragma pack — set global struct alignment
        // Forms: pack(1), pack(), pack(0), pack(push,1), pack(pop)
        else if (rest.size() >= 4 && rest.substr(0, 4) == "pack") {
            std::string args = trim(rest.substr(4));
            // Strip outer parens: "pack(1)" → "1"
            if (!args.empty() && args.front() == '(') {
                size_t rp = args.rfind(')');
                if (rp != std::string::npos)
                    args = trim(args.substr(1, rp - 1));
            }
            if (args.empty() || args == "0") {
                g_pack_level = 0;  // restore default
            } else if (args == "pop") {
                g_pack_level = 0;
            } else {
                // Extract number from "1", "push, 1", "push,1"
                size_t comma = args.find(',');
                std::string numstr = (comma != std::string::npos) ? trim(args.substr(comma + 1)) : args;
                if (!numstr.empty() && isdigit((unsigned char)numstr[0]))
                    g_pack_level = std::stoi(numstr);
            }
        }
        return;  // Other pragmas silently ignored
    }
    // unknown directive: ignore
}

void Preprocessor::handleDefine(const std::string& rest) {
    if (rest.empty()) return;
    size_t pos = 0;
    while (pos < rest.size() && isIdentChar(rest[pos])) pos++;
    std::string name = rest.substr(0, pos);
    if (name.empty()) return;
    PPMacro m;
    m.name = name; m.is_function = false; m.is_variadic = false; m.is_predefined = false;
    if (pos < rest.size() && rest[pos] == '(') {
        m.is_function = true; pos++;
        while (pos < rest.size() && rest[pos] != ')') {
            while (pos < rest.size() && (rest[pos] == ' ' || rest[pos] == ',')) pos++;
            if (pos < rest.size() && rest[pos] == ')') break;
            if (rest.substr(pos, 3) == "...") { m.is_variadic = true; pos += 3; }
            else {
                size_t ps = pos;
                while (pos < rest.size() && isIdentChar(rest[pos])) pos++;
                if (pos > ps) m.params.push_back(rest.substr(ps, pos - ps));
            }
        }
        if (pos < rest.size()) pos++; // skip )
    }
    while (pos < rest.size() && (rest[pos] == ' ' || rest[pos] == '\t')) pos++;
    m.body = pos < rest.size() ? rest.substr(pos) : "";
    macros_[name] = m;
}

void Preprocessor::handleUndef(const std::string& rest) { macros_.erase(trim(rest)); }

void Preprocessor::handleInclude(const std::string& rest, std::string& output) {
    std::string t = trim(rest);
    bool is_sys = false; std::string fn;
    if (!t.empty() && t[0] == '<') { is_sys = true; auto e = t.find('>'); if (e != std::string::npos) fn = t.substr(1, e-1); }
    else if (!t.empty() && t[0] == '"') { auto e = t.find('"', 1); if (e != std::string::npos) fn = t.substr(1, e-1); }
    if (fn.empty()) return;
    std::string path = findIncludeFile(fn, is_sys);
    if (path.empty()) { fprintf(stderr, "%s:%d: warning: cannot find '%s'\n", current_file_.c_str(), current_line_, fn.c_str()); return; }
    if (include_depth_ >= MAX_INCLUDE_DEPTH) { fprintf(stderr, "%s:%d: error: include depth exceeded\n", current_file_.c_str(), current_line_); return; }
    std::string content = readFile(path);
    if (!content.empty()) { include_depth_++; output += processSource(content.c_str(), path.c_str()); include_depth_--; }
}

void Preprocessor::handleIfdef(const std::string& rest, bool negate) {
    std::string name = trim(rest);
    bool def = macros_.count(name) > 0;
    bool active = negate ? !def : def;
    if (!isActive()) active = false;
    cond_stack_.push_back({active, active, false});
}

void Preprocessor::handleIf(const std::string& rest) {
    bool active = false;
    if (isActive()) { std::set<std::string> ex; active = evalExpr(expandMacros(rest, ex)) != 0; }
    cond_stack_.push_back({active, active, false});
}

void Preprocessor::handleElif(const std::string& rest) {
    if (cond_stack_.empty()) return;
    auto& top = cond_stack_.back();
    if (top.in_else || top.has_been_true) { top.active = false; return; }
    bool parent = true;
    for (size_t i = 0; i + 1 < cond_stack_.size(); i++) if (!cond_stack_[i].active) { parent = false; break; }
    if (parent) { std::set<std::string> ex; bool r = evalExpr(expandMacros(rest, ex)) != 0; top.active = r; if (r) top.has_been_true = true; }
}

void Preprocessor::handleElifdef(const std::string& rest, bool negate) {
    if (cond_stack_.empty()) return;
    auto& top = cond_stack_.back();
    if (top.in_else || top.has_been_true) { top.active = false; return; }
    bool parent = true;
    for (size_t i = 0; i + 1 < cond_stack_.size(); i++) if (!cond_stack_[i].active) { parent = false; break; }
    if (parent) {
        std::string name = trim(rest);
        bool def = macros_.count(name) > 0;
        bool active = negate ? !def : def;
        top.active = active;
        if (active) top.has_been_true = true;
    }
}

void Preprocessor::handleElse() {
    if (cond_stack_.empty()) return;
    auto& top = cond_stack_.back();
    top.in_else = true;
    bool parent = true;
    for (size_t i = 0; i + 1 < cond_stack_.size(); i++) if (!cond_stack_[i].active) { parent = false; break; }
    top.active = parent && !top.has_been_true;
}

void Preprocessor::handleEndif() { if (!cond_stack_.empty()) cond_stack_.pop_back(); }
void Preprocessor::handleError(const std::string& rest) { fprintf(stderr, "%s:%d: #error %s\n", current_file_.c_str(), current_line_, rest.c_str()); exit(1); }
void Preprocessor::handleWarning(const std::string& rest) { fprintf(stderr, "%s:%d: warning: %s\n", current_file_.c_str(), current_line_, rest.c_str()); }
void Preprocessor::handleLine(const std::string& rest) { current_line_ = std::atoi(rest.c_str()) - 1; }

std::string Preprocessor::expandMacros(const std::string& text, std::set<std::string>& expanding) {
    std::string result;
    size_t i = 0;
    while (i < text.size()) {
        if (text[i] == '"') { result += text[i++]; while (i < text.size() && text[i] != '"') { if (text[i] == '\\' && i+1 < text.size()) result += text[i++]; result += text[i++]; } if (i < text.size()) result += text[i++]; continue; }
        if (text[i] == '\'') { result += text[i++]; while (i < text.size() && text[i] != '\'') { if (text[i] == '\\' && i+1 < text.size()) result += text[i++]; result += text[i++]; } if (i < text.size()) result += text[i++]; continue; }
        if (text[i] == '/' && i+1 < text.size() && text[i+1] == '/') break;
        if (text[i] == '/' && i+1 < text.size() && text[i+1] == '*') { i += 2; while (i+1 < text.size() && !(text[i]=='*' && text[i+1]=='/')) i++; if (i+1 < text.size()) i += 2; result += ' '; continue; }
        if (isIdentStart(text[i])) {
            size_t s = i;
            while (i < text.size() && isIdentChar(text[i])) i++;
            std::string id = text.substr(s, i - s);
            // C11/C23: _Generic(controlling-expr, type1: expr1, ..., default: exprN)
            // The parser cannot handle "type: expr" associations as function arguments,
            // so we must expand _Generic here in the preprocessor.
            // Without type-checking capability at this level, we use the 'default:' branch.
            // If there is no default, we use the first branch expression.
            if (id == "_Generic") {
                while (i < text.size() && text[i] == ' ') i++;
                if (i < text.size() && text[i] == '(') {
                    // Collect everything inside the outer parens
                    size_t start_paren = i++;
                    int depth = 1;
                    std::string inner;
                    while (i < text.size() && depth > 0) {
                        if (text[i] == '(') depth++;
                        else if (text[i] == ')') { depth--; if (depth == 0) break; }
                        inner += text[i++];
                    }
                    if (i < text.size()) i++;  // skip closing ')'
                    // Split inner text into comma-separated fields (respecting parens)
                    std::vector<std::string> fields;
                    std::string cur;
                    int d2 = 0;
                    for (char ch : inner) {
                        if (ch == '(' || ch == '{') d2++;
                        else if (ch == ')' || ch == '}') d2--;
                        if (ch == ',' && d2 == 0) { fields.push_back(cur); cur.clear(); }
                        else cur += ch;
                    }
                    if (!cur.empty()) fields.push_back(cur);
                    // fields[0] = controlling expression — heuristic type inference
                    // fields[1..] = "type: expr" or "default: expr" associations
                    //
                    // G8 improvement: inspect the controlling expression to determine
                    // a likely C type category, then try to match a type association
                    // before falling back to "default:".
                    std::string ctrl = fields.empty() ? std::string() : fields[0];
                    while (!ctrl.empty() && (ctrl.front()==' '||ctrl.front()=='\t')) ctrl.erase(ctrl.begin());
                    while (!ctrl.empty() && (ctrl.back() ==' '||ctrl.back() =='\t')) ctrl.pop_back();

                    // Determine heuristic type of controlling expression
                    // Categories: "float", "double", "long double", "long long",
                    //             "unsigned long long", "long", "unsigned long",
                    //             "unsigned", "int", "char", "char*", "pointer"
                    std::string ctrl_type;
                    if (!ctrl.empty()) {
                        // R3: Cast expression: (type)expr — extract type from cast
                        // Pattern: ctrl starts with '(' and has a matching ')'
                        if (ctrl.front() == '(') {
                            size_t rp = ctrl.find(')');
                            if (rp != std::string::npos && rp > 1) {
                                std::string cast_t = ctrl.substr(1, rp - 1);
                                while (!cast_t.empty() && (cast_t.front()==' '||cast_t.front()=='\t')) cast_t.erase(cast_t.begin());
                                while (!cast_t.empty() && (cast_t.back() ==' '||cast_t.back() =='\t')) cast_t.pop_back();
                                // Map common C types
                                if (cast_t=="int"||cast_t=="signed int"||cast_t=="signed") ctrl_type="int";
                                else if (cast_t=="unsigned"||cast_t=="unsigned int") ctrl_type="unsigned";
                                else if (cast_t=="long") ctrl_type="long";
                                else if (cast_t=="unsigned long") ctrl_type="unsigned long";
                                else if (cast_t=="long long") ctrl_type="long long";
                                else if (cast_t=="unsigned long long") ctrl_type="unsigned long long";
                                else if (cast_t=="float") ctrl_type="float";
                                else if (cast_t=="double") ctrl_type="double";
                                else if (cast_t=="long double") ctrl_type="long double";
                                else if (cast_t=="char") ctrl_type="char";
                                else if (cast_t=="short"||cast_t=="unsigned short") ctrl_type=cast_t;
                                else if (!cast_t.empty() && cast_t.back()=='*') ctrl_type="pointer";
                                else ctrl_type=cast_t; // use as-is for typedef names
                            }
                        }
                        // R3: NULL → pointer type (before it gets expanded)
                        if (ctrl_type.empty() &&
                            (ctrl=="NULL"||ctrl=="((void*)0)"||ctrl=="(void*)0")) {
                            ctrl_type = "pointer";
                        }
                        // R3: String literal → char* type
                        if (ctrl_type.empty() && !ctrl.empty() && ctrl.front()=='"') {
                            ctrl_type = "char*";
                        }
                        // Literal/expression heuristic — only if cast/NULL/string did not match
                        if (ctrl_type.empty()) {
                            // Float literal: contains '.', 'e'/'E', or ends with 'f'/'F'
                            bool has_dot = ctrl.find('.') != std::string::npos;
                            bool has_exp = ctrl.find('e') != std::string::npos || ctrl.find('E') != std::string::npos;
                            bool ends_f  = !ctrl.empty() && (ctrl.back() == 'f' || ctrl.back() == 'F');
                            bool ends_l  = !ctrl.empty() && (ctrl.back() == 'l' || ctrl.back() == 'L');
                            bool ends_u  = !ctrl.empty() && (ctrl.back() == 'u' || ctrl.back() == 'U');
                            // Strip common suffixes for analysis
                            std::string base = ctrl;
                            while (!base.empty() && (base.back()=='l'||base.back()=='L'||base.back()=='u'||base.back()=='U'||base.back()=='f'||base.back()=='F')) base.pop_back();
                            std::string sfx = ctrl.substr(base.size());
                            // Convert suffix to lowercase for comparison
                            std::string sfx_lc = sfx;
                            for (char& c : sfx_lc) c = (char)std::tolower((unsigned char)c);

                            if (has_dot || has_exp || ends_f) {
                                if (ends_f) ctrl_type = "float";
                                else if (ends_l && !ends_f) ctrl_type = "long double";
                                else ctrl_type = "double";
                            } else if (ctrl.find('*') != std::string::npos || ctrl.front() == '&') {
                                ctrl_type = "pointer";
                            } else {
                                // Integer literal or expression
                                if (sfx_lc == "ll" || sfx_lc == "ull" || sfx_lc == "llu") {
                                    ctrl_type = (sfx_lc[0]=='u') ? "unsigned long long" : "long long";
                                } else if (sfx_lc == "l" || sfx_lc == "ul" || sfx_lc == "lu") {
                                    ctrl_type = (sfx_lc[0]=='u') ? "unsigned long" : "long";
                                } else if (sfx_lc == "u") {
                                    ctrl_type = "unsigned";
                                } else {
                                    ctrl_type = "int"; // default for unadorned integer expressions
                                }
                            }
                        }
                    }

                    // Variable name lookup: if ctrl is a plain identifier, check
                    // var_types_ map for the declared type (overrides the "int" default
                    // that the heuristic assigns to unrecognized expressions).
                    if (!ctrl.empty() && isIdentStart((unsigned char)ctrl[0])) {
                        bool all_id = true;
                        for (unsigned char cc : ctrl)
                            if (!isIdentChar(cc)) { all_id = false; break; }
                        if (all_id) {
                            auto vit = var_types_.find(ctrl);
                            if (vit != var_types_.end()) ctrl_type = vit->second;
                        }
                    }

                    // Collect all type associations and the default branch
                    std::string chosen;
                    std::string first_branch;
                    std::string default_branch;
                    // Also record type->val map for type-matching
                    std::vector<std::pair<std::string,std::string>> assocs; // (key, val)
                    for (size_t fi = 1; fi < fields.size(); fi++) {
                        // Find the ':' separator (skip '::' to avoid C++ compat issues)
                        size_t col = std::string::npos;
                        for (size_t ci = 0; ci < fields[fi].size(); ci++) {
                            if (fields[fi][ci] == ':' &&
                                (ci+1 >= fields[fi].size() || fields[fi][ci+1] != ':')) {
                                col = ci; break;
                            }
                        }
                        if (col == std::string::npos) continue;
                        std::string key = fields[fi].substr(0, col);
                        std::string val = fields[fi].substr(col + 1);
                        // Trim whitespace
                        while (!key.empty() && (key.front() == ' ' || key.front() == '\t')) key.erase(key.begin());
                        while (!key.empty() && (key.back()  == ' ' || key.back()  == '\t')) key.pop_back();
                        while (!val.empty() && (val.front() == ' ' || val.front() == '\t')) val.erase(val.begin());
                        while (!val.empty() && (val.back()  == ' ' || val.back()  == '\t')) val.pop_back();
                        if (first_branch.empty()) first_branch = val;
                        if (key == "default") { default_branch = val; continue; }
                        assocs.push_back({key, val});
                    }

                    // Try to match ctrl_type against type associations
                    if (!ctrl_type.empty()) {
                        for (auto& [k, v] : assocs) {
                            if (k == ctrl_type) { chosen = v; break; }
                        }
                        // Also try common aliases
                        if (chosen.empty() && ctrl_type == "int") {
                            for (auto& [k, v] : assocs) {
                                if (k == "signed" || k == "signed int") { chosen = v; break; }
                            }
                        }
                        if (chosen.empty() && ctrl_type == "double") {
                            // 'double' expressions may match 'long double' in tgmath patterns
                        }
                        // char* and pointer are interchangeable for string literal dispatch
                        if (chosen.empty() && (ctrl_type == "char*" || ctrl_type == "pointer")) {
                            for (auto& [k, v] : assocs) {
                                if (k == "char*" || k == "const char*" || k == "pointer") { chosen = v; break; }
                            }
                        }
                    }
                    // Fall back to default:, then first branch
                    if (chosen.empty()) chosen = default_branch;
                    if (chosen.empty()) chosen = first_branch;
                    result += expandMacros(chosen, expanding);
                    (void)start_paren;
                } else {
                    result += id;
                }
                continue;
            }
            // C23 §6.7.2.2: typed enum — 'enum NAME : underlying_type { ... }'
            // The parser handles 'enum NAME { }' but not 'enum NAME : TYPE { }'.
            // Strip the ': TYPE' here so existing grammar parses it correctly.
            // The underlying type annotation is discarded (defaults to int).
            if (id == "enum") {
                result += id;
                // Save position before whitespace after 'enum'
                size_t saved_before_ws = i;
                while (i < text.size() && (text[i] == ' ' || text[i] == '\t')) i++;
                // Optional tag name
                if (i < text.size() && isIdentStart(text[i])) {
                    size_t ts = i;
                    while (i < text.size() && isIdentChar(text[i])) i++;
                    std::string tag = text.substr(ts, i - ts);
                    // Save position right after tag (before any whitespace)
                    size_t saved_after_tag = i;
                    while (i < text.size() && (text[i] == ' ' || text[i] == '\t')) i++;
                    // If followed by ':' (but not '::'), this is a typed enum
                    if (i < text.size() && text[i] == ':' &&
                        (i+1 >= text.size() || text[i+1] != ':')) {
                        i++; // skip ':'
                        // Collect underlying type tokens until '{' or ';' for sizeof table
                        std::string utype;
                        while (i < text.size() && text[i] != '{' && text[i] != ';') {
                            if (text[i] == '\n') { result += '\n'; }
                            else utype += text[i];
                            i++;
                        }
                        // Trim whitespace
                        while (!utype.empty() && (utype.front()==' '||utype.front()=='\t')) utype.erase(utype.begin());
                        while (!utype.empty() && (utype.back()==' '||utype.back()=='\t')) utype.pop_back();
                        // Determine byte size of underlying type for sizeof/alignof
                        int usize = 4; // default: int
                        if (utype=="char"||utype=="signed char"||utype=="unsigned char"||
                            utype=="int8_t"||utype=="uint8_t") usize = 1;
                        else if (utype=="short"||utype=="signed short"||utype=="unsigned short"||
                                 utype=="short int"||utype=="int16_t"||utype=="uint16_t") usize = 2;
                        else if (utype=="int"||utype=="signed int"||utype=="unsigned int"||
                                 utype=="int32_t"||utype=="uint32_t") usize = 4;
                        else if (utype=="long long"||utype=="signed long long"||
                                 utype=="unsigned long long"||utype=="long long int"||
                                 utype=="int64_t"||utype=="uint64_t") usize = 8;
                        // long (without long long) is 4 on Windows x64 ABI
                        else if (utype=="long"||utype=="signed long"||utype=="unsigned long"||
                                 utype=="long int"||utype=="unsigned long int") usize = 4;
                        g_enum_underlying_sizes[tag] = usize;
                        // Emit ' tag'; '{' or ';' will be processed on next iteration
                        result += ' ';
                        result += tag;
                    } else {
                        // Not a typed enum: restore position to right after tag
                        // so the outer loop emits the original whitespace.
                        i = saved_after_tag;
                        result += ' ';
                        result += tag;
                    }
                } else {
                    // No tag (anonymous enum): restore whitespace position
                    i = saved_before_ws;
                }
                continue;
            }
            // C11/C23: _Alignof(type) / alignof(type) — compile-time alignment constant
            // Handled at preprocessor level so 'int' etc. type-keywords are never parsed as expressions.
            if (id == "_Alignof" || id == "alignof") {
                while (i < text.size() && text[i] == ' ') i++;
                if (i < text.size() && text[i] == '(') {
                    i++; // skip '('
                    std::string tname;
                    int depth = 1;
                    while (i < text.size() && depth > 0) {
                        if (text[i] == '(') { depth++; tname += text[i++]; }
                        else if (text[i] == ')') { depth--; if (depth > 0) tname += text[i]; i++; }
                        else tname += text[i++];
                    }
                    // Trim whitespace
                    while (!tname.empty() && (tname.front() == ' ' || tname.front() == '\t')) tname.erase(tname.begin());
                    while (!tname.empty() && (tname.back()  == ' ' || tname.back()  == '\t')) tname.pop_back();
                    // Alignment values for x64 Windows (MSVC ABI)
                    long long align = 8; // default: pointer/double
                    if (tname == "char" || tname == "signed char" || tname == "unsigned char")
                        align = 1;
                    else if (tname == "short" || tname == "short int" || tname == "unsigned short")
                        align = 2;
                    else if (tname == "int" || tname == "unsigned int" || tname == "unsigned" ||
                             tname == "long" || tname == "unsigned long" || tname == "float")
                        align = 4;
                    else if (tname == "long long" || tname == "unsigned long long" ||
                             tname == "double" || tname == "long double")
                        align = 8;
                    result += std::to_string(align);
                } else {
                    result += id;
                }
                continue;
            }
            // C11: _Noreturn — function attribute keyword (C11 §6.7.4).
            // RCC does not enforce non-return analysis; silently consume the keyword.
            // Declared functions will compile normally; runtime behavior is unchanged.
            // NOTE: _Alignas, _Thread_local, _Atomic are now real grammar tokens —
            //       handled by the LALR parser (G-A/G-B/G-C/G-D). NOT preprocessed.
            if (id == "_Noreturn") {
                // C11 §6.7.4: _Noreturn → [[noreturn]] so the return-path
                // warning added in task 7.44 fires for the keyword form too.
                // P4-D: warn if used under -std=c89 or -std=c99
                if (g_std_level < 11) {
                    fprintf(stderr, "warning: '_Noreturn' requires -std=c11 or later"
                            " (compiling as C%d)\n", g_std_level == 89 ? 89 : 99);
                }
                // C23 §6.7.4: _Noreturn is deprecated — compilers should warn (optional 3.5)
                if (g_std_level >= 23) {
                    fprintf(stderr, "warning: '_Noreturn' is deprecated in C23; "
                            "use [[noreturn]] attribute instead\n");
                }
                result += "[[noreturn]]";
                continue;
            }
            // C23 §6.2.6.3: _BitInt(N) — N-bit integer type.
            // RCC maps to the smallest standard integer type that holds N bits.
            // For N > 64 a warning is emitted and long long is used.
            // P4-D: warn if _BitInt is used before C23
            if (id == "_BitInt" || id == "_Sat") {
                if (g_std_level < 23) {
                    fprintf(stderr, "warning: '_BitInt' is a C23 extension"
                            " (compiling as -std=c%d; use -std=c23)\n", g_std_level);
                }
                while (i < text.size() && text[i] == ' ') i++;
                if (i < text.size() && text[i] == '(') {
                    i++; // skip '('
                    while (i < text.size() && text[i] == ' ') i++;
                    long long N = 0;
                    while (i < text.size() && std::isdigit((unsigned char)text[i]))
                        N = N * 10 + (text[i++] - '0');
                    while (i < text.size() && text[i] == ' ') i++;
                    if (i < text.size() && text[i] == ')') i++;
                    if (N <= 8)       result += "signed char";
                    else if (N <= 16) result += "short";
                    else if (N <= 32) result += "int";
                    else if (N <= 64) result += "long long";
                    else {
                        fprintf(stderr, "warning: _BitInt(%lld) > 64 not supported by RCC; "
                                        "using long long\n", (long long)N);
                        result += "long long";
                    }
                } else {
                    result += id;
                }
                continue;
            }
            // C23 §6.7.2.5: typeof(EXPR) / typeof_unqual(EXPR) in declaration context.
            // When the argument is a known variable name or a literal, expand to the type
            // string so the parser sees a normal type-specifier declaration.
            // Unknown arguments (function calls, complex expressions) fall back to
            // sizeof(ARG) so expression-context uses still work.
            if (id == "typeof" || id == "typeof_unqual") {
                while (i < text.size() && text[i] == ' ') i++;
                if (i < text.size() && text[i] == '(') {
                    i++; // skip '('
                    int depth = 1;
                    size_t arg_start = i;
                    while (i < text.size() && depth > 0) {
                        if (text[i] == '(') depth++;
                        else if (text[i] == ')') { if (--depth == 0) break; }
                        i++;
                    }
                    std::string arg = text.substr(arg_start, i - arg_start);
                    if (i < text.size()) i++; // skip closing ')'
                    // Trim whitespace from arg
                    while (!arg.empty() && arg.front() == ' ') arg.erase(arg.begin());
                    while (!arg.empty() && arg.back()  == ' ') arg.pop_back();
                    // Attempt to resolve the type of the argument
                    std::string type_str;
                    // GAP-E (pre-strip): for typeof_unqual, strip leading qualifiers from arg
                    // BEFORE any case matching so "const int" → "int", "volatile double" → "double".
                    // For plain typeof, also check qualifier-prefixed types (pass through with quals).
                    {
                        static const char* quals_pre[] = {
                            "const ", "volatile ", "restrict ", "_Atomic ", nullptr
                        };
                        // Build unqualified version of arg
                        std::string unqual = arg;
                        bool more = true;
                        while (more) { more = false;
                            for (const char** q = quals_pre; *q; ++q) {
                                size_t qlen = strlen(*q);
                                if (unqual.size() >= qlen && unqual.compare(0, qlen, *q) == 0) {
                                    unqual = unqual.substr(qlen); more = true; break;
                                }
                            }
                        }
                        if (id == "typeof_unqual" && unqual != arg)
                            arg = unqual; // strip qualifiers — typeof_unqual exposes base type
                        // For typeof with qualifiers: we'll detect this below via unqual mismatch
                        // and pass arg through as-is (qualifiers preserved) if base is known.
                    }
                    // GAP-4 Case 0a: arg is itself a C type keyword → pass through directly
                    {
                        static const char* type_kws[] = {
                            "int", "char", "short", "long", "double", "float", "void",
                            "unsigned", "signed", "long long", "unsigned int", "signed int",
                            "unsigned long", "unsigned long long", "long double",
                            "signed char", "unsigned char", "unsigned short", "signed short",
                            nullptr
                        };
                        for (const char** tk = type_kws; *tk; ++tk) {
                            if (arg == *tk) { type_str = arg; break; }
                        }
                    }
                    // GAP-4 Case 0b: pointer type (ends with '*') → pass through as-is
                    if (type_str.empty() && !arg.empty() && arg.back() == '*')
                        type_str = arg;
                    // GAP-4 Case 0c: struct/union/enum type → pass through (grammar handles these)
                    if (type_str.empty() && arg.size() > 5 &&
                        (arg.substr(0, 7) == "struct " || arg.substr(0, 6) == "union " ||
                         arg.substr(0, 5) == "enum "))
                        type_str = arg;
                    // Case 1: plain identifier — look in var_types_ map
                    if (!arg.empty() && (isalpha((unsigned char)arg[0]) || arg[0] == '_')) {
                        bool plain = true;
                        for (unsigned char cc : arg) if (!isIdentChar(cc)) { plain = false; break; }
                        if (plain) {
                            auto vit = var_types_.find(arg);
                            if (vit != var_types_.end()) {
                                type_str = vit->second;
                                // C99/C23 optional 3.7: array/VLA variables decay to pointer in expressions.
                                // typeof(arr) where arr is T arr[] returns T*, not T.
                                if (var_array_names_.count(arg) &&
                                    !type_str.empty() && type_str.back() != '*')
                                    type_str += " *";
                            }
                        }
                    }
                    // Case 2: numeric literal — infer from suffix / decimal point
                    if (type_str.empty() && !arg.empty() &&
                        (isdigit((unsigned char)arg[0]) || arg[0] == '.')) {
                        char suf = arg.back();
                        if (suf == 'f' || suf == 'F') type_str = "float";
                        else if (suf == 'l' || suf == 'L') type_str = "long double";
                        else if (arg.find('.') != std::string::npos ||
                                 arg.find('e') != std::string::npos ||
                                 arg.find('E') != std::string::npos)
                            type_str = "double";
                        else type_str = "int";
                    }
                    if (!type_str.empty()) {
                        result += type_str;  // expanded: typeof(x) → "double"
                    } else {
                        // Cannot resolve — emit sizeof(arg) so expression context works
                        result += "sizeof(";
                        result += arg;
                        result += ")";
                    }
                    continue;
                }
                result += id; continue;
            }
            if (id == "defined") {
                while (i < text.size() && text[i] == ' ') i++;
                bool hp = (i < text.size() && text[i] == '('); if (hp) i++;
                while (i < text.size() && text[i] == ' ') i++;
                size_t ns = i; while (i < text.size() && isIdentChar(text[i])) i++;
                std::string mn = text.substr(ns, i - ns);
                if (hp) { while (i < text.size() && text[i] == ' ') i++; if (i < text.size() && text[i] == ')') i++; }
                result += macros_.count(mn) ? "1" : "0"; continue;
            }
            // C11 §6.10.9.1: standard predefined macros (dynamic values)
            if (id == "__LINE__") { result += std::to_string(current_line_); continue; }
            if (id == "__FILE__") {
                // Escape backslashes in file path (Windows uses backslash separators)
                std::string esc;
                for (char ch : current_file_) {
                    if (ch == '\\') esc += "\\\\";
                    else esc += ch;
                }
                result += "\"" + esc + "\"";
                continue;
            }
            if (id == "__func__" || id == "__FUNCTION__") { result += id; continue; }
            auto it = macros_.find(id);
            if (it != macros_.end() && !expanding.count(id)) {
                if (it->second.is_function) {
                    size_t sv = i; while (i < text.size() && text[i] == ' ') i++;
                    if (i < text.size() && text[i] == '(') {
                        auto args = collectMacroArgs(text, i, (int)it->second.params.size());
                        expanding.insert(id);
                        result += expandFunctionMacro(it->second, args, expanding);
                        expanding.erase(id);
                    } else { i = sv; result += id; }
                } else {
                    expanding.insert(id);
                    result += expandObjectMacro(it->second, expanding);
                    expanding.erase(id);
                }
            } else result += id;
            continue;
        }
        result += text[i++];
    }
    return result;
}

std::string Preprocessor::expandObjectMacro(const PPMacro& m, std::set<std::string>& ex) { return expandMacros(m.body, ex); }

std::string Preprocessor::expandFunctionMacro(const PPMacro& m, const std::vector<std::string>& args, std::set<std::string>& ex) {
    // For variadic macros, collect __VA_ARGS__ from extra arguments
    std::string va_args_value;
    if (m.is_variadic && args.size() > m.params.size()) {
        for (size_t i = m.params.size(); i < args.size(); i++) {
            if (i > m.params.size()) va_args_value += ", ";
            va_args_value += args[i];
        }
    }

    std::string result;
    size_t i = 0;
    while (i < m.body.size()) {
        // Handle # (stringification)
        if (m.body[i] == '#' && i + 1 < m.body.size() && m.body[i + 1] != '#') {
            i++; // skip #
            while (i < m.body.size() && (m.body[i] == ' ' || m.body[i] == '\t')) i++;
            if (i < m.body.size() && isIdentStart(m.body[i])) {
                size_t s = i; while (i < m.body.size() && isIdentChar(m.body[i])) i++;
                std::string p = m.body.substr(s, i - s);
                bool found = false;
                // Check for __VA_ARGS__ stringification
                if (p == "__VA_ARGS__" && m.is_variadic) {
                    result += stringify(va_args_value);
                    found = true;
                } else {
                    for (size_t j = 0; j < m.params.size(); j++) {
                        if (m.params[j] == p && j < args.size()) { result += stringify(args[j]); found = true; break; }
                    }
                }
                if (!found) result += "#" + p;
            }
            continue;
        }
        if (isIdentStart(m.body[i])) {
            size_t s = i; while (i < m.body.size() && isIdentChar(m.body[i])) i++;
            std::string p = m.body.substr(s, i - s);
            // Check for ## (token pasting) following this token
            size_t save = i;
            while (save < m.body.size() && (m.body[save] == ' ' || m.body[save] == '\t')) save++;
            if (save + 1 < m.body.size() && m.body[save] == '#' && m.body[save + 1] == '#') {
                // Token paste: get left operand
                std::string left;
                bool found_left = false;
                for (size_t j = 0; j < m.params.size(); j++) {
                    if (m.params[j] == p && j < args.size()) { left = args[j]; found_left = true; break; }
                }
                if (!found_left) left = p;
                i = save + 2; // skip ##
                while (i < m.body.size() && (m.body[i] == ' ' || m.body[i] == '\t')) i++;
                // Get right operand
                std::string right;
                if (i < m.body.size() && isIdentStart(m.body[i])) {
                    size_t rs = i; while (i < m.body.size() && isIdentChar(m.body[i])) i++;
                    std::string rp = m.body.substr(rs, i - rs);
                    bool found_right = false;
                    for (size_t j = 0; j < m.params.size(); j++) {
                        if (m.params[j] == rp && j < args.size()) { right = args[j]; found_right = true; break; }
                    }
                    if (!found_right) right = rp;
                } else if (i < m.body.size() && std::isdigit((unsigned char)m.body[i])) {
                    size_t rs = i; while (i < m.body.size() && std::isdigit((unsigned char)m.body[i])) i++;
                    right = m.body.substr(rs, i - rs);
                }
                result += pasteTokens(left, right);
            } else {
                bool found = false;
                // Check for __VA_ARGS__ replacement
                if (p == "__VA_ARGS__" && m.is_variadic) {
                    result += va_args_value;
                    found = true;
                // C23: __VA_OPT__(tokens) — include tokens only if __VA_ARGS__ is non-empty
                } else if (p == "__VA_OPT__" && m.is_variadic) {
                    while (i < m.body.size() && (m.body[i] == ' ' || m.body[i] == '\t')) i++;
                    if (i < m.body.size() && m.body[i] == '(') {
                        i++; // skip opening '('
                        int depth = 1;
                        std::string opt_content;
                        while (i < m.body.size() && depth > 0) {
                            if (m.body[i] == '(') { depth++; opt_content += m.body[i++]; }
                            else if (m.body[i] == ')') { depth--; if (depth > 0) opt_content += m.body[i]; i++; }
                            else opt_content += m.body[i++];
                        }
                        if (!va_args_value.empty()) result += opt_content;
                    }
                    found = true;
                } else {
                    for (size_t j = 0; j < m.params.size(); j++) if (m.params[j] == p && j < args.size()) { result += args[j]; found = true; break; }
                }
                if (!found) result += p;
            }
        } else {
            // Check for ## preceded by non-ident token
            if (m.body[i] == '#' && i + 1 < m.body.size() && m.body[i + 1] == '#') {
                // Remove trailing whitespace from result
                while (!result.empty() && (result.back() == ' ' || result.back() == '\t')) result.pop_back();
                i += 2; // skip ##
                while (i < m.body.size() && (m.body[i] == ' ' || m.body[i] == '\t')) i++;
                // Next token gets pasted
            } else {
                result += m.body[i++];
            }
        }
    }
    return expandMacros(result, ex);
}

std::vector<std::string> Preprocessor::collectMacroArgs(const std::string& text, size_t& pos, int) {
    std::vector<std::string> args; pos++; int depth = 1; std::string cur;
    bool in_string = false; char str_delim = 0;
    while (pos < text.size() && depth > 0) {
        char c = text[pos];
        if (in_string) {
            cur += c;
            if (c == '\\' && pos + 1 < text.size()) { pos++; cur += text[pos]; }
            else if (c == str_delim) { in_string = false; }
        } else if (c == '"' || c == '\'') {
            in_string = true; str_delim = c; cur += c;
        } else if (c == '(') { depth++; cur += c; }
        else if (c == ')') { depth--; if (depth > 0) cur += c; }
        else if (c == ',' && depth == 1) { args.push_back(trim(cur)); cur.clear(); }
        else cur += c;
        pos++;
    }
    if (!cur.empty() || !args.empty()) args.push_back(trim(cur));
    return args;
}

std::string Preprocessor::stringify(const std::string& s) {
    // C11 §6.10.3.2: escape \ and " in stringified macro argument
    std::string out = "\"";
    for (char c : s) {
        if (c == '\\') out += "\\\\";
        else if (c == '"') out += "\\\"";
        else out += c;
    }
    out += "\"";
    return out;
}
std::string Preprocessor::pasteTokens(const std::string& l, const std::string& r) { return l + r; }

void Preprocessor::skipSpaces(const char*& p) { while (*p == ' ' || *p == '\t') p++; }

long long Preprocessor::evalExpr(const std::string& expr) { const char* p = expr.c_str(); return evalExprImpl(p, 0); }

int Preprocessor::getPrecedence(const char* p, std::string& op) {
    skipSpaces(p);
    if (p[0]=='|'&&p[1]=='|') { op="||"; return 1; }
    if (p[0]=='&'&&p[1]=='&') { op="&&"; return 2; }
    if (p[0]=='|'&&p[1]!='|') { op="|"; return 3; }
    if (p[0]=='^') { op="^"; return 4; }
    if (p[0]=='&'&&p[1]!='&') { op="&"; return 5; }
    if (p[0]=='='&&p[1]=='=') { op="=="; return 6; }
    if (p[0]=='!'&&p[1]=='=') { op="!="; return 6; }
    if (p[0]=='<'&&p[1]=='=') { op="<="; return 7; }
    if (p[0]=='>'&&p[1]=='=') { op=">="; return 7; }
    if (p[0]=='<'&&p[1]!='<') { op="<"; return 7; }
    if (p[0]=='>'&&p[1]!='>') { op=">"; return 7; }
    if (p[0]=='<'&&p[1]=='<') { op="<<"; return 8; }
    if (p[0]=='>'&&p[1]=='>') { op=">>"; return 8; }
    if (p[0]=='+') { op="+"; return 9; }
    if (p[0]=='-') { op="-"; return 9; }
    if (p[0]=='*') { op="*"; return 10; }
    if (p[0]=='/') { op="/"; return 10; }
    if (p[0]=='%') { op="%"; return 10; }
    return -1;
}

long long Preprocessor::evalExprImpl(const char*& p, int min_prec) {
    long long left = evalPrimary(p);
    while (true) {
        skipSpaces(p); std::string op; int prec = getPrecedence(p, op);
        if (prec < 0 || prec < min_prec) break;
        p += op.size();
        long long right = evalExprImpl(p, prec + 1);
        if (op=="||") left=left||right; else if (op=="&&") left=left&&right;
        else if (op=="|") left=left|right; else if (op=="^") left=left^right;
        else if (op=="&") left=left&right; else if (op=="==") left=left==right;
        else if (op=="!=") left=left!=right; else if (op=="<") left=left<right;
        else if (op==">") left=left>right; else if (op=="<=") left=left<=right;
        else if (op==">=") left=left>=right; else if (op=="<<") left=left<<right;
        else if (op==">>") left=left>>right; else if (op=="+") left=left+right;
        else if (op=="-") left=left-right; else if (op=="*") left=left*right;
        else if (op=="/") left=right?left/right:0; else if (op=="%") left=right?left%right:0;
    }
    return left;
}

long long Preprocessor::evalPrimary(const char*& p) {
    skipSpaces(p);
    if (*p=='(') { p++; long long v = evalExprImpl(p,0); skipSpaces(p); if (*p==')') p++; return v; }
    if (*p=='!') { p++; return !evalPrimary(p); }
    if (*p=='~') { p++; return ~evalPrimary(p); }
    if (*p=='-') { p++; return -evalPrimary(p); }
    if (*p=='+') { p++; return evalPrimary(p); }
    if (std::isdigit((unsigned char)*p)) {
        long long v = 0;
        if (p[0]=='0' && (p[1]=='x'||p[1]=='X')) { p+=2; while (std::isxdigit((unsigned char)*p)) { v=v*16+(std::isdigit((unsigned char)*p)?*p-'0':std::tolower((unsigned char)*p)-'a'+10); p++; } }
        else if (p[0]=='0' && p[1]>='0' && p[1]<='7') { while (*p>='0' && *p<='7') { v=v*8+(*p-'0'); p++; } }
        else { while (std::isdigit((unsigned char)*p)) { v=v*10+(*p-'0'); p++; } }
        while (*p=='L'||*p=='l'||*p=='U'||*p=='u') p++;
        return v;
    }
    if (*p=='\'') { p++; long long v=*p++; if (v=='\\') { if (*p=='n'){v='\n';p++;} else if (*p=='t'){v='\t';p++;} else if (*p=='0'){v=0;p++;} else v=*p++; } if (*p=='\'') p++; return v; }
    if (isIdentStart(*p)) {
        const char* ident_start = p;
        while (isIdentChar(*p)) p++;
        size_t ident_len = p - ident_start;
        // C23 §6.10.9: __has_c_attribute(attr-name) → attribute-date constant, or 0
        if (ident_len == 17 && memcmp(ident_start, "__has_c_attribute", 17) == 0) {
            skipSpaces(p);
            if (*p == '(') {
                p++; skipSpaces(p);
                const char* attr_start = p;
                while (*p && (isIdentChar(*p) || *p == ':')) p++;
                std::string attr(attr_start, p - attr_start);
                skipSpaces(p);
                if (*p == ')') p++;
                // Return C23 standard attribute introduction date (yyyymm format)
                if (attr == "deprecated")   return 201904LL;
                if (attr == "fallthrough")  return 201910LL;
                if (attr == "nodiscard")    return 202003LL;
                if (attr == "noreturn")     return 202011LL;
                if (attr == "maybe_unused") return 202106LL;
                if (attr == "unsequenced")  return 202207LL;
                if (attr == "reproducible") return 202207LL;
                return 0LL;
            }
            return 0LL;
        }
        // C23 §6.10.10: __has_embed(<file>) / __has_embed("file")
        // Returns 1 if the file can be embedded with #embed, 0 otherwise.
        if (ident_len == 11 && memcmp(ident_start, "__has_embed", 11) == 0) {
            skipSpaces(p);
            if (*p == '(') {
                p++; skipSpaces(p);
                bool is_sys = (*p == '<');
                if (*p == '<' || *p == '"') p++;
                const char* name_start = p;
                char end_ch = is_sys ? '>' : '"';
                while (*p && *p != end_ch && *p != ')' && *p != '\n') p++;
                std::string fname(name_start, p - name_start);
                if (*p == end_ch) p++;
                skipSpaces(p);
                if (*p == ')') p++;
                // Return __STDC_EMBED_FOUND__ (1) if file exists, __STDC_EMBED_NOT_FOUND__ (0)
                return findIncludeFile(fname, is_sys).empty() ? 0LL : 1LL;
            }
            return 0;
        }
        // W24: C23 __has_include(<file>) / __has_include("file")
        if (ident_len == 13 && memcmp(ident_start, "__has_include", 13) == 0) {
            skipSpaces(p);
            if (*p == '(') {
                p++; skipSpaces(p);
                bool is_sys = (*p == '<');
                if (*p == '<' || *p == '"') p++;
                const char* name_start = p;
                char end_ch = is_sys ? '>' : '"';
                while (*p && *p != end_ch && *p != ')' && *p != '\n') p++;
                std::string fname(name_start, p - name_start);
                if (*p == end_ch) p++;
                skipSpaces(p);
                if (*p == ')') p++;
                return findIncludeFile(fname, is_sys).empty() ? 0LL : 1LL;
            }
            return 0;
        }
        // defined(MACRO) / defined MACRO
        if (ident_len == 7 && memcmp(ident_start, "defined", 7) == 0) {
            skipSpaces(p);
            bool has_paren = (*p == '(');
            if (has_paren) { p++; skipSpaces(p); }
            const char* m_start = p;
            while (isIdentChar(*p)) p++;
            std::string mname(m_start, p - m_start);
            if (has_paren) { skipSpaces(p); if (*p == ')') p++; }
            return macros_.count(mname) ? 1LL : 0LL;
        }
        return 0;
    }
    return 0;
}

std::string Preprocessor::findIncludeFile(const std::string& name, bool is_system) {
    if (!is_system) {
        std::string dir = current_file_;
        size_t sl = dir.find_last_of("/\\");
        dir = (sl != std::string::npos) ? dir.substr(0, sl + 1) : "";
        std::string path = dir + name;
        std::ifstream f(path); if (f.good()) return path;
        for (auto& p : user_include_paths_) { path = p + "/" + name; std::ifstream tf(path); if (tf.good()) return path; }
    }
    for (auto& p : sys_include_paths_) { std::string path = p + "/" + name; std::ifstream tf(path); if (tf.good()) return path; }
    // Also search user include paths for system includes (matches GCC/clang -I behavior)
    if (is_system) {
        for (auto& p : user_include_paths_) { std::string path = p + "/" + name; std::ifstream tf(path); if (tf.good()) return path; }
    }
    return "";
}

std::string Preprocessor::readFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return "";
    return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

// W23: C23 #embed — expand file contents as comma-separated integer literals.
// GAP-B: C23 §6.10.6.1 — also parse embed-parameters: limit(), prefix(), suffix(), if_empty()
void Preprocessor::handleEmbed(const std::string& rest, std::string& output) {
    if (rest.empty()) return;
    size_t pos = 0;
    while (pos < rest.size() && (rest[pos] == ' ' || rest[pos] == '\t')) pos++;
    if (pos >= rest.size()) return;

    bool is_system = (rest[pos] == '<');
    if (rest[pos] == '<' || rest[pos] == '"') pos++;
    size_t name_start = pos;
    char end_ch = is_system ? '>' : '"';
    while (pos < rest.size() && rest[pos] != end_ch) pos++;
    std::string filename = rest.substr(name_start, pos - name_start);
    if (pos < rest.size()) pos++; // skip closing delimiter

    // Parse embed-parameters: limit(N) prefix(...) suffix(...) if_empty(...)
    long long limit_val = -1;           // -1 = no limit
    std::string prefix_tokens;          // prepended if file non-empty
    std::string suffix_tokens;          // appended  if file non-empty
    std::string if_empty_tokens;        // used if file is empty / not found

    auto skipSpaces = [&]() {
        while (pos < rest.size() && (rest[pos]==' '||rest[pos]=='\t')) pos++;
    };
    auto collectParenContent = [&]() -> std::string {
        // Collect balanced parenthesised content (past the opening '(')
        int depth = 1;
        size_t start2 = pos;
        while (pos < rest.size() && depth > 0) {
            if (rest[pos] == '(') depth++;
            else if (rest[pos] == ')') { if (--depth == 0) break; }
            pos++;
        }
        std::string content = rest.substr(start2, pos - start2);
        if (pos < rest.size()) pos++; // skip closing ')'
        return content;
    };

    while (pos < rest.size()) {
        skipSpaces();
        if (pos >= rest.size()) break;
        // Read parameter name
        size_t kw_start = pos;
        while (pos < rest.size() && (isalnum((unsigned char)rest[pos]) || rest[pos]=='_')) pos++;
        std::string kw = rest.substr(kw_start, pos - kw_start);
        skipSpaces();
        if (pos >= rest.size() || rest[pos] != '(') break; // not a valid parameter
        pos++; // skip '('

        if (kw == "limit") {
            std::string arg = collectParenContent();
            // Simple integer expression evaluation (decimal only)
            long long n = 0;
            for (char c2 : arg) if (c2 >= '0' && c2 <= '9') n = n*10 + (c2-'0');
            limit_val = n;
        } else if (kw == "prefix") {
            prefix_tokens = collectParenContent();
        } else if (kw == "suffix") {
            suffix_tokens = collectParenContent();
        } else if (kw == "if_empty") {
            if_empty_tokens = collectParenContent();
        } else {
            collectParenContent(); // unknown parameter — skip
        }
        skipSpaces();
    }

    std::string path = findIncludeFile(filename, is_system);
    if (path.empty()) {
        if (!if_empty_tokens.empty()) { output += if_empty_tokens + '\n'; return; }
        fprintf(stderr, "%s:%d: error: #embed: file not found: %s\n",
                current_file_.c_str(), current_line_, filename.c_str());
        return;
    }

    std::string bytes = readFile(path);

    // Apply limit
    if (limit_val >= 0 && (long long)bytes.size() > limit_val)
        bytes.resize((size_t)limit_val);

    if (bytes.empty()) {
        if (!if_empty_tokens.empty()) { output += if_empty_tokens + '\n'; }
        return;
    }

    // Emit prefix, comma-separated bytes, suffix
    std::string expanded;
    if (!prefix_tokens.empty()) expanded += prefix_tokens + ',';
    for (size_t i = 0; i < bytes.size(); i++) {
        if (i > 0) expanded += ',';
        expanded += std::to_string((unsigned char)bytes[i]);
    }
    if (!suffix_tokens.empty()) expanded += ',' + suffix_tokens;
    output += expanded + '\n';
}
