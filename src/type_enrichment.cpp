/**
 * Type Enrichment Pass - Implementation
 *
 * Infers pointer ownership from C code patterns without requiring annotations.
 */

#include "type_enrichment.h"
#include <cstring>

void TypeEnrichment::analyze(ASTNode* root) {
    if (!root) return;

    if (root->kind == NodeKind::TranslationUnit) {
        analyzeTranslationUnit(root);
    }
}

void TypeEnrichment::analyzeTranslationUnit(ASTNode* node) {
    for (auto& child : node->children) {
        if (child->kind == NodeKind::FunctionDef) {
            analyzeFunctionDef(child.get());
        } else if (child->kind == NodeKind::Declaration) {
            analyzeDeclaration(child.get());
        }
    }
}

void TypeEnrichment::analyzeFunctionDef(ASTNode* node) {
    // Clear parameter set for this function
    current_params_.clear();

    // Set current function in SafetyContext so all setVarType/getVarType calls
    // use a scoped key (prevents cross-function namespace collisions).
    if (node->children.size() >= 2) {
        std::string fname = extractNameFromDeclarator(node->children[1].get());
        ctx_.setCurrentFunction(fname);
    }

    // FunctionDef: [0]=DeclSpecs [1]=Declarator(function) [2]=Body
    // Extract parameter names from the function declarator and mark as Borrowed
    if (node->children.size() >= 2) {
        extractParamNames(node->children[1].get());
    }

    // Register each parameter as Borrowed so auto-free won't touch them
    for (const auto& param : current_params_) {
        SafeTypeInfo borrowed_info;
        borrowed_info.ptr_kind = PointerKind::Borrowed;
        borrowed_info.borrow_type = BorrowType::None;
        borrowed_info.alloc_site = "function parameter";
        ctx_.setVarType(param, borrowed_info);
    }

    // Analyze function body
    for (auto& child : node->children) {
        if (child->kind == NodeKind::CompoundStmt) {
            analyzeStatement(child.get());
        }
    }
}

// Recursively search a Declarator subtree for a ParamList and extract param names
void TypeEnrichment::extractParamNames(ASTNode* node) {
    if (!node) return;

    if (node->kind == NodeKind::ParamList) {
        for (auto& param_decl : node->children) {
            if (!param_decl || param_decl->kind != NodeKind::ParamDecl) continue;
            if (param_decl->sval == "...") continue;  // variadic ellipsis
            // ParamDecl: [0]=DeclSpecs [1]=Declarator (optional for unnamed params)
            if (param_decl->children.size() >= 2) {
                std::string name = extractNameFromDeclarator(param_decl->children[1].get());
                if (!name.empty()) current_params_.insert(name);
            }
        }
        return;  // Don't recurse further once we found the ParamList
    }

    for (auto& child : node->children) {
        extractParamNames(child.get());
    }
}

// Extract the variable name from a Declarator / DirectDeclarator subtree
std::string TypeEnrichment::extractNameFromDeclarator(ASTNode* node) {
    if (!node) return "";
    if (node->kind == NodeKind::Ident) return node->sval;

    if (node->kind == NodeKind::Declarator && !node->children.empty()) {
        ASTNode* cur = node->children[0].get();
        if (cur && cur->kind == NodeKind::Pointer && node->children.size() > 1)
            cur = node->children[1].get();
        while (cur) {
            if (!cur->sval.empty() && cur->sval != "*") return cur->sval;
            if (cur->kind == NodeKind::DirectDeclarator) {
                if (!cur->children.empty() && cur->children[0]->kind == NodeKind::Ident)
                    return cur->children[0]->sval;
                break;
            }
            if (cur->kind == NodeKind::Declarator && !cur->children.empty()) {
                ASTNode* next = cur->children[0].get();
                if (next && next->kind == NodeKind::Pointer)
                    next = (cur->children.size() > 1) ? cur->children[1].get() : nullptr;
                cur = next;
            } else break;
        }
    }
    return "";
}

