#pragma once
/**
 * Type Enrichment Pass
 *
 * Classifies pointers into ownership categories:
 * - Owner: Owns heap allocation (malloc, calloc, etc.)
 * - Borrowed: Borrows from another variable (&x, function param)
 * - Raw: Unknown ownership (conservative)
 * - Temp: Temporary stack address
 */

#include "safety.h"
#include "ast.h"
#include "codegen.h"
#include <set>

class TypeEnrichment {
public:
    TypeEnrichment(SafetyContext& ctx) : ctx_(ctx) {}

    // Main entry point
    void analyze(ASTNode* root);

private:
    SafetyContext& ctx_;
    std::set<std::string> current_params_;  // Names of current function's parameters

    // Analysis methods
    void analyzeTranslationUnit(ASTNode* node);
    void analyzeFunctionDef(ASTNode* node);
    void analyzeDeclaration(ASTNode* node);
    void analyzeStatement(ASTNode* node);
    void analyzeExpr(ASTNode* node);

    // Parameter extraction
    void extractParamNames(ASTNode* declarator);
    std::string extractNameFromDeclarator(ASTNode* declarator);

    // Pointer classification
    PointerKind classifyPointer(ASTNode* expr);
    BorrowType inferBorrowType(ASTNode* expr);

    // Helpers
    bool isAllocation(ASTNode* expr);      // malloc, calloc, etc.
    bool isAddressOf(ASTNode* expr);       // &variable
    bool isFunctionParam(const std::string& var);
    bool isStackAddress(ASTNode* expr);    // Address of local variable
};

