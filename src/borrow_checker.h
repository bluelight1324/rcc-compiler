#pragma once
/**
 * Borrow Checker Pass
 *
 * Enforces Rust-style borrow rules:
 * - Mutable XOR shared: Either one mutable borrow OR multiple shared borrows
 * - No use-while-borrowed: Original variable can't be used while borrowed
 * - Borrow lifetime: Borrow must not outlive borrowed data
 */

#include "safety.h"
#include "ast.h"
#include <map>
#include <set>

class BorrowChecker {
public:
    BorrowChecker(SafetyContext& ctx) : ctx_(ctx) {}

    // Main entry point
    void analyze(ASTNode* root);

private:
    SafetyContext& ctx_;
    std::vector<ActiveBorrow> current_borrows_;

    // W2b: Maps borrow-pointer name → borrowed variable name.
    // e.g., after "int* p = &x;", ptr_to_var_["p"] = "x".
    // Entries are removed when the borrow is deactivated (scope exit).
    std::map<std::string, std::string> ptr_to_var_;

    // P5.1: Cross-call lifetime tracking.
    // Set of local variable names declared in the current function body.
    // Used to detect "return pointer to local" and similar dangling-pointer escapes.
    std::set<std::string> local_vars_;

    // Analysis methods
    void analyzeFunction(ASTNode* node);
    void analyzeStatement(ASTNode* node);
    void analyzeExpr(ASTNode* node);
    void checkBorrowDecl(ASTNode* init_decl, int line);
    void checkReturnEscape(ASTNode* return_expr, int line); // P5.1

    // Borrow operations
    void trackBorrowCreation(const std::string& borrowed_var,
                            const std::string& borrow_var,
                            BorrowType type, int line);
    void checkBorrowConflicts(const std::string& var, int line);
    // W2b: Detect mutable alias — writing *ptr when another borrow of the same
    //       variable exists.  ptr_var is the pointer used for the write.
    void checkMutableAccess(const std::string& ptr_var, int line);
    void endBorrowsAtScope(int line);
    // Deactivate borrows from index 'checkpoint' onward and remove ptr_to_var_ entries.
    void deactivateBorrowsFrom(size_t checkpoint);

    // Helpers
    bool hasMutableBorrow(const std::string& var);
    int countSharedBorrows(const std::string& var);
    bool isBorrowActive(const std::string& borrow_var);
};