void TypeEnrichment::analyzeDeclaration(ASTNode* node) {
    // Look for pointer declarations with initializers
    // NOTE: Declaration structure is [0]=DeclSpecs, [1]=DeclList
    // DeclList contains the InitDeclarator nodes (Bug fix: was looking for direct InitDeclarator)

    for (auto& child : node->children) {
        // Check if this is a DeclList containing InitDeclarator nodes
        if (child->kind == NodeKind::DeclList) {
            // Iterate through InitDeclarator children
            for (auto& init_decl : child->children) {
                if (init_decl->kind == NodeKind::InitDeclarator) {
                    // InitDeclarator has: declarator + initializer (optional)
                    if (init_decl->children.size() >= 2) {
                        ASTNode* declarator = init_decl->children[0].get();
                        ASTNode* initializer = init_decl->children[1].get();

                        // Get variable name - check sval first (nested Declarators store name here)
                        std::string var_name;
                        if (declarator->kind == NodeKind::Declarator &&
                            declarator->children.size() > 0) {
                            // May have Pointer node as first child
                            ASTNode* current = nullptr;
                            if (declarator->children[0]->kind == NodeKind::Pointer) {
                                if (declarator->children.size() > 1) {
                                    current = declarator->children[1].get();
                                }
                            } else {
                                current = declarator->children[0].get();
                            }

                            // Traverse nested Declarators to find name
                            while (current && var_name.empty()) {
                                // Check if name is in sval (common for nested Declarators)
                                if (!current->sval.empty() && current->sval != "*") {
                                    var_name = current->sval;
                                    break;
                                }

                                if (current->kind == NodeKind::DirectDeclarator) {
                                    if (current->children.size() > 0 &&
                                        current->children[0]->kind == NodeKind::Ident) {
                                        var_name = current->children[0]->sval;
                                    }
                                    break;
                                } else if (current->kind == NodeKind::Declarator) {
                                    // Nested Declarator - traverse into it
                                    if (current->children.size() > 0) {
                                        if (current->children[0]->kind == NodeKind::Pointer) {
                                            current = (current->children.size() > 1) ? current->children[1].get() : nullptr;
                                        } else {
                                            current = current->children[0].get();
                                        }
                                    } else {
                                        break;
                                    }
                                } else {
                                    break;
                                }
                            }
                        }

                        if (!var_name.empty()) {
                            // Classify pointer based on initializer
                            PointerKind kind = classifyPointer(initializer);
                            BorrowType borrow = inferBorrowType(initializer);

                            SafeTypeInfo type_info;
                            type_info.ptr_kind = kind;
                            type_info.borrow_type = borrow;
                            type_info.alloc_line = initializer->line;

                            if (kind == PointerKind::Owner) {
                                type_info.alloc_site = "malloc-like allocation";
                            } else if (kind == PointerKind::Borrowed) {
                                type_info.alloc_site = "borrowed reference";
                            }

                            ctx_.setVarType(var_name, type_info);
                        }
                    }
                }
            }
        }
        // Also check for direct InitDeclarator (compatibility/fallback)
        else if (child->kind == NodeKind::InitDeclarator) {
            // InitDeclarator has: declarator + initializer (optional)
            if (child->children.size() >= 2) {
                ASTNode* declarator = child->children[0].get();
                ASTNode* initializer = child->children[1].get();

                // Get variable name
                std::string var_name;
                if (declarator->kind == NodeKind::Declarator &&
                    declarator->children.size() > 0) {
                    // May have Pointer node as first child
                    ASTNode* direct = nullptr;
                    if (declarator->children[0]->kind == NodeKind::Pointer) {
                        if (declarator->children.size() > 1) {
                            direct = declarator->children[1].get();
                        }
                    } else {
                        direct = declarator->children[0].get();
                    }

                    if (direct && direct->kind == NodeKind::DirectDeclarator) {
                        if (direct->children.size() > 0 &&
                            direct->children[0]->kind == NodeKind::Ident) {
                            var_name = direct->children[0]->sval;
                        }
                    }
                }

                if (!var_name.empty()) {
                    // Classify pointer based on initializer
                    PointerKind kind = classifyPointer(initializer);
                    BorrowType borrow = inferBorrowType(initializer);

                    SafeTypeInfo type_info;
                    type_info.ptr_kind = kind;
                    type_info.borrow_type = borrow;
                    type_info.alloc_line = initializer->line;

                    if (kind == PointerKind::Owner) {
                        type_info.alloc_site = "malloc-like allocation";
                    } else if (kind == PointerKind::Borrowed) {
                        type_info.alloc_site = "borrowed reference";
                    }

                    ctx_.setVarType(var_name, type_info);
                }
            }
        }
    }
}

void TypeEnrichment::analyzeStatement(ASTNode* node) {
    if (!node) return;

    switch (node->kind) {
        case NodeKind::CompoundStmt:
        case NodeKind::BlockItems:
            for (auto& child : node->children) {
                analyzeStatement(child.get());
            }
            break;

        case NodeKind::Declaration:
            analyzeDeclaration(node);
            break;

        case NodeKind::ExprStmt:
            if (node->children.size() > 0) {
                analyzeExpr(node->children[0].get());
            }
            break;

        case NodeKind::IfStmt:
            // Analyze condition and both branches
            for (auto& child : node->children) {
                analyzeStatement(child.get());
            }
            break;

        case NodeKind::WhileStmt:
        case NodeKind::DoWhileStmt:
        case NodeKind::ForStmt:
            // Analyze loop body
            for (auto& child : node->children) {
                analyzeStatement(child.get());
            }
            break;

        case NodeKind::ReturnStmt:
            if (node->children.size() > 0) {
                analyzeExpr(node->children[0].get());
            }
            break;

        // Bug #4 fix: Handle switch statements
        case NodeKind::SwitchStmt:
            for (auto& child : node->children) {
                analyzeStatement(child.get());
            }
            break;

        // Bug #4 fix: Handle labeled statements, cases, and defaults
        case NodeKind::LabeledStmt:
        case NodeKind::CaseStmt:
        case NodeKind::DefaultStmt:
            for (auto& child : node->children) {
                analyzeStatement(child.get());
            }
            break;

        // Bug #4 fix: Control flow statements (no analysis needed)
        case NodeKind::GotoStmt:
        case NodeKind::BreakStmt:
        case NodeKind::ContinueStmt:
            break;

        default:
            break;
    }
}

