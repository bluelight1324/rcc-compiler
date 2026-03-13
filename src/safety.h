#pragma once
/**
 * RCC Version 4.0 - Memory Safety Analysis
 *
 * Core data structures for Rust-style memory safety without syntax changes.
 * Provides automatic ownership inference, lifetime analysis, and borrow checking.
 */

#include "ast.h"
#include "codegen.h"
#include <string>
#include <vector>
#include <map>
#include <set>

// ═══════════════════════════════════════════════════════════════════════════════
// SAFETY LEVELS
// ═══════════════════════════════════════════════════════════════════════════════

enum class SafetyLevel {
    None,       // No guards, static analysis only (0% overhead)
    Minimal,    // Null checks only (1-3% overhead)
    Medium,     // Borrow tracking (5-15% overhead)
    Full        // Full instrumentation (30-100% overhead)
};

// ═══════════════════════════════════════════════════════════════════════════════
// POINTER CLASSIFICATION
// ═══════════════════════════════════════════════════════════════════════════════

enum class PointerKind {
    Owner,      // Owns the allocation (e.g., result of malloc)
    Borrowed,   // Borrows from another pointer (e.g., &x, pointer arg)
    Raw,        // Unknown ownership (conservative)
    Temp        // Temporary (stack address, compound literal)
};

enum class BorrowType {
    None,       // Not a borrow
    Shared,     // Shared/immutable borrow (read-only)
    Mutable     // Mutable borrow (read-write)
};

// Enriched type information with ownership
struct SafeTypeInfo {
    TypeInfo base_type;             // Original C type
    PointerKind ptr_kind;           // Ownership classification
    BorrowType borrow_type;         // Borrow type
    bool is_nullable;               // Can be NULL
    std::string alloc_site;         // Where allocated (for diagnostics)
    int alloc_line;                 // Source line of allocation

    SafeTypeInfo()
        : ptr_kind(PointerKind::Raw),
          borrow_type(BorrowType::None),
          is_nullable(true),
          alloc_line(0) {}
};

// ═══════════════════════════════════════════════════════════════════════════════
// LIFETIME ANALYSIS
// ═══════════════════════════════════════════════════════════════════════════════

struct Lifetime {
    std::string name;               // Lifetime name (generated or variable name)
    int start_line;                 // Where lifetime begins
    int end_line;                   // Where lifetime ends (NLL: last use, not scope end)
    std::string scope;              // Function or block scope
    bool is_static;                 // Static lifetime (globals, string literals)

    Lifetime() : start_line(0), end_line(0), is_static(false) {}
};

// Variable with lifetime information
struct LifetimeVar {
    std::string name;
    Lifetime lifetime;
    SafeTypeInfo type;
    bool moved;                     // Has ownership been moved?
    int last_use_line;              // Last use (for NLL)

    LifetimeVar() : moved(false), last_use_line(0) {}
};

// ═══════════════════════════════════════════════════════════════════════════════
// OWNERSHIP TRACKING
// ═══════════════════════════════════════════════════════════════════════════════

enum class OwnershipState {
    Valid,      // Valid, can be used
    Moved,      // Ownership moved to another variable
    Freed,      // Explicitly freed (via free())
    Null,       // Known to be NULL
    Unknown     // Unknown state (conservative)
};

struct OwnershipInfo {
    std::string var_name;
    OwnershipState state;
    int state_line;                 // Line where state changed
    std::string moved_to;           // If Moved, where did it go?
    std::string reason;             // Human-readable reason for state

    OwnershipInfo()
        : state(OwnershipState::Unknown), state_line(0) {}
};

// ═══════════════════════════════════════════════════════════════════════════════
// BORROW TRACKING
// ═══════════════════════════════════════════════════════════════════════════════

struct ActiveBorrow {
    std::string borrowed_var;       // What variable is borrowed
    std::string borrow_var;         // The borrow itself (e.g., &x → p)
    BorrowType type;                // Shared or Mutable
    int start_line;                 // Where borrow begins
    int end_line;                   // Where borrow ends (NLL)
    bool active;                    // Is borrow currently active?

