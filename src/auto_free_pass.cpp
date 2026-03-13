#include "auto_free_pass.h"
#include <algorithm>

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

AutoFreePass::AutoFreePass(SafetyContext& ctx)
    : ctx_(ctx), memory_mgr_(ctx) {}

void AutoFreePass::transform(ASTNode* root) {
    if (!root) return;

    // Transform each function definition
    for (auto& child : root->children) {
        if (isFunctionDef(child.get())) {
            transformFunction(child.get());
        }
    }
}

void AutoFreePass::transformFunction(ASTNode* func) {
    if (!func || func->children.size() < 3) return;

    // Clear per-function state
    manually_freed_.clear();
    function_owner_vars_.clear();      // Prevent cross-function namespace collisions in B5
    initialized_owner_vars_.clear();  // Reset per-function: only track vars that have received a value

    // Set SafetyContext function scope so getVarType/setVarType use scoped keys.
    ctx_.setCurrentFunction(getFuncName(func));

    memory_mgr_.enterScope();  // Function scope

    // Function structure: [0]=decl_specs, [1]=declarator, [2]=body
    ASTNode* body = func->children[2].get();

    if (isCompoundStmt(body)) {
        // First pass: Scan for manual free() calls
        scanForManualFree(body);

        // Second pass: Transform compound statement
        transformCompoundStmt(body);
    }

    memory_mgr_.exitScope(std::vector<std::string>{});  // Exit function scope
}

void AutoFreePass::transformCompoundStmt(ASTNode* stmt) {
    if (!stmt || !isCompoundStmt(stmt)) return;

    memory_mgr_.enterScope();

    // First pass: Process declarations and nested statements
    std::vector<size_t> return_indices;    // Track positions of return statements
    std::vector<size_t> reassign_indices;  // Track owner-pointer reassignment positions

    for (size_t i = 0; i < stmt->children.size(); ++i) {
        auto& child = stmt->children[i];

        // Check for declarations
        if (isDeclaration(child.get())) {
            processDeclaration(child.get());
        }

        // Track early return statements for later cleanup insertion
        if (isReturnStmt(child.get())) {
            return_indices.push_back(i);
        }

        // Track owner-pointer reassignment: p = malloc(...) where p is already Owner
        // This detects leaks when the old allocation is overwritten without freeing it.
        if (isOwnerReassignment(child.get())) {
            reassign_indices.push_back(i);
        }

        // Recursively process nested compound statements
        if (isCompoundStmt(child.get())) {
            transformCompoundStmt(child.get());
        }

        // Recursively process if statements, loops, etc.
        // (They may contain nested compound statements)
        for (auto& nested : child->children) {
            if (isCompoundStmt(nested.get())) {
                transformCompoundStmt(nested.get());
            }
        }
    }

    // Second pass A: Insert free() BEFORE owner-pointer reassignments (reverse order)
    // This prevents leaks like: p = malloc(100); p = malloc(200);  // old 100-byte block leaked
    for (auto it = reassign_indices.rbegin(); it != reassign_indices.rend(); ++it) {
        insertFreeBeforeReassign(stmt, *it);
    }

    // Second pass B: Recompute return indices (reassign insertions may shift them)
    // then insert cleanup BEFORE return statements
    return_indices.clear();
    for (size_t i = 0; i < stmt->children.size(); ++i) {
        if (isReturnStmt(stmt->children[i].get())) {
            return_indices.push_back(i);
        }
    }
    for (auto it = return_indices.rbegin(); it != return_indices.rend(); ++it) {
        size_t return_idx = *it;
        insertCleanupBeforeReturn(stmt, return_idx);
    }

    // Insert cleanup code at scope exit (before closing brace)
    insertCleanupAtScopeExit(stmt);

    std::vector<std::string> cleanup_list;
    memory_mgr_.exitScope(cleanup_list);
}