void TypeEnrichment::analyzeExpr(ASTNode* node) {
    if (!node) return;

    // Assignment: track ownership transfer
    if (node->kind == NodeKind::AssignExpr ||
        (node->kind == NodeKind::BinaryExpr && node->sval == "=")) {
        if (node->children.size() >= 2) {
            ASTNode* lhs = node->children[0].get();
            ASTNode* rhs = node->children[1].get();

            // Get LHS variable name
            std::string var_name;
            if (lhs->kind == NodeKind::Ident) {
                var_name = lhs->sval;
            }

            if (!var_name.empty()) {
                // Classify RHS
                PointerKind kind = classifyPointer(rhs);
                BorrowType borrow = inferBorrowType(rhs);

                SafeTypeInfo type_info;
                type_info.ptr_kind = kind;
                type_info.borrow_type = borrow;
                type_info.alloc_line = rhs->line;

                ctx_.setVarType(var_name, type_info);
            }
        }
    }

    // Recursively analyze children
    for (auto& child : node->children) {
        analyzeExpr(child.get());
    }
}

PointerKind TypeEnrichment::classifyPointer(ASTNode* expr) {
    if (!expr) return PointerKind::Raw;

    // Owner: malloc, calloc, realloc
    if (isAllocation(expr)) {
        return PointerKind::Owner;
    }

    // Borrowed: &variable
    if (isAddressOf(expr)) {
        if (isStackAddress(expr)) {
            return PointerKind::Temp;  // Stack address
        }
        return PointerKind::Borrowed;
    }

    // Borrowed: function parameter (TODO: track param info)
    // For now, we'd need additional context from function signature

    // Raw: everything else (conservative)
    return PointerKind::Raw;
}

BorrowType TypeEnrichment::inferBorrowType(ASTNode* expr) {
    if (!expr) return BorrowType::None;

    // Address-of creates a borrow
    if (isAddressOf(expr)) {
        // Conservative: assume Mutable if we can't prove it's Shared
        // This gives fewer false negatives (catches more bugs)
        // but may give some false positives (spurious warnings)
        // TODO: Track usage patterns to determine Mutable vs Shared precisely
        return BorrowType::Mutable;  // Changed from Shared - Bug #7 fix
    }

    return BorrowType::None;
}

bool TypeEnrichment::isAllocation(ASTNode* expr) {
    if (!expr) return false;

    // Look through cast expressions: (int*)malloc(100)
    // CastExpr: child[0]=type_name, child[1]=expression
    ASTNode* actual_expr = expr;
    if (expr->kind == NodeKind::CastExpr && expr->children.size() > 1) {
        actual_expr = expr->children[1].get();  // Get the expression being cast
    }

    // Check for function call to malloc-like functions
    if (actual_expr->kind == NodeKind::CallExpr) {
        if (actual_expr->children.size() > 0) {
            ASTNode* callee = actual_expr->children[0].get();
            if (callee->kind == NodeKind::Ident) {
                const std::string& name = callee->sval;
                return (name == "malloc" || name == "calloc" ||
                        name == "realloc" || name == "aligned_alloc" ||
                        name == "strdup" || name == "strndup" ||
                        name == "asprintf");  // N8: asprintf returns heap-allocated string
            }
        }
    }
    return false;
}

bool TypeEnrichment::isAddressOf(ASTNode* expr) {
    // Check for unary & operator
    if (expr->kind == NodeKind::UnaryExpr) {
        return (expr->sval == "&");
    }
    return false;
}

bool TypeEnrichment::isFunctionParam(const std::string& var) {
    return current_params_.count(var) > 0;
}

bool TypeEnrichment::isStackAddress(ASTNode* expr) {
    // Address-of a local variable
    if (expr->kind == NodeKind::UnaryExpr && expr->sval == "&") {
        if (expr->children.size() > 0) {
            ASTNode* operand = expr->children[0].get();
            if (operand->kind == NodeKind::Ident) {
                // TODO: Check if variable is local (on stack)
                // For now, assume all identifiers are locals (conservative)
                return true;
            }
        }
    }
    return false;
}