    ActiveBorrow()
        : type(BorrowType::None), start_line(0), end_line(0), active(false) {}
};

// ═══════════════════════════════════════════════════════════════════════════════
// ALIAS ANALYSIS
// ═══════════════════════════════════════════════════════════════════════════════

enum class AliasRelation {
    MustAlias,      // Definitely point to same location
    MayAlias,       // Might point to same location
    NoAlias         // Definitely don't alias
};

struct AliasInfo {
    std::string var1;
    std::string var2;
    AliasRelation relation;
    int confidence;                 // 0-100, how confident in this analysis

    AliasInfo() : relation(AliasRelation::MayAlias), confidence(50) {}
};

// ═══════════════════════════════════════════════════════════════════════════════
// SAFETY DIAGNOSTICS
// ═══════════════════════════════════════════════════════════════════════════════

enum class DiagnosticLevel {
    Error,      // Safety violation, must fix
    Warning,    // Potential issue, should review
    Note        // Informational
};

struct SafetyDiagnostic {
    DiagnosticLevel level;
    std::string message;
    std::string file;
    int line;
    int column;
    std::string suggestion;         // Suggested fix (optional)

    SafetyDiagnostic(DiagnosticLevel lvl, const std::string& msg, int ln)
        : level(lvl), message(msg), line(ln), column(0) {}
};

// ═══════════════════════════════════════════════════════════════════════════════
// SAFETY CONTEXT
// ═══════════════════════════════════════════════════════════════════════════════

class SafetyContext {
public:
    SafetyContext()
        : level_(SafetyLevel::Minimal), error_count_(0), warning_count_(0) {}

    // Configuration
    void setSafetyLevel(SafetyLevel level) { level_ = level; }
    SafetyLevel getSafetyLevel() const { return level_; }

    // Safety level queries (v4.1 - Automatic Memory Management)
    bool shouldInsertAutoFree() const {
        return level_ >= SafetyLevel::Medium;
    }

    bool shouldInsertRuntimeGuards() const {
        return level_ >= SafetyLevel::Full;
    }

    // Current function scope (set at start of each function analysis pass)
    void setCurrentFunction(const std::string& func) { current_function_ = func; }
    const std::string& getCurrentFunction() const { return current_function_; }

    // Type enrichment (scoped by current_function_ to prevent cross-function collisions)
    void setVarType(const std::string& var, const SafeTypeInfo& type) {
        var_types_[current_function_ + "::" + var] = type;
    }

    SafeTypeInfo getVarType(const std::string& var) const {
        auto it = var_types_.find(current_function_ + "::" + var);
        return (it != var_types_.end()) ? it->second : SafeTypeInfo();
    }

    // Lifetime tracking
    void setVarLifetime(const std::string& var, const Lifetime& lt) {
        LifetimeVar lv;
        lv.name = var;
        lv.lifetime = lt;
        lv.type = getVarType(var);
        lifetimes_[var] = lv;
    }

    LifetimeVar* getVarLifetime(const std::string& var) {
        auto it = lifetimes_.find(var);
        return (it != lifetimes_.end()) ? &it->second : nullptr;
    }

    // Ownership tracking
    void setOwnershipState(const std::string& var, OwnershipState state,
                          int line, const std::string& reason) {
        OwnershipInfo info;
        info.var_name = var;
        info.state = state;
        info.state_line = line;
        info.reason = reason;
        ownership_[var] = info;
    }

    OwnershipInfo getOwnershipInfo(const std::string& var) const {
        auto it = ownership_.find(var);
        return (it != ownership_.end()) ? it->second : OwnershipInfo();
    }

    // Borrow tracking
    void addActiveBorrow(const ActiveBorrow& borrow) {
        active_borrows_.push_back(borrow);
    }

    const std::vector<ActiveBorrow>& getActiveBorrows() const {
        return active_borrows_;
    }

    void endBorrow(const std::string& borrow_var, int line) {
        for (auto& b : active_borrows_) {
            if (b.borrow_var == borrow_var && b.active) {
                b.end_line = line;
                b.active = false;
            }
        }
    }

