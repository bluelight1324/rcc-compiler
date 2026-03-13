#include "auto_memory.h"
#include <algorithm>

AutoMemoryManager::AutoMemoryManager(SafetyContext& ctx)
    : ctx_(ctx), current_scope_depth_(0), cleanup_counter_(0) {
}

void AutoMemoryManager::enterScope() {
    current_scope_depth_++;
}

void AutoMemoryManager::exitScope(std::vector<std::string>& cleanup_list) {
    // Find all OWNER variables at this scope depth that need cleanup
    for (auto& var : scoped_vars_) {
        if (var.scope_depth == current_scope_depth_ &&
            var.ownership == PointerKind::Owner &&
            var.needs_cleanup &&
            !var.manually_freed) {

            cleanup_list.push_back(var.name);
        }
    }

    // Remove variables from this scope
    scoped_vars_.erase(
        std::remove_if(scoped_vars_.begin(), scoped_vars_.end(),
            [this](const ScopedVariable& v) {
                return v.scope_depth == current_scope_depth_;
            }),
        scoped_vars_.end()
    );

    current_scope_depth_--;
}

void AutoMemoryManager::trackVariable(const std::string& name,
                                      PointerKind ownership,
                                      int line) {
    scoped_vars_.push_back({
        name,
        ownership,
        current_scope_depth_,
        line,
        true,   // needs_cleanup by default
        false   // not manually freed yet
    });
}

void AutoMemoryManager::markTransferred(const std::string& name) {
    ScopedVariable* var = findVariable(name);
    if (var) {
        var->needs_cleanup = false;  // Don't auto-free if ownership transferred
    }
}

void AutoMemoryManager::markFreed(const std::string& name) {
    ScopedVariable* var = findVariable(name);
    if (var) {
        var->manually_freed = true;  // Don't auto-free if already freed manually
    }
}

bool AutoMemoryManager::needsCleanup(const std::string& name) const {
    for (const auto& var : scoped_vars_) {
        if (var.name == name) {
            return var.needs_cleanup && !var.manually_freed;
        }
    }
    return false;
}

void AutoMemoryManager::getCleanupListForCurrentScope(std::vector<std::string>& cleanup_list) const {
    // Get list of variables that need cleanup at current scope (without modifying state)
    for (const auto& var : scoped_vars_) {
        if (var.scope_depth == current_scope_depth_ &&
            var.ownership == PointerKind::Owner &&
            var.needs_cleanup &&
            !var.manually_freed) {
            cleanup_list.push_back(var.name);
        }
    }
}

void AutoMemoryManager::getCleanupListForAllScopes(std::vector<std::string>& cleanup_list) const {
    // Get list of ALL variables that need cleanup from current scope AND all parent scopes
    // Used for early returns that need to clean up everything before returning
    for (const auto& var : scoped_vars_) {
        if (var.scope_depth <= current_scope_depth_ &&
            var.ownership == PointerKind::Owner &&
            var.needs_cleanup &&
            !var.manually_freed) {
            cleanup_list.push_back(var.name);
        }
    }
}

AutoMemoryManager::ScopedVariable* AutoMemoryManager::findVariable(const std::string& name) {
    for (auto& var : scoped_vars_) {
        if (var.name == name) {
            return &var;
        }
    }
    return nullptr;
}
