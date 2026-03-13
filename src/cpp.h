#pragma once
#include <string>
#include <vector>
#include <map>
#include <set>

// P4-D: Standard level for enforcement warnings. Set by main.cpp based on -std=.
// 89=C89, 99=C99, 11=C11, 17=C17, 23=C23. Default 17.
extern int g_std_level;

// P4-B: ANSI colour diagnostics flag (defined in main.cpp; used by codegen.cpp).
extern bool g_colour;

// C23 typed enum underlying size table: enum tag → byte size.
// Populated by Preprocessor when it strips ': TYPE' from typed enum declarations.
// Consulted by CodeGen::resolveType for sizeof/alignof of typed enums.
extern std::map<std::string, int> g_enum_underlying_sizes;

// 5.1: #pragma pack level — 0=default, 1=packed (no padding), >1 = specific alignment.
// Set by Preprocessor when it processes #pragma pack(N) directives.
// Consulted by CodeGen struct layout to skip natural alignment padding.
extern int g_pack_level;

struct PPMacro {
    std::string name;
    bool is_function;           // function-like vs object-like
    std::vector<std::string> params;  // parameter names
    bool is_variadic;
    std::string body;           // replacement text
    bool is_predefined;         // __FILE__, __LINE__, etc.
};

class Preprocessor {
public:
    Preprocessor();

    // Add include search paths
    void addIncludePath(const std::string& path);
    void addSystemIncludePath(const std::string& path);

    // Define/undefine macros from command line
    void defineMacro(const std::string& name, const std::string& value = "1");
    void undefineMacro(const std::string& name);

    // Main entry: preprocess a file and return the result
    std::string preprocess(const char* source, const char* filename);

private:
    // Macro table
    std::map<std::string, PPMacro> macros_;

    // Include paths
    std::vector<std::string> user_include_paths_;
    std::vector<std::string> sys_include_paths_;

    // Conditional compilation stack
    struct CondState {
        bool active;        // is this branch currently emitting?
        bool has_been_true; // has any branch been true?
        bool in_else;       // have we seen #else?
    };
    std::vector<CondState> cond_stack_;

    // Include depth tracking
    int include_depth_;
    static const int MAX_INCLUDE_DEPTH = 200;

    // Current file info
    std::string current_file_;
    int current_line_;

    // Multi-line comment tracking
    bool in_block_comment_;

    // #pragma once: set of canonical file paths already included with #pragma once
    std::set<std::string> pragma_once_files_;

    // _Generic variable type tracking: varname → C type keyword ("int","double",etc.)
    // Populated as simple-type declarations are processed; used to improve _Generic dispatch.
    std::map<std::string, std::string> var_types_;

    // C99/C23: typeof decay tracking: set of variable names declared as arrays (VAR[]).
    // When typeof(arr) is evaluated, array types decay to pointer types (int → int *).
    std::set<std::string> var_array_names_;

    // Processing
    std::string processSource(const char* src, const char* filename);
    void processLine(const std::string& line, std::string& output);
    void processDirective(const std::string& line, std::string& output);
    std::string expandMacros(const std::string& text, std::set<std::string>& expanding);

    // Directive handlers
    void handleDefine(const std::string& rest);
    void handleUndef(const std::string& rest);
    void handleInclude(const std::string& rest, std::string& output);
    void handleIf(const std::string& rest);
    void handleIfdef(const std::string& rest, bool negate);
    void handleElif(const std::string& rest);
    void handleElifdef(const std::string& rest, bool negate); // C23
    void handleElse();
    void handleEndif();
    void handleError(const std::string& rest);
    void handleWarning(const std::string& rest); // C23
    void handleLine(const std::string& rest);
    void handleEmbed(const std::string& rest, std::string& output); // W23: C23 #embed

    // #if expression evaluator
    long long evalExpr(const std::string& expr);
    long long evalExprImpl(const char*& p, int min_prec);
    long long evalPrimary(const char*& p);
    void skipSpaces(const char*& p);
    int getPrecedence(const char* p, std::string& op);

    // Macro expansion helpers
    std::string expandObjectMacro(const PPMacro& m, std::set<std::string>& expanding);
    std::string expandFunctionMacro(const PPMacro& m, const std::vector<std::string>& args, std::set<std::string>& expanding);
    std::vector<std::string> collectMacroArgs(const std::string& text, size_t& pos, int param_count);
    std::string stringify(const std::string& s);
    std::string pasteTokens(const std::string& left, const std::string& right);

    // Include helpers
    std::string findIncludeFile(const std::string& name, bool is_system);
    std::string readFile(const std::string& path);

    // Utility
    bool isActive();
    std::string trim(const std::string& s);
    bool isIdentChar(char c);
    bool isIdentStart(char c);
};
