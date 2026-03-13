/**
 * Borrow Checker Pass - Implementation
 *
 * Detects aliasing violations and enforces Rust-style borrow rules.
 *
 * v4.6.0 additions:
 *   W2b  — Mutable-alias detection: writing *p when another borrow of the
 *           same variable is active.
 *   SF   — Struct-field borrow tracking: &s.field is treated as borrowing s.
 */

#include "borrow_checker.h"

// ── Helpers ────────────────────────────────────────────────────────────────

// Return the root variable name from an lvalue expression.
// Handles: Ident, s.field (MemberExpr / BinaryExpr "."), s->field ("->"),
//          arr[i] (SubscriptExpr).
static std::string getBaseVar(ASTNode* node) {
    if (!node) return "";
    if (node->kind == NodeKind::Ident) return node->sval;
    if (node->kind == NodeKind::MemberExpr ||
        (node->kind == NodeKind::BinaryExpr &&
         (node->sval == "." || node->sval == "->")))
        return getBaseVar(node->children.empty() ? nullptr
                                                 : node->children[0].get());
    if (node->kind == NodeKind::SubscriptExpr)
        return getBaseVar(node->children.empty() ? nullptr
                                                 : node->children[0].get());
    return "";
}

// Extract variable name from a Declarator node
static std::string extractDeclName(ASTNode* declarator) {
    if (!declarator) return "";
    if (declarator->kind == NodeKind::Ident) return declarator->sval;

    // Declarator: may have [Pointer,] DirectDeclarator or nested Declarator
    if (declarator->kind == NodeKind::Declarator && !declarator->children.empty()) {
        ASTNode* cur = declarator->children[0].get();
        if (cur && cur->kind == NodeKind::Pointer && declarator->children.size() > 1)
            cur = declarator->children[1].get();
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

// ── Main entry points ───────────────────────────────────────────────────────

void BorrowChecker::analyze(ASTNode* root) {
    if (!root || root->kind != NodeKind::TranslationUnit) return;

    for (auto& child : root->children) {
        if (child->kind == NodeKind::FunctionDef) {
            analyzeFunction(child.get());
        }
    }
}

void BorrowChecker::analyzeFunction(ASTNode* node) {
    current_borrows_.clear();
    ptr_to_var_.clear();
    local_vars_.clear();  // P5.1: reset per-function local variable tracking

    for (auto& child : node->children) {
        if (child->kind == NodeKind::CompoundStmt) {
            analyzeStatement(child.get());
        }
    }

    endBorrowsAtScope(0);
}

// ── Statement analysis ──────────────────────────────────────────────────────

void BorrowChecker::analyzeStatement(ASTNode* node) {
    if (!node) return;

    switch (node->kind) {
        case NodeKind::CompoundStmt:
        case NodeKind::BlockItems: {
            // Scope-aware borrow lifetime: borrows created inside this scope
            // are ended when it exits (NLL-style).
            size_t borrow_checkpoint = current_borrows_.size();

            for (auto& child : node->children) {
                analyzeStatement(child.get());
            }

            deactivateBorrowsFrom(borrow_checkpoint);
            break;
        }

        case NodeKind::ExprStmt:
            if (!node->children.empty()) {
                ASTNode* expr = node->children[0].get();
                // Detect free() called on a borrowed variable
                if (expr && expr->kind == NodeKind::CallExpr &&
                    !expr->children.empty()) {
                    ASTNode* callee = expr->children[0].get();
                    if (callee && callee->kind == NodeKind::Ident &&
                        callee->sval == "free" && expr->children.size() >= 2) {
                        ASTNode* args = expr->children[1].get();
                        ASTNode* arg = (args && args->kind == NodeKind::ArgList &&
                                        !args->children.empty())
                                       ? args->children[0].get() : nullptr;
                        if (arg && arg->kind == NodeKind::Ident) {
                            for (const auto& b : current_borrows_) {
                                if (b.active && b.borrowed_var == arg->sval) {
                                    ctx_.addDiagnostic(DiagnosticLevel::Warning,
                                        "free() called on '" + arg->sval +
                                        "' while it is borrowed by '" +
                                        b.borrow_var + "'", node->line);
                                    break;
                                }
                            }
                        }
                    }
                }
                analyzeExpr(expr);
            }
            break;

        case NodeKind::Declaration:
            // Check for borrow creation: int* p = &x; or int* p = &s.field;
            for (auto& child : node->children) {
                if (child->kind == NodeKind::DeclList) {
                    for (auto& init_decl : child->children) {
                        checkBorrowDecl(init_decl.get(), node->line);
                        // P5.1: Record local variable name for escape analysis
                        if (init_decl->kind == NodeKind::InitDeclarator &&
                            !init_decl->children.empty()) {
                            std::string vn = extractDeclName(init_decl->children[0].get());
                            if (!vn.empty()) local_vars_.insert(vn);
                        }
                    }
                } else if (child->kind == NodeKind::InitDeclarator) {
                    checkBorrowDecl(child.get(), node->line);
                    // P5.1: Record local variable name
                    if (!child->children.empty()) {
                        std::string vn = extractDeclName(child->children[0].get());
                        if (!vn.empty()) local_vars_.insert(vn);
                    }
                }
            }
            break;

        case NodeKind::IfStmt: {
            // Analyze condition
            if (!node->children.empty())
                analyzeStatement(node->children[0].get());
            // Isolate then-branch
            size_t checkpoint = current_borrows_.size();
            if (node->children.size() >= 2)
                analyzeStatement(node->children[1].get());
            deactivateBorrowsFrom(checkpoint);
            // Isolate else-branch
            checkpoint = current_borrows_.size();
            if (node->children.size() >= 3)
                analyzeStatement(node->children[2].get());
            deactivateBorrowsFrom(checkpoint);
            break;
        }

        case NodeKind::WhileStmt:
        case NodeKind::DoWhileStmt:
        case NodeKind::ForStmt:
            for (auto& child : node->children) {
                analyzeStatement(child.get());
            }
            break;

        case NodeKind::SwitchStmt:
            for (auto& child : node->children) {
                analyzeStatement(child.get());
            }
            break;

        case NodeKind::LabeledStmt:
        case NodeKind::CaseStmt:
        case NodeKind::DefaultStmt:
            for (auto& child : node->children) {
                analyzeStatement(child.get());
            }
            break;

        case NodeKind::GotoStmt:
        case NodeKind::BreakStmt:
        case NodeKind::ContinueStmt:
            break;

        case NodeKind::ReturnStmt:
            // P5.1: Check for returning pointer to local variable (dangling pointer)
            if (!node->children.empty()) {
                checkReturnEscape(node->children[0].get(), node->line);
                analyzeExpr(node->children[0].get());
            }
            break;

        default:
            break;
    }
}

// ── Expression analysis ─────────────────────────────────────────────────────

void BorrowChecker::analyzeExpr(ASTNode* node) {
    if (!node) return;

    // Assignment creating borrow: p = &x;  or  p = &s.field;
    if (node->kind == NodeKind::AssignExpr ||
        (node->kind == NodeKind::BinaryExpr && node->sval == "=")) {
        if (node->children.size() >= 2) {
            ASTNode* lhs = node->children[0].get();
            ASTNode* rhs = node->children[1].get();

            // RHS is address-of
            if (rhs && rhs->kind == NodeKind::UnaryExpr && rhs->sval == "&" &&
                !rhs->children.empty() && lhs && lhs->kind == NodeKind::Ident) {
                std::string borrowed = getBaseVar(rhs->children[0].get());
                if (!borrowed.empty()) {
                    trackBorrowCreation(borrowed, lhs->sval,
                                        BorrowType::Shared, node->line);
                }
            }

            // W2b: LHS is a dereference write — *p = value
            // Detect write through a borrow pointer.
            if (lhs && lhs->kind == NodeKind::UnaryExpr && lhs->sval == "*" &&
                !lhs->children.empty()) {
                ASTNode* ptr_node = lhs->children[0].get();
                if (ptr_node && ptr_node->kind == NodeKind::Ident) {
                    checkMutableAccess(ptr_node->sval, node->line);
                }
            }
        }
    }

    // W2b: Compound assignment writing through a pointer: *p += value etc.
    if (node->kind == NodeKind::BinaryExpr &&
        (node->sval == "+=" || node->sval == "-=" || node->sval == "*=" ||
         node->sval == "/=" || node->sval == "%=" || node->sval == "&=" ||
         node->sval == "|=" || node->sval == "^=" || node->sval == "<<=" ||
         node->sval == ">>=")) {
        if (!node->children.empty()) {
            ASTNode* lhs = node->children[0].get();
            if (lhs && lhs->kind == NodeKind::UnaryExpr && lhs->sval == "*" &&
                !lhs->children.empty()) {
                ASTNode* ptr_node = lhs->children[0].get();
                if (ptr_node && ptr_node->kind == NodeKind::Ident) {
                    checkMutableAccess(ptr_node->sval, node->line);
                }
            }
        }
    }

    // Check for use of a variable that is mutably borrowed
    if (node->kind == NodeKind::Ident) {
        checkBorrowConflicts(node->sval, node->line);
    }

    // Recurse into children
    for (auto& child : node->children) {
        analyzeExpr(child.get());
    }
}

// ── Borrow declaration check ────────────────────────────────────────────────

// Check a single InitDeclarator for borrow creation:
//   int* p = &x;          → borrows x
//   int* p = &s.field;    → borrows s  (struct-field borrow tracking, SF)
void BorrowChecker::checkBorrowDecl(ASTNode* init_decl, int line) {
    if (!init_decl || init_decl->kind != NodeKind::InitDeclarator) return;
    if (init_decl->children.size() < 2) return;

    ASTNode* declarator = init_decl->children[0].get();
    ASTNode* init       = init_decl->children[1].get();
    if (!init) return;

    if (init->kind == NodeKind::UnaryExpr && init->sval == "&" &&
        !init->children.empty()) {
        // SF: use getBaseVar to handle s.field, arr[i], etc.
        std::string borrowed = getBaseVar(init->children[0].get());
        if (!borrowed.empty()) {
            std::string borrow = extractDeclName(declarator);
            if (!borrow.empty()) {
                trackBorrowCreation(borrowed, borrow, BorrowType::Shared, line);
            }
        }
    }
}

// P5.1: Cross-call lifetime — check if return expression escapes a local variable
//
// Detects two patterns:
//  (a) return &local;               — direct address-of local
//  (b) return ptr;  (where ptr = &local via ptr_to_var_)
//  (c) return (cast)&local;         — cast wrapping an address-of
//
// These are classic "dangling pointer" bugs in C. The local variable is
// destroyed when the function returns; any pointer to it becomes invalid.
void BorrowChecker::checkReturnEscape(ASTNode* expr, int line) {
    if (!expr) return;

    // (a) return &local;
    if (expr->kind == NodeKind::UnaryExpr && expr->sval == "&" &&
        !expr->children.empty()) {
        std::string var = getBaseVar(expr->children[0].get());
        if (!var.empty() && local_vars_.count(var)) {
            ctx_.addDiagnostic(DiagnosticLevel::Warning,
                "returning address of local variable '" + var +
                "' — pointer will be dangling after return", line);
        }
        return;
    }

    // (b) return ptr;  where ptr_to_var_["ptr"] is a local
    if (expr->kind == NodeKind::Ident) {
        auto pit = ptr_to_var_.find(expr->sval);
        if (pit != ptr_to_var_.end() && local_vars_.count(pit->second)) {
            ctx_.addDiagnostic(DiagnosticLevel::Warning,
                "returning pointer '" + expr->sval + "' that borrows local variable '" +
                pit->second + "' — use-after-return is undefined behavior", line);
        }
        return;
    }

    // (c) return (type)expr;  — strip cast and recurse
    if (expr->kind == NodeKind::CastExpr && !expr->children.empty()) {
        // CastExpr: children[0] = type, children[1] = expression (if 2 children)
        // or children[0] = expression (if 1 child)
        if (expr->children.size() >= 2) {
            checkReturnEscape(expr->children[1].get(), line);
        } else {
            checkReturnEscape(expr->children[0].get(), line);
        }
        return;
    }
}

// ── Borrow tracking ─────────────────────────────────────────────────────────

void BorrowChecker::trackBorrowCreation(const std::string& borrowed_var,
                                        const std::string& borrow_var,
                                        BorrowType type, int line) {
    if (borrowed_var.empty() || borrow_var.empty()) return;

    // P2-B: Multiple simultaneous borrows warning.
    // In Rust, any two active borrows of the same variable through different
    // pointer variables creates potential aliasing.  We warn (not error) because
    // C deliberately allows it, but it often indicates unsafe code.
    for (const auto& b : current_borrows_) {
        if (b.active && b.borrowed_var == borrowed_var && b.borrow_var != borrow_var) {
            ctx_.addDiagnostic(DiagnosticLevel::Warning,
                "Simultaneous aliases: '" + borrow_var + "' and '" + b.borrow_var +
                "' both borrow '" + borrowed_var +
                "' — writes through either pointer may alias", line);
            break;  // one warning per new borrow is enough
        }
    }

    // Conflict checks (existing logic)
    if (type == BorrowType::Mutable) {
        if (hasMutableBorrow(borrowed_var) || countSharedBorrows(borrowed_var) > 0) {
            ctx_.addDiagnostic(DiagnosticLevel::Error,
                "Cannot create mutable borrow: variable '" + borrowed_var +
                "' is already borrowed", line);
            return;
        }
    } else if (type == BorrowType::Shared) {
        if (hasMutableBorrow(borrowed_var)) {
            ctx_.addDiagnostic(DiagnosticLevel::Error,
                "Cannot create shared borrow: variable '" + borrowed_var +
                "' has active mutable borrow", line);
            return;
        }
    }

    // Create borrow
    ActiveBorrow borrow;
    borrow.borrowed_var = borrowed_var;
    borrow.borrow_var   = borrow_var;
    borrow.type         = type;
    borrow.start_line   = line;
    borrow.active       = true;

    current_borrows_.push_back(borrow);
    ctx_.addActiveBorrow(borrow);

    // W2b: Record pointer → borrowed-variable mapping
    ptr_to_var_[borrow_var] = borrowed_var;
}

// W2b: Detect mutable alias — writing *ptr_var when another borrow of the same
// variable is active.
void BorrowChecker::checkMutableAccess(const std::string& ptr_var, int line) {
    auto it = ptr_to_var_.find(ptr_var);
    if (it == ptr_to_var_.end()) return;  // ptr doesn't borrow anything tracked

    const std::string& borrowed = it->second;

    // Look for OTHER active borrows of the same variable
    for (const auto& b : current_borrows_) {
        if (b.active && b.borrowed_var == borrowed && b.borrow_var != ptr_var) {
            ctx_.addDiagnostic(DiagnosticLevel::Error,
                "Cannot write through '" + ptr_var + "': '" + borrowed +
                "' is also borrowed by '" + b.borrow_var + "'", line);
            return;  // report only the first conflict
        }
    }
}

void BorrowChecker::checkBorrowConflicts(const std::string& var, int line) {
    for (const auto& borrow : current_borrows_) {
        if (borrow.active && borrow.borrowed_var == var) {
            if (borrow.type == BorrowType::Mutable) {
                ctx_.addDiagnostic(DiagnosticLevel::Error,
                    "Cannot use '" + var + "': mutably borrowed by '" +
                    borrow.borrow_var + "'", line);
            }
            // Shared borrows allow reads
        }
    }
}

// Deactivate all borrows from index 'checkpoint' onward and remove their
// ptr_to_var_ entries.
void BorrowChecker::deactivateBorrowsFrom(size_t checkpoint) {
    for (size_t i = checkpoint; i < current_borrows_.size(); i++) {
        if (current_borrows_[i].active) {
            current_borrows_[i].active = false;
            ptr_to_var_.erase(current_borrows_[i].borrow_var);
        }
    }
}

void BorrowChecker::endBorrowsAtScope(int line) {
    for (auto& borrow : current_borrows_) {
        if (borrow.active) {
            borrow.active   = false;
            borrow.end_line = line;
            ctx_.endBorrow(borrow.borrow_var, line);
            ptr_to_var_.erase(borrow.borrow_var);
        }
    }
}

// ── Helpers ─────────────────────────────────────────────────────────────────

bool BorrowChecker::hasMutableBorrow(const std::string& var) {
    for (const auto& borrow : current_borrows_) {
        if (borrow.active && borrow.borrowed_var == var &&
            borrow.type == BorrowType::Mutable) {
            return true;
        }
    }
    return false;
}

int BorrowChecker::countSharedBorrows(const std::string& var) {
    int count = 0;
    for (const auto& borrow : current_borrows_) {
        if (borrow.active && borrow.borrowed_var == var &&
            borrow.type == BorrowType::Shared) {
            count++;
        }
    }
    return count;
}

bool BorrowChecker::isBorrowActive(const std::string& borrow_var) {
    for (const auto& borrow : current_borrows_) {
        if (borrow.active && borrow.borrow_var == borrow_var) {
            return true;
        }
    }
    return false;
}
