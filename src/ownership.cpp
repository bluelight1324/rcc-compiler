/**
 * Ownership Analysis Pass - Implementation
 *
 * Tracks ownership state through program execution to detect memory safety violations.
 */

#include "ownership.h"
#include <set>

// Helper: extract function name from a FunctionDef's Declarator (children[1])
static std::string getFuncName(ASTNode* func_node) {
    if (!func_node || func_node->children.size() < 2) return "";
    ASTNode* decl = func_node->children[1].get();
    if (!decl) return "";
    if (!decl->sval.empty() && decl->sval != "*") return decl->sval;
    if (decl->kind == NodeKind::Declarator && !decl->children.empty()) {
        ASTNode* cur = decl->children[0].get();
        if (cur && cur->kind == NodeKind::Pointer && decl->children.size() > 1)
            cur = decl->children[1].get();
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

void OwnershipAnalysis::analyze(ASTNode* root) {
    if (!root || root->kind != NodeKind::TranslationUnit) return;

    // Analyze each function separately
    for (auto& child : root->children) {
        if (child->kind == NodeKind::FunctionDef) {
            analyzeFunction(child.get());
        }
    }
}

void OwnershipAnalysis::analyzeFunction(ASTNode* node) {
    // Set SafetyContext function scope so getVarType uses scoped keys.
    ctx_.setCurrentFunction(getFuncName(node));

    // Local ownership state map: variable name → ownership state
    std::map<std::string, OwnershipState> state;

    // Analyze function body
    for (auto& child : node->children) {
        if (child->kind == NodeKind::CompoundStmt) {
            analyzeStatement(child.get(), state);
        }
    }

    // Check for memory leaks at function exit
    for (const auto& [var, st] : state) {
        if (st == OwnershipState::Valid) {
            SafeTypeInfo type = ctx_.getVarType(var);
            if (type.ptr_kind == PointerKind::Owner) {
                ctx_.addDiagnostic(DiagnosticLevel::Warning,
                    "Memory leak: variable '" + var + "' goes out of scope without being freed",
                    0); // TODO: Get actual line number
            }
        }
    }
}

void OwnershipAnalysis::analyzeStatement(ASTNode* node,
                                         std::map<std::string, OwnershipState>& state) {
    if (!node) return;

    switch (node->kind) {
        case NodeKind::CompoundStmt:
        case NodeKind::BlockItems:
            for (auto& child : node->children) {
                analyzeStatement(child.get(), state);
            }
            break;

        case NodeKind::Declaration:
            // Track variable declarations with initializers
            for (auto& child : node->children) {
                if (child->kind == NodeKind::InitDeclarator && child->children.size() >= 2) {
                    std::string var = getVarName(child->children[0].get());
                    ASTNode* init = child->children[1].get();

                    if (!var.empty()) {
                        SafeTypeInfo type = ctx_.getVarType(var);
                        if (type.ptr_kind == PointerKind::Owner) {
                            // Owner initialized - mark as Valid
                            trackAllocation(var, node->line, state);
                        }
                    }
                }
            }
            break;

        case NodeKind::ExprStmt:
            if (node->children.size() > 0) {
                analyzeExpr(node->children[0].get(), state);
            }
            break;

        case NodeKind::IfStmt: {
            // Children: [0]=condition, [1]=then_stmt, [2]=else_stmt (optional)
            if (!node->children.empty())
                analyzeExpr(node->children[0].get(), state);

            // Analyze branches independently, then merge conservatively
            auto then_state = state;
            auto else_state = state;

            if (node->children.size() >= 2)
                analyzeStatement(node->children[1].get(), then_state);
            if (node->children.size() >= 3)
                analyzeStatement(node->children[2].get(), else_state);
            else
                // No else branch: else_state == pre-if state (variable still valid)
                else_state = state;

            mergeStates(state, then_state, else_state);
            break;
        }

        case NodeKind::WhileStmt:
        case NodeKind::DoWhileStmt:
        case NodeKind::ForStmt: {
            // Analyze loop body; detect double-free risk (free on iteration 1,
            // same var still in scope on iteration 2).
            auto pre_loop = state;
            for (auto& child : node->children) {
                analyzeStatement(child.get(), state);
            }
            // Warn about variables that were Valid before loop but freed inside it.
            for (const auto& [var, st] : state) {
                if (st == OwnershipState::Freed) {
                    auto it = pre_loop.find(var);
                    if (it != pre_loop.end() && it->second == OwnershipState::Valid) {
                        ctx_.addDiagnostic(DiagnosticLevel::Warning,
                            "Potential double-free in loop: '" + var +
                            "' freed inside loop body - may be freed again on next iteration",
                            node->line);
                    }
                }
            }
            break;
        }

        case NodeKind::ReturnStmt:
            if (node->children.size() > 0) {
                analyzeExpr(node->children[0].get(), state);
                // Ownership transferred to caller
                std::string var = getVarName(node->children[0].get());
                if (!var.empty()) {
                    SafeTypeInfo type = ctx_.getVarType(var);
                    if (type.ptr_kind == PointerKind::Owner) {
                        trackMove(var, "<return>", node->line, state);
                    }
                }
            }
            break;

        // Bug #4 fix: Handle switch statements
        case NodeKind::SwitchStmt:
            // Analyze condition and body
            for (auto& child : node->children) {
                analyzeStatement(child.get(), state);
            }
            break;

        // Bug #4 fix: Handle labeled statements, cases, and defaults
        case NodeKind::LabeledStmt:
        case NodeKind::CaseStmt:
        case NodeKind::DefaultStmt:
            // These contain statements as children - analyze them
            for (auto& child : node->children) {
                analyzeStatement(child.get(), state);
            }
            break;

        // Bug #4 fix: Handle control flow statements
        case NodeKind::GotoStmt:
        case NodeKind::BreakStmt:
        case NodeKind::ContinueStmt:
            // No analysis needed (control flow only, no ownership changes)
            break;

        default:
            break;
    }
}

void OwnershipAnalysis::analyzeExpr(ASTNode* node,
                                    std::map<std::string, OwnershipState>& state) {
    if (!node) return;

    // Function call - handled specially to ensure correct state ordering.
    // Args are evaluated BEFORE the call effect (e.g., free(p) should NOT flag
    // p as use-after-free - the arg is valid at the point it's evaluated).
    if (node->kind == NodeKind::CallExpr) {
        // Recurse into children first (args evaluated before call takes effect)
        for (auto& child : node->children) {
            analyzeExpr(child.get(), state);
        }

        std::string target = getCallTarget(node);
        if (target == "free" && node->children.size() >= 2) {
            // Extract arg: CallExpr[1] is ArgList, ArgList[0] is the pointer
            ASTNode* arg_node = node->children[1].get();
            std::string var;
            if (arg_node && arg_node->kind == NodeKind::ArgList &&
                !arg_node->children.empty()) {
                var = getVarName(arg_node->children[0].get());
            } else {
                var = getVarName(arg_node);  // fallback for non-ArgList
            }
            if (!var.empty()) {
                trackFree(var, node->line, state);
            }
        }
        return;  // Already handled children above
    }

    // Assignment - check for ownership transfer
    if (node->kind == NodeKind::AssignExpr ||
        (node->kind == NodeKind::BinaryExpr && node->sval == "=")) {
        if (node->children.size() >= 2) {
            std::string lhs = getVarName(node->children[0].get());
            std::string rhs = getVarName(node->children[1].get());

            if (!lhs.empty() && !rhs.empty()) {
                SafeTypeInfo rhs_type = ctx_.getVarType(rhs);
                if (rhs_type.ptr_kind == PointerKind::Owner) {
                    // Ownership transfer: rhs → lhs
                    trackMove(rhs, lhs, node->line, state);
                    trackAllocation(lhs, node->line, state);
                }
            }
        }
    }

    // Variable use - check state (UAF / use-after-move detection)
    if (node->kind == NodeKind::Ident) {
        std::string var = node->sval;
        SafeTypeInfo type = ctx_.getVarType(var);
        if (type.ptr_kind == PointerKind::Owner ||
            type.ptr_kind == PointerKind::Borrowed) {
            trackUse(var, node->line, state);
        }
    }

    // UAF dereference: *freed_ptr  or  freed_ptr->field
    if (node->kind == NodeKind::UnaryExpr && (node->sval == "*" || node->sval == "STAR")) {
        if (!node->children.empty()) {
            std::string var = getVarName(node->children[0].get());
            if (!var.empty()) {
                auto it = state.find(var);
                if (it != state.end() && it->second == OwnershipState::Freed) {
                    ctx_.addDiagnostic(DiagnosticLevel::Error,
                        "use of freed pointer '*" + var + "' (dereference after free)", node->line);
                }
            }
        }
    }
    if (node->kind == NodeKind::MemberExpr && node->sval == "->") {
        if (!node->children.empty()) {
            std::string var = getVarName(node->children[0].get());
            if (!var.empty()) {
                auto it = state.find(var);
                if (it != state.end() && it->second == OwnershipState::Freed) {
                    ctx_.addDiagnostic(DiagnosticLevel::Error,
                        "use of freed pointer '" + var + "->'(member access after free)", node->line);
                }
            }
        }
    }

    // Recursively analyze children
    for (auto& child : node->children) {
        analyzeExpr(child.get(), state);
    }
}

void OwnershipAnalysis::trackAllocation(const std::string& var, int line,
                                        std::map<std::string, OwnershipState>& state) {
    state[var] = OwnershipState::Valid;
    ctx_.setOwnershipState(var, OwnershipState::Valid, line, "Allocation");
}

void OwnershipAnalysis::trackFree(const std::string& var, int line,
                                  std::map<std::string, OwnershipState>& state) {
    auto it = state.find(var);
    if (it == state.end()) {
        ctx_.addDiagnostic(DiagnosticLevel::Warning,
            "free() called on untracked variable '" + var + "'", line);
        return;
    }

    if (it->second == OwnershipState::Freed) {
        // Double-free
        ctx_.addDiagnostic(DiagnosticLevel::Error,
            "Double-free detected: variable '" + var + "' already freed", line);
    } else if (it->second == OwnershipState::Moved) {
        // Free after move
        ctx_.addDiagnostic(DiagnosticLevel::Error,
            "Use-after-move: free() called on moved variable '" + var + "'", line);
    } else {
        // Valid free
        state[var] = OwnershipState::Freed;
        ctx_.setOwnershipState(var, OwnershipState::Freed, line, "Explicit free()");
    }
}

void OwnershipAnalysis::trackMove(const std::string& from, const std::string& to,
                                  int line, std::map<std::string, OwnershipState>& state) {
    auto it = state.find(from);
    if (it != state.end() && it->second == OwnershipState::Valid) {
        state[from] = OwnershipState::Moved;
        ctx_.setOwnershipState(from, OwnershipState::Moved, line,
            "Ownership moved to '" + to + "'");
    }
}

void OwnershipAnalysis::trackUse(const std::string& var, int line,
                                 std::map<std::string, OwnershipState>& state) {
    auto it = state.find(var);
    if (it == state.end()) return; // Not tracked

    if (it->second == OwnershipState::Freed) {
        // Use-after-free
        ctx_.addDiagnostic(DiagnosticLevel::Error,
            "Use-after-free: variable '" + var + "' used after free()", line);
    } else if (it->second == OwnershipState::Moved) {
        // Use-after-move
        ctx_.addDiagnostic(DiagnosticLevel::Error,
            "Use-after-move: variable '" + var + "' used after ownership transfer", line);
    }
}

std::string OwnershipAnalysis::getCallTarget(ASTNode* call) {
    if (call->kind == NodeKind::CallExpr && call->children.size() > 0) {
        ASTNode* callee = call->children[0].get();
        if (callee->kind == NodeKind::Ident) {
            return callee->sval;
        }
    }
    return "";
}

std::string OwnershipAnalysis::getVarName(ASTNode* expr) {
    if (!expr) return "";

    if (expr->kind == NodeKind::Ident) {
        return expr->sval;
    }

    // Handle declarators
    if (expr->kind == NodeKind::Declarator && expr->children.size() > 0) {
        // Skip Pointer node if present
        ASTNode* direct = expr->children[0].get();
        if (!direct) return "";  // FIX: Add null check

        if (direct->kind == NodeKind::Pointer && expr->children.size() > 1) {
            direct = expr->children[1].get();
            if (!direct) return "";  // FIX: Add null check
        }

        if (direct->kind == NodeKind::DirectDeclarator && direct->children.size() > 0) {
            if (direct->children[0] && direct->children[0]->kind == NodeKind::Ident) {
                // FIX: Add null check before dereferencing
                return direct->children[0]->sval;
            }
        }
    }

    return "";
}

void OwnershipAnalysis::mergeStates(std::map<std::string, OwnershipState>& result,
                                    const std::map<std::string, OwnershipState>& then_state,
                                    const std::map<std::string, OwnershipState>& else_state) {
    // Union of all variable names seen in either branch
    std::set<std::string> all_vars;
    for (const auto& [k, v] : then_state) all_vars.insert(k);
    for (const auto& [k, v] : else_state) all_vars.insert(k);

    for (const auto& var : all_vars) {
        auto ti = then_state.find(var);
        auto ei = else_state.find(var);

        OwnershipState ts = (ti != then_state.end()) ? ti->second : OwnershipState::Valid;
        OwnershipState es = (ei != else_state.end()) ? ei->second : OwnershipState::Valid;

        if (ts == es) {
            result[var] = ts;  // Both branches agree
        } else if (ts == OwnershipState::Freed || es == OwnershipState::Freed) {
            // Freed on one branch only → conservatively Freed (worst case for callers)
            result[var] = OwnershipState::Freed;
        } else if (ts == OwnershipState::Moved || es == OwnershipState::Moved) {
            result[var] = OwnershipState::Moved;
        } else {
            result[var] = OwnershipState::Valid;
        }
    }
}

bool OwnershipAnalysis::isOwnershipTransfer(ASTNode* assign) {
    // Check if RHS is an owning pointer
    if (assign->children.size() >= 2) {
        ASTNode* rhs = assign->children[1].get();
        std::string var = getVarName(rhs);
        if (!var.empty()) {
            SafeTypeInfo type = ctx_.getVarType(var);
            return (type.ptr_kind == PointerKind::Owner);
        }
    }
    return false;
}
