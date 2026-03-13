#ifndef AUTO_MEMORY_H
#define AUTO_MEMORY_H

#include <string>
#include <vector>
#include <map>
#include "safety.h"

// AutoMemoryManager: Manages automatic memory cleanup at scope exit
// Part of RCC v4.1+ Hybrid Memory Management System
class AutoMemoryManager {
public:
    AutoMemoryManager(SafetyContext& ctx);

    // Scope management
    void enterScope();
    void exitScope(std::vector<std::string>& cleanup_list);

    // Variable tracking
    void trackVariable(const std::string& name, PointerKind ownership, int line);
    void markTransferred(const std::string& name);
    void markFreed(const std::string& name);

    // Query methods
    bool needsCleanup(const std::string& name) const;
    int getCurrentScopeDepth() const { return current_scope_depth_; }
    void getCleanupListForCurrentScope(std::vector<std::string>& cleanup_list) const;
    void getCleanupListForAllScopes(std::vector<std::string>& cleanup_list) const;  // For early returns

private:
    struct ScopedVariable {
        std::string name;
        PointerKind ownership;
        int scope_depth;
        int declaration_line;
        bool needs_cleanup;
        bool manually_freed;
    };

    SafetyContext& ctx_;
    std::vector<ScopedVariable> scoped_vars_;
    int current_scope_depth_;
    int cleanup_counter_;  // For unique label generation

    // Helper methods
    ScopedVariable* findVariable(const std::string& name);
};

#endif // AUTO_MEMORY_H