void AutoFreePass::processDeclaration(ASTNode* decl) {
    if (!decl) return;

    // Extract variable name from declaration
    std::string var_name = extractVarName(decl);
    if (var_name.empty()) return;

    // CRITICAL FIX: Do NOT use ctx_.getVarType() to determine if this variable needs
    // auto-free. The SafetyContext uses a FLAT namespace (not per-function scoped), so
    // a variable named "buf" in process_loop (Owner: int* buf = malloc(...)) would
    // pollute the classification of "buf" in main (stack array: char buf[32]).
    //
    // Instead, we determine ownership DIRECTLY from the declaration's initializer
    // in the AST. Only variables declared with a malloc/calloc/aligned_alloc initializer
    // in THIS declaration node are tracked for auto-free.

    bool has_alloc_init = false;
    for (auto& child : decl->children) {
        // DeclList -> InitDeclarator -> [1] is initializer
        if (child->kind == NodeKind::DeclList) {
            for (auto& init_decl : child->children) {
                if (init_decl->kind == NodeKind::InitDeclarator &&
                    init_decl->children.size() >= 2) {
                    ASTNode* init_expr = init_decl->children[1].get();
                    if (isAllocationExpr(init_expr)) {
                        has_alloc_init = true;
                    }
                }
            }
        } else if (child->kind == NodeKind::InitDeclarator &&
                   child->children.size() >= 2) {
            ASTNode* init_expr = child->children[1].get();
            if (isAllocationExpr(init_expr)) {
                has_alloc_init = true;
            }
        }
    }

    if (has_alloc_init) {
        // This declaration is: int* p = malloc(...);  -- genuine heap allocation.
        // Track it as Owner for auto-free at scope exit.
        memory_mgr_.trackVariable(var_name, PointerKind::Owner, decl->line);
        function_owner_vars_.insert(var_name);
        initialized_owner_vars_.insert(var_name);
    }
    // else: no malloc initializer -> not tracked, not auto-freed
    // (covers: int* p;  char buf[32];  int* p = NULL;  int* p = other_func();)
    // Variables later assigned malloc are tracked via B5 insertFreeBeforeReassign.
}

void AutoFreePass::scanForManualFree(ASTNode* node) {
    if (!node) return;

    // Check if this is a free() call
    if (isFreeCall(node)) {
        std::string arg = extractFreeArg(node);
        if (!arg.empty()) {
            manually_freed_.insert(arg);
            // Mark as manually freed in memory manager
            memory_mgr_.markFreed(arg);
        }
    }

    // Recursively scan children
    for (auto& child : node->children) {
        scanForManualFree(child.get());
    }
}

void AutoFreePass::insertCleanupAtScopeExit(ASTNode* scope) {
    if (!scope || !isCompoundStmt(scope)) return;

    std::vector<std::string> cleanup_list;

    // Get list of variables that need cleanup at this scope
    memory_mgr_.getCleanupListForCurrentScope(cleanup_list);

    // Insert free() calls in reverse order (LIFO - last allocated, first freed)
    std::reverse(cleanup_list.begin(), cleanup_list.end());

    for (const std::string& var_name : cleanup_list) {
        // Skip variables that were already freed explicitly in this function.
        // scanForManualFree populates manually_freed_ before trackVariable is called,
        // so memory_mgr_.markFreed() has no effect (variable not yet in scoped_vars_).
        // manually_freed_ is the authoritative guard against double-free here.
        if (manually_freed_.count(var_name)) continue;

        ASTNode* free_stmt = nullptr;

        if (ctx_.shouldInsertRuntimeGuards()) {
            // Full safety: Use guarded free with runtime checks
            free_stmt = createGuardedFreeCall(var_name, scope->line);
        } else {
            // Medium safety: Simple null check + free
            ASTNode* null_check = createNullCheck(var_name, scope->line);
            ASTNode* free_call = createFreeCall(var_name, scope->line);
            free_stmt = createIfStatement(null_check, free_call, scope->line);
        }

        // Add cleanup statement to end of scope
        scope->add(std::move(std::unique_ptr<ASTNode>(free_stmt)));
    }
}