    // Alias tracking
    void addAlias(const AliasInfo& alias) {
        aliases_.push_back(alias);
    }

    AliasRelation getAliasRelation(const std::string& var1, const std::string& var2) const {
        for (const auto& a : aliases_) {
            if ((a.var1 == var1 && a.var2 == var2) ||
                (a.var1 == var2 && a.var2 == var1)) {
                return a.relation;
            }
        }
        return AliasRelation::MayAlias; // Conservative default
    }

    // Diagnostics
    void addDiagnostic(DiagnosticLevel level, const std::string& msg, int line) {
        // Limit diagnostics to prevent memory exhaustion on large files
        static const size_t MAX_DIAGNOSTICS = 1000;

        if (diagnostics_.size() >= MAX_DIAGNOSTICS) {
            if (diagnostics_.size() == MAX_DIAGNOSTICS) {
                // Add one final message about suppression
                diagnostics_.push_back(SafetyDiagnostic(
                    DiagnosticLevel::Note,
                    "Too many diagnostics generated, suppressing further messages",
                    line));
            }
            // Still count errors/warnings but don't store diagnostics
            if (level == DiagnosticLevel::Error) error_count_++;
            if (level == DiagnosticLevel::Warning) warning_count_++;
            return;
        }

        diagnostics_.push_back(SafetyDiagnostic(level, msg, line));
        if (level == DiagnosticLevel::Error) error_count_++;
        if (level == DiagnosticLevel::Warning) warning_count_++;
    }

    const std::vector<SafetyDiagnostic>& getDiagnostics() const {
        return diagnostics_;
    }

    int getErrorCount() const { return error_count_; }
    int getWarningCount() const { return warning_count_; }

    // Report all diagnostics
    void reportDiagnostics(FILE* out = stderr) const {
        for (const auto& diag : diagnostics_) {
            const char* level_str = "note";
            if (diag.level == DiagnosticLevel::Error) level_str = "error";
            else if (diag.level == DiagnosticLevel::Warning) level_str = "warning";

            fprintf(out, "%s:%d: %s: %s\n",
                    diag.file.empty() ? "<input>" : diag.file.c_str(),
                    diag.line, level_str, diag.message.c_str());

            if (!diag.suggestion.empty()) {
                fprintf(out, "%s:%d: note: %s\n",
                        diag.file.empty() ? "<input>" : diag.file.c_str(),
                        diag.line, diag.suggestion.c_str());
            }
        }

        if (error_count_ > 0 || warning_count_ > 0) {
            fprintf(out, "\n%d error(s), %d warning(s) generated.\n",
                    error_count_, warning_count_);
        }
    }

private:
    SafetyLevel level_;
    std::string current_function_;              // Set at start of each function pass
    std::map<std::string, SafeTypeInfo> var_types_;  // Key: "function::varname"
    std::map<std::string, LifetimeVar> lifetimes_;
    std::map<std::string, OwnershipInfo> ownership_;
    std::vector<ActiveBorrow> active_borrows_;
    std::vector<AliasInfo> aliases_;
    std::vector<SafetyDiagnostic> diagnostics_;
    int error_count_;
    int warning_count_;
};

// ═══════════════════════════════════════════════════════════════════════════════
// SAFETY RUNTIME GUARDS
// ═══════════════════════════════════════════════════════════════════════════════

// Guard types for runtime checking
enum class GuardType {
    NullCheck,          // Check pointer != NULL before dereference
    UseAfterFree,       // Check allocation wasn't freed
    DoubleFree,         // Check not already freed
    BorrowConflict,     // Check no conflicting borrows
    MemoryLeak          // Check ownership transferred or freed
};

struct RuntimeGuard {
    GuardType type;
    std::string var_name;
    int line;
    std::string check_code;     // Assembly code for check
    std::string error_msg;      // Error message if check fails

    RuntimeGuard(GuardType t, const std::string& var, int ln)
        : type(t), var_name(var), line(ln) {}
};

// Global configuration (set from command line)
extern SafetyLevel g_safety_level;
extern bool g_safety_enabled;

