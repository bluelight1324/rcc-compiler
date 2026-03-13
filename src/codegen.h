#pragma once
#include "ast.h"
#include <string>
#include <vector>
#include <map>
#include <set>
#include <tuple>
#include <cstdio>

// Simple type system
enum class CType { Void, Int, Char, Long, Short, Float, Double, Ptr, Array, Struct };

struct TypeInfo {
    CType base;
    int size;       // in bytes
    bool is_unsigned;
    TypeInfo* pointee; // for pointers
    std::string struct_name;
    std::vector<int> array_dims;  // Multi-dim array dimensions, e.g. [3][4] => {3,4}

    TypeInfo() : base(CType::Int), size(4), is_unsigned(false), pointee(nullptr) {}
    static TypeInfo make_int() { TypeInfo t; t.base = CType::Int; t.size = 4; return t; }
    static TypeInfo make_char() { TypeInfo t; t.base = CType::Char; t.size = 1; return t; }
    static TypeInfo make_void() { TypeInfo t; t.base = CType::Void; t.size = 0; return t; }
    static TypeInfo make_long() { TypeInfo t; t.base = CType::Long; t.size = 8; return t; }
    static TypeInfo make_ptr() { TypeInfo t; t.base = CType::Ptr; t.size = 8; return t; }
    static TypeInfo make_float() { TypeInfo t; t.base = CType::Float; t.size = 4; return t; }
    static TypeInfo make_double() { TypeInfo t; t.base = CType::Double; t.size = 8; return t; }
    static TypeInfo make_struct(const std::string& name, int size) {
        TypeInfo t; t.base = CType::Struct; t.size = size; t.struct_name = name; return t;
    }
};

struct VarInfo {
    TypeInfo type;
    int stack_offset; // offset from RBP (negative)
    bool is_global;
    std::string global_label;
    bool has_init = false;
    long long init_value = 0;
    std::vector<long long> init_values; // for array/struct initializers
    std::vector<std::string> init_fn_names; // C99: per-element function names ("" = not a fn ptr)
    std::string init_fn_scalar;         // C99: function name for scalar fn-ptr init (empty = value)
    std::string init_str_label;         // C11: label of string literal init (global ptr to string)
    int align_req = 8;                  // C11: _Alignas alignment requirement (default 8)
    bool is_tls = false;                // C11: _Thread_local — thread-local storage
};

struct StructMemberInfo {
    std::string name;
    TypeInfo type;
    int offset;      // byte offset of storage unit from struct base
    int bit_offset;  // bit offset within storage unit (0 for non-bitfield)
    int bit_width;   // 0 = not a bit-field; >0 = bit-field width in bits
    StructMemberInfo() : offset(0), bit_offset(0), bit_width(0) {}
};

struct StructLayout {
    std::string name;
    std::vector<StructMemberInfo> members;
    int total_size;
    bool is_packed = false;  // 5.1: #pragma pack(1) or __attribute__((packed))
};

struct FuncInfo {
    TypeInfo return_type;
    std::vector<TypeInfo> param_types;
    std::vector<std::string> param_names;
    bool is_variadic          = false;  // C99: function has ... parameter
    bool is_nodiscard         = false;  // C23: [[nodiscard]] — warn if return discarded
    bool is_deprecated        = false;  // C23: [[deprecated]] — warn on any use
    bool is_noreturn          = false;  // C23: [[noreturn]] — function must not return
    bool returns_large_struct = false;  // 1.1: return type is struct > 16 bytes (hidden ptr ABI)
    std::string deprecated_msg;         // optional deprecation message
    int def_line = 0;                   // 4.2: source line of definition (for unused-fn warning)
};

class CodeGen {
public:
    CodeGen(FILE* out, const char* source_file = "<input>");
    void generate(ASTNode* root);
    void setDebugInfo(bool on) { debug_info_ = on; }  // P4-A: -g flag

private:
    FILE* out_;
    std::string source_file_;  // source filename for diagnostics
    int label_count_;
    int string_count_;

    // Symbol tables
    std::map<std::string, VarInfo> globals_;
    std::map<std::string, VarInfo> locals_;
    std::map<std::string, FuncInfo> functions_;
    std::vector<std::tuple<std::string, std::string, int>> string_literals_; // label, value, char_width
    std::set<std::string> called_functions_; // functions called (for EXTERN)
    std::set<std::string> static_once_flags_; // P2.3: labels for static-local once guards
    std::map<std::string, StructLayout> structs_; // struct name -> layout
    std::map<std::string, long long> enum_constants_; // enum constant -> value
    std::map<std::string, TypeInfo> typedefs_; // typedef name -> type
    std::map<std::string, long long> constexpr_values_; // C23: constexpr integer variable -> value
    std::map<std::string, double> constexpr_float_values_; // C23: constexpr float/double variable -> value
    std::map<std::string, std::vector<long long>> constexpr_array_values_; // C23: constexpr int array -> elements
    std::map<std::string, std::map<std::string, long long>> constexpr_struct_values_; // C23: constexpr struct -> {member->value}
    std::map<std::string, long long> const_locals_;     // N3: non-constexpr locals w/ constant init
    std::vector<std::pair<std::string, float>> float_literals_; // label, value (REAL4)
    std::vector<std::pair<std::string, double>> double_literals_; // label, value (REAL8)
    int float_count_;
    int double_count_;

