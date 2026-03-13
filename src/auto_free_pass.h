#pragma once
#include "ast.h"
#include "safety.h"
#include "auto_memory.h"
#include <string>
#include <unordered_set>

/**
 * AutoFreePass - Transformation Pass for Automatic Memory Management
 *
 * Inserts automatic free() calls for OWNER pointers at scope exit.
 *
 * Transformation:
 *   void func() {
 *       int* p = malloc(100);  // OWNER
 *       // ... use p ...
 *   }  // <-- Inserts: if (p) free(p);
 *
 * Features:
 *   - Scope-based cleanup (RAII-style in C)
 *   - Respects manual free() calls (no double-free)
 *   - Detects ownership transfers (return, assignment to global/struct)
 *   - Handles nested scopes, early returns, loops
 *
 * Safety levels:
 *   - Medium: Auto-free only
 *   - Full: Auto-free + runtime guards (__guarded_free, __check_ptr)
 *
 * Usage (in main.cpp after safety analysis):
 *   if (safety_ctx.shouldInsertAutoFree()) {
 *       AutoFreePass auto_free(safety_ctx);
 *       auto_free.transform(ast.get());
 *   }
 */
class AutoFreePass {
public:
    explicit AutoFreePass(SafetyContext& ctx);

    // Transform entire AST - inserts automatic free() calls
    void transform(ASTNode* root);

private:
    SafetyContext& ctx_;
    AutoMemoryManager memory_mgr_;

    // Track variables manually freed (to prevent double-free)
    std::unordered_set<std::string> manually_freed_;

    // Track Owner variables declared in the CURRENT function (cleared per-function).
    // B5 only triggers for variables in this set, preventing false positives from
    // SafetyContext namespace collisions across functions (e.g. two functions both
    // declare "int* p"; test_N's p being Owner must not affect test_M's uninitialized p).
    std::unordered_set<std::string> function_owner_vars_;

    // Track Owner variables that have been INITIALIZED (have received a malloc value).
    // Uninitialized declarations like "int* p;" must NOT trigger pre-reassign free,
    // since p holds garbage at that point (not a valid heap pointer).
    // A variable enters this set when:
    //   (a) declared with a malloc initializer: int* p = malloc(...)
    //   (b) assigned for the first time via insertFreeBeforeReassign (it was initialized before)
    // Cleared per-function to prevent cross-function interference.
    std::unordered_set<std::string> initialized_owner_vars_;

    // AST transformation methods
    void transformFunction(ASTNode* func);
    void transformCompoundStmt(ASTNode* stmt);
    void processDeclaration(ASTNode* decl);
    void scanForManualFree(ASTNode* node);

    // AST node creation helpers
    ASTNode* createFreeCall(const std::string& var_name, int line);
    ASTNode* createNullCheck(const std::string& var_name, int line);
    ASTNode* createIfStatement(ASTNode* condition, ASTNode* body, int line);
    ASTNode* createGuardedFreeCall(const std::string& var_name, int line);
    ASTNode* createCompoundStmt(int line);

    // AST query helpers
    bool isFunctionDef(ASTNode* node) const;
    bool isCompoundStmt(ASTNode* node) const;
    bool isDeclaration(ASTNode* node) const;
    bool isFreeCall(ASTNode* node) const;
    bool isReturnStmt(ASTNode* node) const;

    // AST extraction helpers
    std::string extractVarName(ASTNode* decl) const;
    std::string extractFreeArg(ASTNode* call) const;
    ASTNode* getFunctionBody(ASTNode* func) const;

    // Insert cleanup code at scope exit
    void insertCleanupAtScopeExit(ASTNode* scope);

    // Insert cleanup code before early return statements
    void insertCleanupBeforeReturn(ASTNode* parent_scope, size_t return_index);

    // Pointer reassignment leak detection (B5)
    bool isOwnerReassignment(ASTNode* stmt) const;
    bool isAllocationExpr(ASTNode* expr) const;
    void insertFreeBeforeReassign(ASTNode* scope, size_t stmt_index);
};