void AutoFreePass::insertCleanupBeforeReturn(ASTNode* parent_scope, size_t return_index) {
    if (!parent_scope || !isCompoundStmt(parent_scope)) return;
    if (return_index >= parent_scope->children.size()) return;

    ASTNode* return_stmt = parent_scope->children[return_index].get();
    if (!isReturnStmt(return_stmt)) return;

    // Check for ownership transfer: if return expression is an OWNER variable,
    // the ownership is transferred to the caller - do NOT free it here.
    // Example: int* p = malloc(100); return p;  <- p must NOT be freed!
    if (!return_stmt->children.empty()) {
        ASTNode* return_expr = return_stmt->children[0].get();
        if (return_expr && return_expr->kind == NodeKind::Ident) {
            SafeTypeInfo type_info = ctx_.getVarType(return_expr->sval);
            if (type_info.ptr_kind == PointerKind::Owner) {
                // Mark as transferred - prevents free at scope exit too
                memory_mgr_.markTransferred(return_expr->sval);
            }
        }
    }

    std::vector<std::string> cleanup_list;

    // Get list of ALL variables that need cleanup from ALL scopes (current + parents)
    // This is CRITICAL for early returns in nested scopes!
    // Note: transferred variables are excluded by markTransferred() above.
    memory_mgr_.getCleanupListForAllScopes(cleanup_list);

    if (cleanup_list.empty()) {
        return;  // Nothing to clean up
    }

    // Insert free() calls in reverse order (LIFO)
    std::reverse(cleanup_list.begin(), cleanup_list.end());

    // Insert cleanup statements into parent's children list BEFORE the return statement
    for (const std::string& var_name : cleanup_list) {
        // Skip variables already freed explicitly (same guard as insertCleanupAtScopeExit).
        if (manually_freed_.count(var_name)) continue;

        ASTNode* free_stmt = nullptr;

        if (ctx_.shouldInsertRuntimeGuards()) {
            // Full safety: Use guarded free with runtime checks
            free_stmt = createGuardedFreeCall(var_name, return_stmt->line);
        } else {
            // Medium safety: Simple null check + free
            ASTNode* null_check = createNullCheck(var_name, return_stmt->line);
            ASTNode* free_call = createFreeCall(var_name, return_stmt->line);
            free_stmt = createIfStatement(null_check, free_call, return_stmt->line);
        }

        // Insert cleanup statement at return_index position (pushes return statement forward)
        parent_scope->children.insert(
            parent_scope->children.begin() + return_index,
            std::unique_ptr<ASTNode>(free_stmt)
        );
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// AST Node Creation Helpers
// ═══════════════════════════════════════════════════════════════════════════════

ASTNode* AutoFreePass::createFreeCall(const std::string& var_name, int line) {
    // Create: free(var_name);

    // CallExpr node
    auto call = ASTNode::make(NodeKind::CallExpr, line, 0);

    // Function name: "free"
    auto func_name = ASTNode::make(NodeKind::Ident, line, 0);
    func_name->sval = "free";
    call->add(std::move(func_name));

    // Argument: variable name
    auto arg_list = ASTNode::make(NodeKind::ArgList, line, 0);
    auto arg = ASTNode::make(NodeKind::Ident, line, 0);
    arg->sval = var_name;
    arg_list->add(std::move(arg));
    call->add(std::move(arg_list));

    // Wrap in ExprStmt
    auto stmt = ASTNode::make(NodeKind::ExprStmt, line, 0);
    stmt->add(std::move(call));

    return stmt.release();
}

ASTNode* AutoFreePass::createNullCheck(const std::string& var_name, int line) {
    // Create: var_name != NULL

    auto binary = ASTNode::make(NodeKind::BinaryExpr, line, 0);
    binary->sval = "!=";

    // Left: variable name
    auto var = ASTNode::make(NodeKind::Ident, line, 0);
    var->sval = var_name;
    binary->add(std::move(var));

    // Right: NULL (integer 0)
    auto null_val = ASTNode::make(NodeKind::IntLit, line, 0);
    null_val->ival = 0;
    binary->add(std::move(null_val));

    return binary.release();
}

ASTNode* AutoFreePass::createIfStatement(ASTNode* condition, ASTNode* body, int line) {
    // Create: if (condition) { body; }

    auto if_stmt = ASTNode::make(NodeKind::IfStmt, line, 0);

    // Condition
    if_stmt->add(std::unique_ptr<ASTNode>(condition));

    // Body (wrap in compound statement if not already)
    auto compound = ASTNode::make(NodeKind::CompoundStmt, line, 0);
    compound->add(std::unique_ptr<ASTNode>(body));
    if_stmt->add(std::move(compound));

    return if_stmt.release();
}

ASTNode* AutoFreePass::createGuardedFreeCall(const std::string& var_name, int line) {
    // Create: __guarded_free(var_name, __FILE__, __LINE__);

    auto call = ASTNode::make(NodeKind::CallExpr, line, 0);

    // Function name: "__guarded_free"
    auto func_name = ASTNode::make(NodeKind::Ident, line, 0);
    func_name->sval = "__guarded_free";
    call->add(std::move(func_name));

    // Arguments: (ptr, __FILE__, __LINE__)
    auto arg_list = ASTNode::make(NodeKind::ArgList, line, 0);

    // Arg 1: pointer variable
    auto arg1 = ASTNode::make(NodeKind::Ident, line, 0);
    arg1->sval = var_name;
    arg_list->add(std::move(arg1));

    // Arg 2: __FILE__ (string literal)
    auto arg2 = ASTNode::make(NodeKind::Ident, line, 0);
    arg2->sval = "__FILE__";
    arg_list->add(std::move(arg2));

    // Arg 3: __LINE__ (integer - use declaration line)
    auto arg3 = ASTNode::make(NodeKind::IntLit, line, 0);
    arg3->ival = line;
    arg_list->add(std::move(arg3));

    call->add(std::move(arg_list));

    // Wrap in ExprStmt
    auto stmt = ASTNode::make(NodeKind::ExprStmt, line, 0);
    stmt->add(std::move(call));

    return stmt.release();
}

ASTNode* AutoFreePass::createCompoundStmt(int line) {
    return ASTNode::make(NodeKind::CompoundStmt, line, 0).release();
}

// ═══════════════════════════════════════════════════════════════════════════════
// AST Query Helpers
// ═══════════════════════════════════════════════════════════════════════════════

bool AutoFreePass::isFunctionDef(ASTNode* node) const {
    return node && node->kind == NodeKind::FunctionDef;
}

bool AutoFreePass::isCompoundStmt(ASTNode* node) const {
    return node && node->kind == NodeKind::CompoundStmt;
}

bool AutoFreePass::isDeclaration(ASTNode* node) const {
    return node && node->kind == NodeKind::Declaration;
}

bool AutoFreePass::isFreeCall(ASTNode* node) const {
    if (!node || node->kind != NodeKind::CallExpr) return false;
    if (node->children.empty()) return false;

    ASTNode* func = node->children[0].get();
    return func && func->kind == NodeKind::Ident && func->sval == "free";
}

bool AutoFreePass::isReturnStmt(ASTNode* node) const {
    return node && node->kind == NodeKind::ReturnStmt;
}

// ═══════════════════════════════════════════════════════════════════════════════
// AST Extraction Helpers
// ═══════════════════════════════════════════════════════════════════════════════

std::string AutoFreePass::extractVarName(ASTNode* decl) const {
    if (!decl || !isDeclaration(decl)) return "";

    // Declaration structure:
    // Declaration -> [0]=DeclSpecs, [1]=DeclList
    // DeclList -> children are InitDeclarator nodes
    // InitDeclarator -> [0]=Declarator, [1]=initializer (optional)
    // Declarator -> [0]=Pointer or DirectDeclarator, [1]=DirectDeclarator (if Pointer)
    // DirectDeclarator or Declarator -> sval contains variable name

    for (auto& child : decl->children) {
        // DeclList containing InitDeclarator nodes
        if (child->kind == NodeKind::DeclList) {
            for (auto& init_decl : child->children) {
                if (init_decl->kind == NodeKind::InitDeclarator) {
                    if (init_decl->children.size() >= 1) {
                        ASTNode* declarator = init_decl->children[0].get();

                        if (declarator && declarator->kind == NodeKind::Declarator &&
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

                            // Traverse nested Declarators to find name in sval
                            while (current) {
                                if (!current->sval.empty() && current->sval != "*") {
                                    return current->sval;
                                }

                                if (current->kind == NodeKind::DirectDeclarator) {
                                    if (current->children.size() > 0 &&
                                        current->children[0]->kind == NodeKind::Ident) {
                                        return current->children[0]->sval;
                                    }
                                    break;
                                } else if (current->kind == NodeKind::Declarator) {
                                    // Nested Declarator - traverse into it
                                    if (current->children.size() > 0) {
                                        if (current->children[0]->kind == NodeKind::Pointer) {
                                            current = (current->children.size() > 1)
                                                      ? current->children[1].get() : nullptr;
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
                    }
                }
            }
        }
        // Direct InitDeclarator (in case of single declarator)
        else if (child->kind == NodeKind::InitDeclarator) {
            if (child->children.size() >= 1) {
                ASTNode* declarator = child->children[0].get();

                if (declarator && declarator->kind == NodeKind::Declarator &&
                    declarator->children.size() > 0) {

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
                            return direct->children[0]->sval;
                        }
                    }
                }
            }
        }
    }

    return "";
}

std::string AutoFreePass::extractFreeArg(ASTNode* call) const {
    if (!isFreeCall(call)) return "";
    if (call->children.size() < 2) return "";

    // CallExpr structure: [0]=function_name, [1]=ArgList
    ASTNode* arg_list = call->children[1].get();
    if (!arg_list || arg_list->children.empty()) return "";

    // First argument to free()
    ASTNode* arg = arg_list->children[0].get();
    if (arg && arg->kind == NodeKind::Ident) {
        return arg->sval;
    }

    return "";
}

ASTNode* AutoFreePass::getFunctionBody(ASTNode* func) const {
    if (!isFunctionDef(func) || func->children.size() < 3) return nullptr;
    return func->children[2].get();
}

// ═══════════════════════════════════════════════════════════════════════════════
// B5: Pointer Reassignment Leak Detection
// ═══════════════════════════════════════════════════════════════════════════════

bool AutoFreePass::isAllocationExpr(ASTNode* expr) const {
    if (!expr) return false;
    // Look through cast: (int*)malloc(...)
    ASTNode* actual = expr;
    if (expr->kind == NodeKind::CastExpr && expr->children.size() > 1)
        actual = expr->children[1].get();
    if (!actual || actual->kind != NodeKind::CallExpr) return false;
    if (actual->children.empty()) return false;
    ASTNode* callee = actual->children[0].get();
    if (!callee || callee->kind != NodeKind::Ident) return false;
    const std::string& n = callee->sval;
    // NOTE: realloc is intentionally excluded — it frees the old pointer internally.
    // Inserting free() before p = realloc(p, ...) would cause a double-free.
    return (n == "malloc" || n == "calloc" || n == "aligned_alloc" ||
            n == "strdup" || n == "strndup");
}

bool AutoFreePass::isOwnerReassignment(ASTNode* stmt) const {
    // ExprStmt -> AssignExpr / BinaryExpr("=") where:
    //   LHS is a tracked Owner variable AND RHS is a new malloc-like call
    if (!stmt || stmt->kind != NodeKind::ExprStmt) return false;
    if (stmt->children.empty()) return false;
    ASTNode* expr = stmt->children[0].get();
    if (!expr) return false;
    if (expr->kind != NodeKind::AssignExpr &&
        !(expr->kind == NodeKind::BinaryExpr && expr->sval == "=")) return false;
    if (expr->children.size() < 2) return false;

    ASTNode* lhs = expr->children[0].get();
    ASTNode* rhs = expr->children[1].get();
    if (!lhs || lhs->kind != NodeKind::Ident) return false;

    // LHS must be an Owner declared in THIS function AND must have been initialized.
    // function_owner_vars_: declared in this function (prevents cross-function collisions).
    // initialized_owner_vars_: has received a malloc value before (prevents free of garbage).
    // "int* p;" declares p as Owner but does NOT initialize it; p = malloc(...) inside an
    // if-branch is the FIRST assignment, not a REASSIGNMENT - no pre-free should fire.
    if (!function_owner_vars_.count(lhs->sval)) return false;
    if (!initialized_owner_vars_.count(lhs->sval)) return false;  // Not yet initialized
    if (manually_freed_.count(lhs->sval)) return false;  // already freed manually
    SafeTypeInfo lhs_type = ctx_.getVarType(lhs->sval);
    if (lhs_type.ptr_kind != PointerKind::Owner) return false;

    // RHS must be a new allocation
    return isAllocationExpr(rhs);
}

void AutoFreePass::insertFreeBeforeReassign(ASTNode* scope, size_t stmt_index) {
    if (!scope || stmt_index >= scope->children.size()) return;

    ASTNode* assign_stmt = scope->children[stmt_index].get();
    if (!assign_stmt || assign_stmt->kind != NodeKind::ExprStmt) return;
    if (assign_stmt->children.empty()) return;

    ASTNode* expr = assign_stmt->children[0].get();
    if (!expr || expr->children.empty()) return;

    ASTNode* lhs = expr->children[0].get();
    if (!lhs || lhs->kind != NodeKind::Ident) return;

    const std::string& var_name = lhs->sval;
    int line = assign_stmt->line;

    // Insert: if (var_name) free(var_name);  before the assignment
    ASTNode* free_stmt = nullptr;
    if (ctx_.shouldInsertRuntimeGuards()) {
        free_stmt = createGuardedFreeCall(var_name, line);
    } else {
        ASTNode* null_check = createNullCheck(var_name, line);
        ASTNode* free_call  = createFreeCall(var_name, line);
        free_stmt = createIfStatement(null_check, free_call, line);
    }

    scope->children.insert(
        scope->children.begin() + stmt_index,
        std::unique_ptr<ASTNode>(free_stmt)
    );
    // NOTE: Do NOT call memory_mgr_.markFreed() here.
    // After this free, the variable is immediately reassigned to a new malloc().
    // Scope-exit cleanup should still free that new allocation.
    // markFreed would prevent that cleanup and leak the new allocation.

    // Mark as initialized: the variable now has a live allocation (the new malloc).
    // Future reassignments to this variable are true reassignments that need pre-free.
    initialized_owner_vars_.insert(var_name);
}