    // W6: Unused-variable tracking
    std::set<std::string> locals_used_;         // locals whose value was read
    std::set<std::string> locals_maybe_unused_; // locals with [[maybe_unused]]
    std::set<std::string> param_names_set_;     // current function's parameter names
    std::map<std::string, int> locals_decl_line_; // declaration line per local

    int local_offset_; // current stack frame size
    int max_align_req_; // C11: maximum _Alignas requirement seen in current function
    std::string current_func_;
    int break_label_;
    int continue_label_;
    bool last_expr_is_ptr_; // track if last expr was pointer/array for arithmetic scaling
    int last_pointee_size_; // element size for pointer indexing (1=char, 4=int, 8=ptr/long)
    int last_lvalue_size_; // element size of the lvalue target (for correct store width)
    bool last_lvalue_is_float_; // track if lvalue is float type
    bool last_expr_is_float_; // track if last expr result is in xmm0
    int  last_float_size_;   // 4=float(ss), 8=double(sd) — valid when last_expr_is_float_
    bool last_expr_is_unsigned_; // track if last expr is unsigned
    int  push_depth_;            // 2.2: active stack pushes from genBinary (for alignment)
    int  last_expr_struct_size_; // 1.1: size of struct returned by last call (0=not struct; 1-8=RAX; 9-16=RAX:RDX)
    int  hidden_ret_ptr_offset_; // 1.1: stack slot of saved hidden return pointer for >16B struct return
    std::set<std::string> static_functions_; // 4.2: static-linkage function names (for unused-fn warning)
    TypeInfo last_typeof_type_;  // C23: type from last typeof() expression
    bool has_typeof_type_;       // whether last_typeof_type_ is valid

    // Bit-field tracking for member access
    bool last_lvalue_is_bitfield_;
    int last_lvalue_bit_offset_;
    int last_lvalue_bit_width_;

    // P4-A: debug info (-g flag)
    bool debug_info_ = false;    // emit "; <file>:<line>" comments before statements
    int  last_debug_line_ = -1;  // avoid duplicate line markers

    // Helpers
    int getExprPointeeSize(ASTNode* node);
    const char* ptrSizeDirective(int elem_size);
    int ptrSizeShift(int elem_size);
    void emitPtrScale(const char* reg, int elem_size); // 1.4: scale reg by stride (shl or imul)
    TypeInfo getParamType(const std::string& fname, int idx); // 2.1: callee param type lookup
    double evalConstExprDouble(ASTNode* node); // C23: evaluate float constexpr at compile time
    int newLabel() { return label_count_++; }
    void emit(const char* fmt, ...);
    void emitLabel(int label);
    TypeInfo resolveType(ASTNode* typeNode);
    std::string getDeclaratorName(ASTNode* decl);
    bool isDeclaratorFunc(ASTNode* decl);
    int allocLocalAligned(int size, int align); // C11: _Alignas-aware allocation
    std::string addStringLiteral(const std::string& val, int char_width); // register string lit

    // Code generation
    void genTranslationUnit(ASTNode* node);
    void genFunctionDef(ASTNode* node);
    void genDeclaration(ASTNode* node, bool global);
    void genStatement(ASTNode* node);
    void genCompoundStmt(ASTNode* node);
    void genIfStmt(ASTNode* node);
    void genWhileStmt(ASTNode* node);
    void genForStmt(ASTNode* node);
    void genSwitchStmt(ASTNode* node);
    void genSwitchBody(ASTNode* node);
    void collectCaseLabels(ASTNode* node, std::vector<std::pair<ASTNode*, int>>& cases, int& defaultLabel);
    std::map<std::string, VarInfo>::iterator findGlobal(const std::string& name);
    void genReturnStmt(ASTNode* node);
    void genExprStmt(ASTNode* node);

    // Expression codegen - result in RAX
    void genExpr(ASTNode* node);
    void genAssign(ASTNode* node);
    void genBinary(ASTNode* node);
    void genUnary(ASTNode* node);
    void genCall(ASTNode* node);
    void genSubscript(ASTNode* node);
    void genMember(ASTNode* node);
    int findMemberOffset(const std::string& member_name);
    void genIdent(ASTNode* node);
    void genIntLit(ASTNode* node);
    void genStrLit(ASTNode* node);
    void genFloatLit(ASTNode* node);
    void genCast(ASTNode* node);
    long long evalConstExpr(ASTNode* node);
    bool isConstExpr(ASTNode* node);
    TypeInfo getExprType(ASTNode* node);
    TypeInfo inferTypeFromExpr(ASTNode* node);  // C23: auto type inference

    // Address generation - result in RAX
    void genLValue(ASTNode* node);
    int allocLocal(int size);
};
