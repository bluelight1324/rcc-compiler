#pragma once
/**
 * Ownership Analysis Pass
 *
 * Tracks ownership transfers and state changes:
 * - Detects use-after-free (access after free())
 * - Detects double-free (free() twice)
 * - Detects use-after-move (access after ownership transfer)
 * - Detects memory leaks (ownership lost without free())
 */

#include "safety.h"
#include "ast.h"

class OwnershipAnalysis {
public:
    OwnershipAnalysis(SafetyContext& ctx) : ctx_(ctx) {}

    // Main entry point
    void analyze(ASTNode* root);

private:
    SafetyContext& ctx_;

    // Analysis methods
    void analyzeFunction(ASTNode* node);
    void analyzeStatement(ASTNode* node, std::map<std::string, OwnershipState>& state);
    void analyzeExpr(ASTNode* node, std::map<std::string, OwnershipState>& state);

    // Ownership operations
    void trackAllocation(const std::string& var, int line,
                        std::map<std::string, OwnershipState>& state);
    void trackFree(const std::string& var, int line,
                  std::map<std::string, OwnershipState>& state);
    void trackMove(const std::string& from, const std::string& to, int line,
                  std::map<std::string, OwnershipState>& state);
    void trackUse(const std::string& var, int line,
                 std::map<std::string, OwnershipState>& state);

    // Branch / loop state merging
    void mergeStates(std::map<std::string, OwnershipState>& result,
                     const std::map<std::string, OwnershipState>& then_state,
                     const std::map<std::string, OwnershipState>& else_state);

    // Helpers
    std::string getCallTarget(ASTNode* call);
    std::string getVarName(ASTNode* expr);
    bool isOwnershipTransfer(ASTNode* assign);
};

