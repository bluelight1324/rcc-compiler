#include "codegen.h"
#include "cpp.h"
#include "lexer.h"
#include <cstdarg>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <functional>
#ifdef _WIN32
#include <cstdio>
#include <cstdlib>
#endif

// Windows tmpfile() often fails when C: drive is full.
// Use fixed temp file paths on a non-C: drive for reliability.
static int g_tmpfile_counter = 0;
#include <map>
static std::map<FILE*, std::string> g_tmpfile_names;
static FILE* reliable_tmpfile() {
#ifdef _WIN32
    // Use deterministic temp file names on I: drive to avoid C: drive issues
    char name[256];
    snprintf(name, sizeof(name), "I:\\tmp\\rcc_body_%d.tmp", g_tmpfile_counter++);
    FILE* f = fopen(name, "w+b");
    if (f) {
        g_tmpfile_names[f] = name;
        return f;
    }
    // Fallback to current directory
    snprintf(name, sizeof(name), "rcc_body_%d.tmp", g_tmpfile_counter - 1);
    f = fopen(name, "w+b");
    if (f) {
        g_tmpfile_names[f] = name;
        return f;
    }
    return tmpfile(); // last resort
#else
    return tmpfile();
#endif
}
static void cleanup_tmpfile(FILE* f) {
    fclose(f);
    auto it = g_tmpfile_names.find(f);
    if (it != g_tmpfile_names.end()) {
        remove(it->second.c_str());
        g_tmpfile_names.erase(it);
    }
}

// ANSI colour helpers — g_colour extern in cpp.h
static const char* cg_warn()  { return g_colour ? "\033[1;35m" : ""; }
static const char* cg_err()   { return g_colour ? "\033[1;31m" : ""; }
static const char* cg_reset() { return g_colour ? "\033[0m"    : ""; }

CodeGen::CodeGen(FILE* out, const char* source_file)
    : out_(out), source_file_(source_file ? source_file : "<input>"), label_count_(0), string_count_(0),
      local_offset_(0), max_align_req_(8), break_label_(-1), continue_label_(-1), last_expr_is_ptr_(false), last_pointee_size_(8), last_lvalue_size_(8),
      last_lvalue_is_float_(false), last_expr_is_float_(false), last_float_size_(4), last_expr_is_unsigned_(false), push_depth_(0), last_expr_struct_size_(0), float_count_(0), double_count_(0),
      hidden_ret_ptr_offset_(0),
      has_typeof_type_(false),
      last_lvalue_is_bitfield_(false), last_lvalue_bit_offset_(0), last_lvalue_bit_width_(0) {}

// findGlobal: look up a variable name in globals_, checking for static-local scoped
// label (__static_<func>_<name>) before falling back to the bare name key.
// This is needed because static locals are stored under their scoped label to avoid
// collision when multiple functions have same-named static locals.
std::map<std::string, VarInfo>::iterator CodeGen::findGlobal(const std::string& name) {
    if (!current_func_.empty()) {
        auto it = globals_.find("__static_" + current_func_ + "_" + name);
        if (it != globals_.end()) return it;
    }
    return globals_.find(name);
}

// Determine the pointee element size for a pointer expression
// Returns 1 for char*, 4 for int*, 8 for long*/void*/other pointers
int CodeGen::getExprPointeeSize(ASTNode* node) {
    if (!node) return 8;
    if (node->kind == NodeKind::Ident) {
        auto it = locals_.find(node->sval);
        if (it != locals_.end()) {
            if (it->second.type.pointee) return it->second.type.pointee->size;
            return 8;
        }
        auto git = findGlobal(node->sval);
        if (git != globals_.end()) {
            if (git->second.type.pointee) return git->second.type.pointee->size;
            return 8;
        }
    }
    // BinaryExpr: pointer + integer or integer + pointer — find the pointer operand
    if (node->kind == NodeKind::BinaryExpr && node->children.size() >= 2) {
        const std::string& op = node->sval;
        if (op == "+" || op == "PLUS" || op == "-" || op == "MINUS") {
            // Try left operand first to get its pointee size
            int left_sz = getExprPointeeSize(node->children[0].get());
            if (left_sz != 8) return left_sz;
            return getExprPointeeSize(node->children[1].get());
        }
    }
    // MemberExpr: struct->member or struct.member — look up member type in struct layouts
    if (node->kind == NodeKind::MemberExpr) {
        std::string member;
        bool is_arrow = false;
        if (node->sval.size() > 2 && node->sval.substr(0, 2) == "->") {
            member = node->sval.substr(2);
            is_arrow = true;
        } else if (node->sval.size() > 1 && node->sval[0] == '.')
            member = node->sval.substr(1);
        else if (node->children.size() > 1 && node->children[1]->kind == NodeKind::Ident)
            member = node->children[1]->sval;
        else
            member = node->sval;

        // Determine the struct type from LHS using getExprType()
        std::string lhs_struct_name;
        ASTNode* lhs = !node->children.empty() ? node->children[0].get() : nullptr;
        if (lhs) {
            TypeInfo lhs_type = getExprType(lhs);
            if (is_arrow && lhs_type.base == CType::Ptr && lhs_type.pointee)
                lhs_type = *lhs_type.pointee;
            if (lhs_type.base == CType::Struct && !lhs_type.struct_name.empty())
                lhs_struct_name = lhs_type.struct_name;
        }

        // First: try specific struct lookup
        if (!lhs_struct_name.empty()) {
            auto sit = structs_.find(lhs_struct_name);
            if (sit != structs_.end()) {
                for (auto& m : sit->second.members) {
                    if (m.name == member) {
                        if ((m.type.base == CType::Ptr || m.type.base == CType::Array) && m.type.pointee)
                            return m.type.pointee->size;
                        return m.type.size > 0 ? m.type.size : 8;
                    }
                }
            }
        }
        // Fallback: search all structs
        for (auto& [sname, layout] : structs_) {
            for (auto& m : layout.members) {
                if (m.name == member) {
                    if ((m.type.base == CType::Ptr || m.type.base == CType::Array) && m.type.pointee)
                        return m.type.pointee->size;
                }
            }
        }
        return 8;
    }
    // CastExpr: (T*)expr — resolve the cast target type including pointer levels
    if (node->kind == NodeKind::CastExpr && node->children.size() >= 2) {
        TypeInfo t = resolveType(node->children[0].get());
        // Count pointer levels from Pointer/AbstractDeclarator nodes (same as genCast)
        int ptr_levels = 0;
        std::function<void(ASTNode*)> countPtrs = [&](ASTNode* n) {
            if (!n) return;
            if (n->kind == NodeKind::Pointer) ptr_levels++;
            if (n->kind == NodeKind::AbstractDeclarator && n->sval == "*") ptr_levels++;
            for (auto& c : n->children) countPtrs(c.get());
        };
        countPtrs(node->children[0].get());
        for (int i = 0; i < ptr_levels; i++) {
            TypeInfo* pointee = new TypeInfo(t);
            t = TypeInfo::make_ptr();
            t.pointee = pointee;
        }
        if (t.base == CType::Ptr && t.pointee) return t.pointee->size;
        return getExprPointeeSize(node->children[1].get());
    }
    // UnaryExpr: *ptr — dereferencing a T** gives T*; element size = sizeof(T) = T*->pointee->size
    if (node->kind == NodeKind::UnaryExpr &&
        (node->sval == "*" || node->sval == "STAR") &&
        !node->children.empty()) {
        auto* inner = node->children[0].get();
        if (inner && inner->kind == NodeKind::Ident) {
            auto it = locals_.find(inner->sval);
            if (it != locals_.end() &&
                it->second.type.base == CType::Ptr &&
                it->second.type.pointee &&
                it->second.type.pointee->base == CType::Ptr &&
                it->second.type.pointee->pointee) {
                return it->second.type.pointee->pointee->size;
            }
            auto git = findGlobal(inner->sval);
            if (git != globals_.end() &&
                git->second.type.base == CType::Ptr &&
                git->second.type.pointee &&
                git->second.type.pointee->base == CType::Ptr &&
                git->second.type.pointee->pointee) {
                return git->second.type.pointee->pointee->size;
            }
        }
    }
    // For other complex exprs, default to 8
    return 8;
}

const char* CodeGen::ptrSizeDirective(int elem_size) {
    switch (elem_size) {
        case 1: return "BYTE";
        case 2: return "WORD";
        case 4: return "DWORD";
        default: return "QWORD";
    }
}

int CodeGen::ptrSizeShift(int elem_size) {
    switch (elem_size) {
        case 1: return 0;
        case 2: return 1;
        case 4: return 2;
        case 8: return 3;
        case 16: return 4;
        case 32: return 5;
        case 64: return 6;
        default: return -1; // 1.4: non-power-of-2 stride — caller must use imul
    }
}

// 2.1: Helper to get parameter type for callee argument i
TypeInfo CodeGen::getParamType(const std::string& fname, int idx) {
    if (!fname.empty()) {
        auto fit = functions_.find(fname);
        if (fit != functions_.end() &&
            idx >= 0 && idx < (int)fit->second.param_types.size())
            return fit->second.param_types[idx];
    }
    return TypeInfo::make_int();
}

// 1.4: scale register by elem_size for pointer arithmetic
// Uses shl for power-of-2 sizes; imul for non-power-of-2 (struct strides)
void CodeGen::emitPtrScale(const char* reg, int elem_size) {
    if (elem_size <= 1) return;
    int shift = ptrSizeShift(elem_size);
    if (shift > 0) emit("shl %s, %d", reg, shift);
    else           emit("imul %s, %d", reg, elem_size);
}

long long CodeGen::evalConstExpr(ASTNode* node) {
    if (!node) return 0;
    if (node->kind == NodeKind::IntLit) return node->ival;
    if (node->kind == NodeKind::CharLit) return node->ival;
    if (node->kind == NodeKind::FloatLit) {
        // C23: float literals in constant expressions — truncate to integer
        // Strip f/F/l/L suffixes before parsing
        try {
            std::string s = node->sval;
            while (!s.empty() && (s.back()=='f'||s.back()=='F'||s.back()=='l'||s.back()=='L')) s.pop_back();
            return (long long)std::stod(s);
        } catch(...) { return 0; }
    }
    if (node->kind == NodeKind::Ident) {
        // C23: Check constexpr variables first
        auto cit = constexpr_values_.find(node->sval);
        if (cit != constexpr_values_.end()) return cit->second;
        // GAP-3: C23 — float constexpr usable in integer constant expression (truncate)
        auto cfit = constexpr_float_values_.find(node->sval);
        if (cfit != constexpr_float_values_.end()) return (long long)cfit->second;
        // N3: Constant-propagated locals
        auto clit = const_locals_.find(node->sval);
        if (clit != const_locals_.end()) return clit->second;
        // Then check enum constants
        auto it = enum_constants_.find(node->sval);
        if (it != enum_constants_.end()) return it->second;
        return 0;
    }
    if (node->kind == NodeKind::UnaryExpr) {
        long long val = evalConstExpr(node->children[0].get());
        if (node->sval == "-" || node->sval == "MINUS") return -val;
        if (node->sval == "~" || node->sval == "TILDE") return ~val;
        if (node->sval == "!" || node->sval == "BANG") return !val;
        return val;
    }
    if (node->kind == NodeKind::BinaryExpr && node->children.size() >= 2) {
        long long l = evalConstExpr(node->children[0].get());
        long long r = evalConstExpr(node->children[1].get());
        const std::string& op = node->sval;
        if (op == "+" || op == "PLUS") return l + r;
        if (op == "-" || op == "MINUS") return l - r;
        if (op == "*" || op == "STAR") return l * r;
        if (op == "/" || op == "SLASH") return r ? l / r : 0;
        if (op == "%" || op == "%%" || op == "PERCENT") return r ? l % r : 0;
        if (op == "<<" || op == "LEFT_OP") return l << r;
        if (op == ">>" || op == "RIGHT_OP") return l >> r;
        if (op == "&" || op == "AMPERSAND") return l & r;
        if (op == "|" || op == "PIPE") return l | r;
        if (op == "^" || op == "CARET") return l ^ r;
        if (op == "==" || op == "EQ_OP") return l == r;
        if (op == "!=" || op == "NE_OP") return l != r;
        if (op == "<" || op == "LT") return l < r;
        if (op == ">" || op == "GT") return l > r;
        if (op == "<=" || op == "LE_OP") return l <= r;
        if (op == ">=" || op == "GE_OP") return l >= r;
        if (op == "&&" || op == "AND_OP") return l && r;
        if (op == "||" || op == "OR_OP") return l || r;
    }
    if (node->kind == NodeKind::ConditionalExpr && node->children.size() >= 3) {
        return evalConstExpr(node->children[0].get()) ?
            evalConstExpr(node->children[1].get()) : evalConstExpr(node->children[2].get());
    }
    // P4-E: Cast expression — propagate the constant value
    if (node->kind == NodeKind::CastExpr && !node->children.empty())
        return evalConstExpr(node->children.back().get());
    // C11: sizeof(type/expr) is a constant expression
    if (node->kind == NodeKind::SizeofExpr) {
        auto* child = node->children.empty() ? nullptr : node->children[0].get();
        if (child) {
            // sizeof(variable): look up variable type first (for arrays/structs/pointers)
            if (child->kind == NodeKind::Ident) {
                auto lit = locals_.find(child->sval);
                if (lit != locals_.end()) {
                    TypeInfo& vt = lit->second.type;
                    if (vt.base == CType::Array && vt.size > 0)
                        return (long long)vt.size;
                    if (vt.base == CType::Struct && !vt.struct_name.empty()) {
                        auto sit = structs_.find(vt.struct_name);
                        if (sit != structs_.end()) return (long long)sit->second.total_size;
                    }
                    if (vt.size > 0) return (long long)vt.size;
                }
                auto git = findGlobal(child->sval);
                if (git != globals_.end()) {
                    TypeInfo& vt = git->second.type;
                    if (vt.base == CType::Array && vt.size > 0)
                        return (long long)vt.size;
                    if (vt.base == CType::Struct && !vt.struct_name.empty()) {
                        auto sit = structs_.find(vt.struct_name);
                        if (sit != structs_.end()) return (long long)sit->second.total_size;
                    }
                    if (vt.size > 0) return (long long)vt.size;
                }
            }
            // sizeof(*ptr): dereference pointer to get pointee size
            if (child->kind == NodeKind::UnaryExpr &&
                (child->sval == "*" || child->sval == "STAR") &&
                !child->children.empty()) {
                auto* inner = child->children[0].get();
                TypeInfo inner_type = TypeInfo::make_int();
                if (inner && inner->kind == NodeKind::Ident) {
                    auto lit = locals_.find(inner->sval);
                    if (lit != locals_.end()) inner_type = lit->second.type;
                    else {
                        auto git = findGlobal(inner->sval);
                        if (git != globals_.end()) inner_type = git->second.type;
                    }
                }
                if (inner_type.base == CType::Ptr && inner_type.pointee) {
                    TypeInfo& pt = *inner_type.pointee;
                    if (pt.base == CType::Struct && !pt.struct_name.empty()) {
                        auto sit = structs_.find(pt.struct_name);
                        if (sit != structs_.end()) return (long long)sit->second.total_size;
                    }
                    if (pt.size > 0) return (long long)pt.size;
                }
            }
            // sizeof(arr[idx]) — subscript expression: resolve element type
            if (child->kind == NodeKind::SubscriptExpr && !child->children.empty()) {
                auto* arr_node = child->children[0].get();
                TypeInfo arr_type = getExprType(arr_node);
                // If getExprType returned struct (array member stored as struct),
                // the SubscriptExpr dereferences the array to get element type.
                // Look up struct size, or fall back to type.size.
                if (arr_type.base == CType::Struct && !arr_type.struct_name.empty()) {
                    auto sit = structs_.find(arr_type.struct_name);
                    if (sit != structs_.end())
                        return (long long)sit->second.total_size;
                    // Try without "struct " prefix
                    std::string bare = arr_type.struct_name;
                    if (bare.substr(0, 7) == "struct ") bare = bare.substr(7);
                    sit = structs_.find(bare);
                    if (sit != structs_.end())
                        return (long long)sit->second.total_size;
                    if (arr_type.size > 0)
                        return (long long)arr_type.size;
                }
                if ((arr_type.base == CType::Array || arr_type.base == CType::Ptr) && arr_type.pointee) {
                    TypeInfo& elem = *arr_type.pointee;
                    if (elem.base == CType::Struct && !elem.struct_name.empty()) {
                        auto sit = structs_.find(elem.struct_name);
                        if (sit != structs_.end()) return (long long)sit->second.total_size;
                    }
                    if (elem.size > 0) return (long long)elem.size;
                }
            }
            // sizeof(expr->member) or sizeof(expr.member): resolve member type
            if (child->kind == NodeKind::MemberExpr && !child->children.empty()) {
                // MemberExpr stores member name in sval: "->member" or ".member"
                std::string member_name = child->sval;
                if (member_name.size() >= 2 && member_name[0] == '-' && member_name[1] == '>')
                    member_name = member_name.substr(2);
                else if (!member_name.empty() && member_name[0] == '.')
                    member_name = member_name.substr(1);
                TypeInfo lhs_type = getExprType(child->children[0].get());
                if (lhs_type.base == CType::Ptr && lhs_type.pointee)
                    lhs_type = *lhs_type.pointee;
                if (lhs_type.base == CType::Struct && !lhs_type.struct_name.empty()) {
                    auto sit = structs_.find(lhs_type.struct_name);
                    if (sit != structs_.end()) {
                        for (auto& mem : sit->second.members) {
                            if (mem.name == member_name) {
                                if (mem.type.base == CType::Array && mem.type.size > 0)
                                    return (long long)mem.type.size;
                                if (mem.type.base == CType::Struct && !mem.type.struct_name.empty()) {
                                    auto sit2 = structs_.find(mem.type.struct_name);
                                    if (sit2 != structs_.end()) return (long long)sit2->second.total_size;
                                }
                                if (mem.type.size > 0) return (long long)mem.type.size;
                                break;
                            }
                        }
                    }
                }
            }
            TypeInfo t = resolveType(child);
            int sz = t.size ? t.size : 8;
            if (t.base == CType::Struct && !t.struct_name.empty()) {
                auto sit = structs_.find(t.struct_name);
                if (sit != structs_.end()) sz = sit->second.total_size;
            }
            // Also try typedef name -> "struct TAGNAME" lookup
            if (t.base != CType::Struct || t.struct_name.empty()) {
                std::string lookup_name;
                if (child->kind == NodeKind::TypeSpec) lookup_name = child->sval;
                else if (child->kind == NodeKind::Ident) lookup_name = child->sval;
                else if (child->kind == NodeKind::DeclSpecs && !child->children.empty()) {
                    auto* c0 = child->children[0].get();
                    if (c0 && (c0->kind == NodeKind::TypeSpec || c0->kind == NodeKind::Ident))
                        lookup_name = c0->sval;
                }
                if (!lookup_name.empty()) {
                    for (const char* prefix : {"struct ", "STRUCT ", "union ", "UNION "}) {
                        auto sn = std::string(prefix) + lookup_name;
                        auto sit2 = structs_.find(sn);
                        if (sit2 != structs_.end() && sit2->second.total_size > 0) {
                            sz = sit2->second.total_size; break;
                        }
                    }
                }
            }
            return (long long)sz;
        }
        return 8; // default pointer size
    }
    // GAP-6: C23 constexpr array subscript — look up in constexpr_array_values_
    if (node->kind == NodeKind::SubscriptExpr && node->children.size() >= 2) {
        auto* arr = node->children[0].get();
        auto* idx = node->children[1].get();
        if (arr && arr->kind == NodeKind::Ident) {
            auto ait = constexpr_array_values_.find(arr->sval);
            if (ait != constexpr_array_values_.end()) {
                long long idx_val = evalConstExpr(idx);
                if (idx_val >= 0 && (size_t)idx_val < ait->second.size())
                    return ait->second[(size_t)idx_val];
            }
        }
    }
    // GAP-D: C23 constexpr struct member access — look up in constexpr_struct_values_
    if (node->kind == NodeKind::MemberExpr && !node->children.empty()) {
        auto* obj = node->children[0].get();
        if (obj && obj->kind == NodeKind::Ident) {
            auto sit = constexpr_struct_values_.find(obj->sval);
            if (sit != constexpr_struct_values_.end()) {
                // sval is ".member" or "->member"
                std::string member = node->sval;
                if (member.size() >= 2 && member[0] == '-' && member[1] == '>')
                    member = member.substr(2);
                else if (!member.empty() && member[0] == '.')
                    member = member.substr(1);
                auto mit = sit->second.find(member);
                if (mit != sit->second.end()) return mit->second;
            }
        }
    }
    // Recurse into single-child nodes
    if (node->children.size() == 1) return evalConstExpr(node->children[0].get());
    return 0;
}

// C23: evaluate a constant expression as double (for constexpr float/double variables).
double CodeGen::evalConstExprDouble(ASTNode* node) {
    if (!node) return 0.0;
    if (node->kind == NodeKind::FloatLit) {
        std::string s = node->sval;
        while (!s.empty() && (s.back()=='f'||s.back()=='F'||s.back()=='l'||s.back()=='L')) s.pop_back();
        try { return std::stod(s); } catch(...) { return 0.0; }
    }
    if (node->kind == NodeKind::IntLit || node->kind == NodeKind::CharLit)
        return (double)node->ival;
    if (node->kind == NodeKind::Ident) {
        auto fit = constexpr_float_values_.find(node->sval);
        if (fit != constexpr_float_values_.end()) return fit->second;
        auto cit = constexpr_values_.find(node->sval);
        if (cit != constexpr_values_.end()) return (double)cit->second;
        return 0.0;
    }
    if (node->kind == NodeKind::UnaryExpr && !node->children.empty()) {
        double v = evalConstExprDouble(node->children[0].get());
        if (node->sval == "-" || node->sval == "MINUS") return -v;
        return v;
    }
    if (node->kind == NodeKind::BinaryExpr && node->children.size() >= 2) {
        double l = evalConstExprDouble(node->children[0].get());
        double r = evalConstExprDouble(node->children[1].get());
        const std::string& op = node->sval;
        if (op == "+" || op == "PLUS")  return l + r;
        if (op == "-" || op == "MINUS") return l - r;
        if (op == "*" || op == "STAR")  return l * r;
        if ((op == "/" || op == "SLASH") && r != 0.0) return l / r;
    }
    if (node->kind == NodeKind::CastExpr && !node->children.empty())
        return evalConstExprDouble(node->children.back().get());
    if (node->children.size() == 1) return evalConstExprDouble(node->children[0].get());
    return 0.0;
}

void CodeGen::emit(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    fprintf(out_, "    ");
    vfprintf(out_, fmt, ap);
    fprintf(out_, "\n");
    va_end(ap);
}

void CodeGen::emitLabel(int label) {
    fprintf(out_, "_L%d:\n", label);
}

int CodeGen::allocLocal(int size) {
    // Align to 8 bytes
    size = (size + 7) & ~7;
    local_offset_ += size;
    return -local_offset_;
}

// C11: _Alignas-aware allocation — pads local_offset_ to alignment boundary first.
int CodeGen::allocLocalAligned(int size, int align) {
    if (align <= 8) return allocLocal(size);
    // Pad local_offset_ up to the alignment boundary
    int rem = local_offset_ % align;
    if (rem != 0) local_offset_ += (align - rem);
    // Track the maximum alignment seen in this function (for prologue)
    if (align > max_align_req_) max_align_req_ = align;
    // Round size up to alignment as well
    size = (size + align - 1) & ~(align - 1);
    local_offset_ += size;
    return -local_offset_;
}


// Helper: parse C string literal (with surrounding quotes) into raw bytes.
// Used for global char arr[] = "..." initializers to inline bytes.
static std::vector<int> parseStringLiteralToBytes(const std::string& val) {
    std::vector<int> bytes;
    for (size_t i = 1; i < val.size() - 1; i++) {
        if ((unsigned char)val[i] == 92 && i + 1 < val.size() - 1) {
            i++;
            switch (val[i]) {
            case 'n':  bytes.push_back(10); break;
            case 't':  bytes.push_back(9);  break;
            case 92:   bytes.push_back(92); break;  // backslash
            case 34:   bytes.push_back(34); break;  // double-quote
            case 39:   bytes.push_back(39); break;  // single-quote
            case '0':  bytes.push_back(0);  break;
            case 'r':  bytes.push_back(13); break;
            case 'a':  bytes.push_back(7);  break;
            case 'b':  bytes.push_back(8);  break;
            case 'f':  bytes.push_back(12); break;
            case 'v':  bytes.push_back(11); break;
            case 'x': {
                int hv = 0;
                while (i + 1 < val.size() - 1) {
                    char h = val[i+1];
                    if (h >= '0' && h <= '9') hv = hv*16 + (h-'0');
                    else if (h >= 'a' && h <= 'f') hv = hv*16 + (h-'a'+10);
                    else if (h >= 'A' && h <= 'F') hv = hv*16 + (h-'A'+10);
                    else break;
                    i++;
                }
                bytes.push_back(hv & 0xFF);
                break;
            }
            default: bytes.push_back((unsigned char)val[i]); break;
            }
        } else {
            bytes.push_back((unsigned char)val[i]);
        }
    }
    bytes.push_back(0); // null terminator
    return bytes;
}

// Register a string literal in string_literals_ and return its label.
// Used for global variable initializers that point to string literals.
std::string CodeGen::addStringLiteral(const std::string& val, int char_width) {
    std::string label = "__str_" + std::to_string(string_count_++);
    string_literals_.emplace_back(label, val, char_width);
    return label;
}

TypeInfo CodeGen::resolveType(ASTNode* node) {
    if (!node) return TypeInfo::make_int();
    // Unwrap DeclSpecs: collect all specifiers and combine
    if (node->kind == NodeKind::DeclSpecs) {
        bool has_unsigned = false, has_signed = false;
        bool has_long = false, has_long_long = false;
        bool has_short = false, has_char = false;
        bool has_int = false, has_void = false;
        bool has_float = false, has_double = false;
        bool has_const = false;
        for (auto& c : node->children) {
            if (c->kind == NodeKind::StructDef || c->kind == NodeKind::EnumDef)
                return resolveType(c.get());
            // type_name (cast type): child may be a nested DeclSpecs (specifier_qualifier_list)
            // Recursively resolve it and propagate flags rather than skipping.
            if (c->kind == NodeKind::DeclSpecs) {
                TypeInfo inner = resolveType(c.get());
                // Propagate the inner type's flags into our flag variables
                if (inner.base == CType::Char) has_char = true;
                else if (inner.base == CType::Short) has_short = true;
                else if (inner.base == CType::Long) has_long = true;
                else if (inner.base == CType::Void) has_void = true;
                else if (inner.base == CType::Float) has_float = true;
                else if (inner.base == CType::Ptr || inner.base == CType::Struct) return inner;
                // For Int, keep the flag as-is (default)
                if (inner.is_unsigned) has_unsigned = true;
                continue;
            }
            if (c->kind == NodeKind::TypeQualifier) {
                if (c->sval == "CONST" || c->sval == "const") has_const = true;
                continue;
            }
            if (c->kind != NodeKind::TypeSpec) continue;
            std::string s = c->sval;
            if (s == "UNSIGNED" || s == "unsigned") has_unsigned = true;
            else if (s == "SIGNED" || s == "signed") has_signed = true;
            else if (s == "long") {  /* only lowercase 'long' keyword; uppercase LONG is a typedef */
                if (has_long) has_long_long = true;
                else has_long = true;
            }
            else if (s == "SHORT" || s == "short") has_short = true;
            else if (s == "CHAR" || s == "char") has_char = true;
            else if (s == "INT" || s == "int") has_int = true;
            else if (s == "VOID" || s == "void") has_void = true;
            else if (s == "FLOAT" || s == "float") has_float = true;
            else if (s == "DOUBLE" || s == "double") has_double = true;
            else {
                // Check if it's a typedef name or struct name
                auto tit = typedefs_.find(s);
                if (tit != typedefs_.end()) {
                    TypeInfo t = tit->second;
                    // If typedef is a struct type with size 0, look up actual struct size
                    if (t.base == CType::Struct && t.size == 0 && !t.struct_name.empty()) {
                        auto ssit = structs_.find(t.struct_name);
                        if (ssit != structs_.end()) t.size = ssit->second.total_size;
                    }
                    if (has_unsigned) t.is_unsigned = true;
                    return t;
                }
                auto sit = structs_.find(s);
                if (sit != structs_.end()) {
                    return TypeInfo::make_struct(s, sit->second.total_size);
                }
                // Handle "struct/STRUCT TAGNAME" or "union/UNION TAGNAME" TypeSpec for forward-declared structs
                for (const char* pfx : {"struct ", "STRUCT ", "union ", "UNION "}) {
                    size_t plen = strlen(pfx);
                    if (s.size() > plen && s.substr(0, plen) == pfx) {
                        auto fsit = structs_.find(s);
                        if (fsit != structs_.end()) return TypeInfo::make_struct(s, fsit->second.total_size);
                        return TypeInfo::make_struct(s, 0);  // Forward reference
                    }
                }
            }
        }
        TypeInfo t;
        if (has_void) { t = TypeInfo::make_void(); }
        else if (has_double) { t = TypeInfo::make_double(); }
        else if (has_float) { t = TypeInfo::make_float(); }
        else if (has_char) { t = TypeInfo::make_char(); }
        else if (has_short) { t.base = CType::Short; t.size = 2; }
        else if (has_long_long || has_long) { t = TypeInfo::make_long(); }
        else { t = TypeInfo::make_int(); }
        t.is_unsigned = has_unsigned;
        return t;
    }
    if (node->kind == NodeKind::EnumDef) {
        // C23: typed enum — look up underlying type size from preprocessor side-table
        if (!node->sval.empty()) {
            auto eit = g_enum_underlying_sizes.find(node->sval);
            if (eit != g_enum_underlying_sizes.end()) {
                TypeInfo t = TypeInfo::make_int();
                t.size = eit->second;
                return t;
            }
        }
        return TypeInfo::make_int();
    }
    if (node->kind == NodeKind::StructDef) {
        if (!node->sval.empty()) {
            auto it = structs_.find(node->sval);
            if (it != structs_.end())
                return TypeInfo::make_struct(node->sval, it->second.total_size);
        }
        // Inline struct/union — compute layout on the fly
        // Handles both anonymous (sval == "struct") and named (sval == "struct Foo")
        if (!node->children.empty() && (node->sval == "struct" || node->sval == "union"
                || node->sval == "STRUCT" || node->sval == "UNION"
                || node->sval.compare(0, 7, "struct ") == 0
                || node->sval.compare(0, 6, "union ") == 0
                || node->sval.compare(0, 7, "STRUCT ") == 0
                || node->sval.compare(0, 6, "UNION ") == 0)) {
            bool inner_union = (node->sval == "union" || node->sval == "UNION"
                || node->sval.compare(0, 6, "union ") == 0
                || node->sval.compare(0, 6, "UNION ") == 0);
            // Collect all members from the inline struct/union
            std::vector<ASTNode*> inlineMembers;
            for (auto& ch : node->children) {
                ASTNode* mem = ch.get();
                if (!mem) continue;
                if (mem->kind == NodeKind::StructDeclList) {
                    for (auto& m2 : mem->children) inlineMembers.push_back(m2.get());
                } else if (mem->kind == NodeKind::StructMember || mem->kind == NodeKind::Declaration) {
                    inlineMembers.push_back(mem);
                }
            }
            int sz = 0;
            int max_align = 1;
            // Bitfield packing state
            int bf_off = -1, bf_used = 0, bf_unit = 0;
            static const std::string BF_TAG = ":bitfield";
            for (auto* mem : inlineMembers) {
                if (!mem) continue;
                // Get member name to detect bitfields
                std::string mname;
                ASTNode* mdnode = nullptr;
                if (mem->children.size() > 1) {
                    mdnode = mem->children[1].get();
                    if (mdnode && mdnode->kind == NodeKind::DeclList && !mdnode->children.empty())
                        mdnode = mdnode->children[0].get();
                    if (mdnode && mdnode->kind == NodeKind::InitDeclarator && !mdnode->children.empty())
                        mdnode = mdnode->children[0].get();
                    if (mdnode) mname = getDeclaratorName(mdnode);
                }
                if (mname.empty() && !mem->children.empty()) mname = mem->sval;
                // Detect bitfield
                bool is_bf = (mname.size() >= BF_TAG.size() &&
                    mname.compare(mname.size() - BF_TAG.size(), BF_TAG.size(), BF_TAG) == 0);
                int bw = 0;
                if (is_bf && mdnode && !mdnode->children.empty())
                    bw = (int)evalConstExpr(mdnode->children.back().get());
                TypeInfo mt = TypeInfo::make_int();
                if (!mem->children.empty()) mt = resolveType(mem->children[0].get());
                // Check for pointer declarator
                if (mem->children.size() > 1) {
                    auto* md = mem->children[1].get();
                    if (md && md->kind == NodeKind::DeclList && !md->children.empty())
                        md = md->children[0].get();
                    if (md && md->kind == NodeKind::InitDeclarator && !md->children.empty())
                        md = md->children[0].get();
                    if (md && md->kind == NodeKind::Declarator && md->sval == "*") {
                        TypeInfo* pe = new TypeInfo(mt);
                        mt = TypeInfo::make_ptr(); mt.pointee = pe;
                    }
                }
                if (is_bf) {
                    int unit_sz = mt.size > 0 ? mt.size : 4;
                    if (unit_sz > 8) unit_sz = 8;
                    int unit_bits = unit_sz * 8;
                    if (bf_off < 0 || bf_used + bw > unit_bits || bf_unit != unit_sz) {
                        if (!inner_union) {
                            if (unit_sz > 1) sz = (sz + unit_sz - 1) & ~(unit_sz - 1);
                            bf_off = sz;
                            sz += unit_sz;
                        } else {
                            bf_off = 0;
                        }
                        bf_used = 0;
                        bf_unit = unit_sz;
                    }
                    bf_used += bw;
                    if (unit_sz > max_align) max_align = unit_sz > 8 ? 8 : unit_sz;
                    if (inner_union && unit_sz > sz) sz = unit_sz;
                } else {
                    // Reset bitfield state
                    bf_off = -1; bf_used = 0; bf_unit = 0;
                    int msz = mt.size > 0 ? mt.size : 8;
                    int a = msz; if (a > 8) a = 8; if (a > max_align) max_align = a;
                    if (inner_union) {
                        if (msz > sz) sz = msz;
                    } else {
                        sz = (sz + a - 1) & ~(a - 1);
                        sz += msz;
                    }
                }
            }
            if (sz > 0) sz = (sz + max_align - 1) & ~(max_align - 1);
            int final_sz = sz > 0 ? sz : 8;
            // Register named inner structs so sizeof/declarations work later
            if (node->sval != "struct" && node->sval != "union" &&
                node->sval != "STRUCT" && node->sval != "UNION" &&
                structs_.find(node->sval) == structs_.end()) {
                StructLayout inner_layout;
                inner_layout.name = node->sval;
                inner_layout.total_size = final_sz;
                inner_layout.is_packed = (g_pack_level == 1);
                structs_[node->sval] = inner_layout;
            }
            return TypeInfo::make_struct(node->sval, final_sz);
        }
        return TypeInfo::make_struct(node->sval, 8);
    }
    if (node->kind != NodeKind::TypeSpec)
        return TypeInfo::make_int();
    // C11: _Atomic(T) — non-atomic stub; resolve inner type
    if (node->sval == "_Atomic" && !node->children.empty())
        return resolveType(node->children[0].get());
    // C11: _Alignas — annotation only; skip (caller gets default int)
    if (node->sval == "_Alignas")
        return TypeInfo::make_int();
    if (node->sval == "INT" || node->sval == "int") return TypeInfo::make_int();
    if (node->sval == "CHAR" || node->sval == "char") return TypeInfo::make_char();
    if (node->sval == "VOID" || node->sval == "void") return TypeInfo::make_void();
    if (node->sval == "long") return TypeInfo::make_long();  /* lowercase only; LONG is a typedef */
    if (node->sval == "SHORT" || node->sval == "short") {
        TypeInfo t; t.base = CType::Short; t.size = 2; return t;
    }
    if (node->sval == "DOUBLE" || node->sval == "double") return TypeInfo::make_double();
    if (node->sval == "FLOAT" || node->sval == "float") return TypeInfo::make_float();
    if (node->sval == "SIGNED" || node->sval == "signed") return TypeInfo::make_int();
    if (node->sval == "UNSIGNED" || node->sval == "unsigned") {
        TypeInfo t = TypeInfo::make_int();
        t.is_unsigned = true;
        return t;
    }
    // C99: va_list is a built-in type (pointer to char)
    if (node->sval == "va_list" || node->sval == "__builtin_va_list") {
        TypeInfo t = TypeInfo::make_ptr();
        t.pointee = new TypeInfo(TypeInfo::make_char());
        return t;
    }
    // Check typedefs
    {
        auto it = typedefs_.find(node->sval);
        if (it != typedefs_.end()) return it->second;
    }
    // Check if it's a known struct name
    {
        auto it = structs_.find(node->sval);
        if (it != structs_.end()) {
            return TypeInfo::make_struct(node->sval, it->second.total_size);
        }
    }
    // Handle "struct/STRUCT/union/UNION TAGNAME" TypeSpec for forward-declared structs
    {
        std::string sval = node->sval;
        for (const char* pfx : {"struct ", "STRUCT ", "union ", "UNION "}) {
            size_t plen = strlen(pfx);
            if (sval.size() > plen && sval.substr(0, plen) == pfx) {
                auto it = structs_.find(sval);
                if (it != structs_.end()) return TypeInfo::make_struct(sval, it->second.total_size);
                return TypeInfo::make_struct(sval, 0);  // Forward reference
            }
        }
    }
    // C23: typed enum reference — look up enum tag in underlying-size table
    {
        auto eit = g_enum_underlying_sizes.find(node->sval);
        if (eit != g_enum_underlying_sizes.end()) {
            TypeInfo t = TypeInfo::make_int();
            t.size = eit->second;
            return t;
        }
    }
    return TypeInfo::make_int();
}

std::string CodeGen::getDeclaratorName(ASTNode* decl) {
    if (!decl) return "";
    if (decl->kind == NodeKind::Declarator) {
        if (decl->sval == "*") {
            // pointer declarator: children[0]=Pointer, children[1]=direct_declarator
            if (decl->children.size() > 1)
                return getDeclaratorName(decl->children[1].get());
            if (!decl->children.empty())
                return getDeclaratorName(decl->children[0].get());
        }
        if (decl->sval == "[]" || decl->sval == "[N]" || decl->sval == "()") {
            if (!decl->children.empty())
                return getDeclaratorName(decl->children[0].get());
        }
        return decl->sval;
    }
    return "";
}

bool CodeGen::isDeclaratorFunc(ASTNode* decl) {
    if (!decl || decl->kind != NodeKind::Declarator) return false;
    if (decl->sval == "()") return true;
    // Check if pointer to function: sval=="*" and inner declarator is "()"
    if (decl->sval == "*") {
        for (auto& c : decl->children) {
            if (c && c->kind == NodeKind::Declarator && c->sval == "()") return true;
        }
    }
    return false;
}

void CodeGen::generate(ASTNode* root) {
    if (!root) return;

    // First pass: collect defined function names
    std::set<std::string> defined_functions;
    for (auto& child : root->children) {
        if (child->kind == NodeKind::FunctionDef && child->children.size() >= 2) {
            defined_functions.insert(getDeclaratorName(child->children[1].get()));
        }
    }

    // Generate code to a temp buffer so we can emit EXTERNs first
    FILE* tmpf = reliable_tmpfile();
    FILE* real_out = out_;
    out_ = tmpf;

    genTranslationUnit(root);

    out_ = real_out;

    // MASM prologue
    fprintf(out_, "; Generated by NCC C Compiler (LALRGen-based)\n");
    fprintf(out_, "; Target: x86-64 MASM for MSVC\n\n");
    fprintf(out_, "INCLUDELIB msvcrt\n");
    fprintf(out_, "INCLUDELIB ucrt\n");
    fprintf(out_, "INCLUDELIB legacy_stdio_definitions\n\n");
    // Disable x87 FPU mnemonics that clash with standard C function names.
    // MASM is case-insensitive so FABS, fabs, and Fabs all conflict.
    // RCC only emits SSE/AVX float ops for x64, so these mnemonics are unused.
    fprintf(out_, "OPTION NOKEYWORD: <fabs, fsin, fcos, fsqrt, fmul, fadd, fsub, fdiv, fst, fld>\n\n");

    // Emit EXTERNs for called but not defined functions
    for (auto& fn : called_functions_) {
        if (defined_functions.find(fn) == defined_functions.end()) {
            fprintf(out_, "EXTERN %s:PROC\n", fn.c_str());
        }
    }
    fprintf(out_, "\n.code\n\n");

    // Copy temp buffer to output
    fflush(tmpf);
    fseek(tmpf, 0, SEEK_END);
    long tmpsize = ftell(tmpf);
    fseek(tmpf, 0, SEEK_SET);
    if (tmpsize > 0) {
        char* buf = (char*)malloc(tmpsize);
        if (buf) {
            size_t nr = fread(buf, 1, tmpsize, tmpf);
            fwrite(buf, 1, nr, out_);
            free(buf);
        }
    }
    cleanup_tmpfile(tmpf);

    // Emit string, float, and double literals in .data section
    if (!string_literals_.empty() || !float_literals_.empty() || !double_literals_.empty()) {
        fprintf(out_, "\n.data\n");
        for (auto& [label, val, char_width] : string_literals_) {
            // Collect code units (each unit is 1, 2, or 4 bytes depending on char_width)
            std::vector<int> units;
            for (size_t i = 1; i < val.size() - 1; i++) { // skip outer quotes
                if (val[i] == '\\' && i + 1 < val.size() - 1) {
                    i++;
                    switch (val[i]) {
                    case 'n':  units.push_back(10);  break;
                    case 't':  units.push_back(9);   break;
                    case '\\': units.push_back(92);  break;
                    case '"':  units.push_back(34);  break;
                    case '0':  units.push_back(0);   break;
                    case 'r':  units.push_back(13);  break;
                    case 'a':  units.push_back(7);   break;
                    case 'b':  units.push_back(8);   break;
                    case 'f':  units.push_back(12);  break;
                    case 'v':  units.push_back(11);  break;
                    case 'x': { // hex escape: \xHH (fixed) or C23 \x{HHH} (delimited)
                        int hv = 0;
                        if (i + 1 < val.size() - 1 && val[i+1] == '{') {
                            // C23 §6.4.5: delimited \x{HHH}
                            i++; // skip '{'
                            while (i + 1 < val.size() - 1 && val[i+1] != '}') {
                                char h = val[++i];
                                if (h >= '0' && h <= '9') hv = hv*16 + (h-'0');
                                else if (h >= 'a' && h <= 'f') hv = hv*16 + (h-'a'+10);
                                else if (h >= 'A' && h <= 'F') hv = hv*16 + (h-'A'+10);
                            }
                            if (i + 1 < val.size() - 1 && val[i+1] == '}') i++; // skip '}'
                        } else { // fixed-width \xHH
                            while (i + 1 < val.size() - 1) {
                                char h = val[i+1];
                                if (h >= '0' && h <= '9') hv = hv*16 + (h-'0');
                                else if (h >= 'a' && h <= 'f') hv = hv*16 + (h-'a'+10);
                                else if (h >= 'A' && h <= 'F') hv = hv*16 + (h-'A'+10);
                                else break;
                                i++;
                            }
                        }
                        units.push_back(hv);
                        break;
                    }
                    case 'o': { // C23 §6.4.5: delimited octal \o{OOO}
                        int ov = 0;
                        if (i + 1 < val.size() - 1 && val[i+1] == '{') {
                            i++; // skip '{'
                            while (i + 1 < val.size() - 1 && val[i+1] != '}') {
                                char h = val[++i];
                                if (h >= '0' && h <= '7') ov = ov*8 + (h-'0');
                            }
                            if (i + 1 < val.size() - 1 && val[i+1] == '}') i++; // skip '}'
                        }
                        units.push_back(ov);
                        break;
                    }
                    case 'u': // C23 §6.4.5: \u{HHHH} or \uHHHH fixed
                    case 'U': { // C23 §6.4.5: \U{HHHHHHHH} or \UHHHHHHHH fixed
                        int uv = 0;
                        if (i + 1 < val.size() - 1 && val[i+1] == '{') {
                            i++; // skip '{'
                            while (i + 1 < val.size() - 1 && val[i+1] != '}') {
                                char h = val[++i];
                                if (h >= '0' && h <= '9') uv = uv*16 + (h-'0');
                                else if (h >= 'a' && h <= 'f') uv = uv*16 + (h-'a'+10);
                                else if (h >= 'A' && h <= 'F') uv = uv*16 + (h-'A'+10);
                            }
                            if (i + 1 < val.size() - 1 && val[i+1] == '}') i++; // skip '}'
                        } else { // fixed-width: read up to 8 hex digits
                            for (int nd = 0; nd < 8 && i + 1 < val.size() - 1; nd++) {
                                char h = val[i+1];
                                if (h >= '0' && h <= '9') uv = uv*16 + (h-'0');
                                else if (h >= 'a' && h <= 'f') uv = uv*16 + (h-'a'+10);
                                else if (h >= 'A' && h <= 'F') uv = uv*16 + (h-'A'+10);
                                else break;
                                i++;
                            }
                        }
                        units.push_back(uv & 0xFF); // truncate to byte for char context
                        break;
                    }
                    default:   units.push_back((int)(unsigned char)val[i]); break;
                    }
                } else {
                    units.push_back((int)(unsigned char)val[i]);
                }
            }
            units.push_back(0); // null terminator (one code unit of the appropriate width)

            // Select directive and chunk size based on char width
            const char* dir = (char_width == 4) ? "DD" : (char_width == 2) ? "DW" : "DB";
            size_t chunk = (char_width >= 2) ? 8 : 16; // fewer per line for wider directives

            for (size_t i = 0; i < units.size(); i += chunk) {
                if (i == 0)
                    fprintf(out_, "%s %s ", label.c_str(), dir);
                else
                    fprintf(out_, "    %s ", dir);
                size_t end = std::min(i + chunk, units.size());
                for (size_t j = i; j < end; j++) {
                    if (j > i) fprintf(out_, ", ");
                    fprintf(out_, "%d", units[j]);
                }
                fprintf(out_, "\n");
            }
        }
        // Emit float literals (REAL4 = single precision)
        // MASM does not accept "inf"/"nan" text; use IEEE 754 hex for special values.
        for (auto& [label, fval] : float_literals_) {
            if (std::isinf(fval) || std::isnan(fval)) {
                uint32_t bits;
                memcpy(&bits, &fval, sizeof(bits));
                fprintf(out_, "%s REAL4 0%08Xr\n", label.c_str(), bits);
            } else {
                fprintf(out_, "%s REAL4 %e\n", label.c_str(), fval);
            }
        }
        // Emit double literals (REAL8 = double precision, used by constexpr float/double)
        for (auto& [label, dval] : double_literals_) {
            if (std::isinf(dval) || std::isnan(dval)) {
                uint64_t bits;
                memcpy(&bits, &dval, sizeof(bits));
                fprintf(out_, "%s REAL8 0%016llXr\n", label.c_str(), (unsigned long long)bits);
            } else {
                fprintf(out_, "%s REAL8 %.17g\n", label.c_str(), dval);
            }
        }
    }

    // C11: _Thread_local variables — emit to _TLS segment before .data sections.
    // The PE TLS mechanism provides per-thread copies at runtime (single-thread access
    // by label name works correctly for the primary thread).
    {
        bool has_tls = false;
        for (auto& [name, info] : globals_) {
            if (!info.is_tls) continue;
            if (!has_tls) {
                fprintf(out_, "\n_TLS SEGMENT 'TLS'\n");
                has_tls = true;
            }
            long long init_val = info.has_init ? info.init_value : 0;
            fprintf(out_, "%s DQ %lld\n", info.global_label.c_str(), init_val);
        }
        if (has_tls) fprintf(out_, "_TLS ENDS\n");
    }

    // Initialized globals (.data section)
    {
        bool has_inited = false;
        for (auto& [name, info] : globals_) {
            if (info.is_global && info.has_init && !info.is_tls) {
                if (!has_inited) { fprintf(out_, "\n.data\n"); has_inited = true; }
                // C11: _Alignas — emit ALIGN directive before the label
                if (info.align_req > 8)
                    fprintf(out_, "ALIGN %d\n", info.align_req);
                if (!info.init_str_label.empty()) {
                    // C11/R3: global pointer initialised to a string literal address
                    fprintf(out_, "%s DQ OFFSET %s\n",
                            info.global_label.c_str(), info.init_str_label.c_str());
                } else if (!info.init_values.empty()) {
                    if (info.type.base == CType::Struct) {
                        // Struct initializer: emit per-member with correct size (DD for int, DQ for ptr)
                        auto sit = structs_.find(info.type.struct_name);
                        if (sit != structs_.end() && sit->second.members.empty()) {
                            // Struct with known size but no member info: emit zero-filled bytes
                            fprintf(out_, "%s DB %d DUP(0)\n",
                                    info.global_label.c_str(), sit->second.total_size);
                        } else if (sit != structs_.end()) {
                            auto& layout = sit->second;
                            int cur_byte = 0; // track current byte offset in emitted data
                            for (size_t iv = 0; iv < info.init_values.size() && iv < layout.members.size(); iv++) {
                                int moff = layout.members[iv].offset;
                                // Emit alignment padding bytes if member offset > current byte position
                                if (moff > cur_byte) {
                                    int pad = moff - cur_byte;
                                    if (iv == 0 && cur_byte == 0) {
                                        // Padding before first member (unusual but handle it)
                                        fprintf(out_, "%s DB %d DUP(0)\n", info.global_label.c_str(), pad);
                                    } else {
                                        fprintf(out_, "    DB %d DUP(0)\n", pad);
                                    }
                                    cur_byte += pad;
                                }
                                int msz = layout.members[iv].type.size > 0 ? layout.members[iv].type.size : 8;
                                const char* mdir = (msz <= 1) ? "DB" : (msz <= 4) ? "DD" : "DQ";
                                // Quality 3.1: function-pointer member — emit OFFSET instead of literal 0
                                bool is_fn = iv < info.init_fn_names.size() && !info.init_fn_names[iv].empty();
                                // Embedded struct members: when the init value is 0 (from nested
                                // initializer list evaluated to 0), emit the full number of zero bytes.
                                // This handles cases like: struct Foo { struct Bar b; } = { {0,0,...} };
                                bool is_embedded_struct = (layout.members[iv].type.base == CType::Struct)
                                    && !is_fn && info.init_values[iv] == 0 && msz > 8;
                                if (is_embedded_struct) {
                                    // Emit msz zero bytes for the embedded struct
                                    if (iv == 0 && cur_byte == 0)
                                        fprintf(out_, "%s DB %d DUP(0)\n", info.global_label.c_str(), msz);
                                    else
                                        fprintf(out_, "    DB %d DUP(0)\n", msz);
                                } else if (iv == 0 && cur_byte == 0) {
                                    if (is_fn)
                                        fprintf(out_, "%s DQ OFFSET %s\n",
                                                info.global_label.c_str(), info.init_fn_names[iv].c_str());
                                    else
                                        fprintf(out_, "%s %s %lld\n",
                                                info.global_label.c_str(), mdir, info.init_values[iv]);
                                } else {
                                    if (is_fn)
                                        fprintf(out_, "    DQ OFFSET %s\n", info.init_fn_names[iv].c_str());
                                    else
                                        fprintf(out_, "    %s %lld\n", mdir, info.init_values[iv]);
                                }
                                cur_byte += msz;
                            }
                            // Emit tail padding to reach total_size
                            if (cur_byte < layout.total_size) {
                                fprintf(out_, "    DB %d DUP(0)\n", layout.total_size - cur_byte);
                            }
                        } else {
                            // Unknown struct layout — fall back to DQ (safe for pointer-heavy structs)
                            for (size_t iv = 0; iv < info.init_values.size(); iv++) {
                                bool is_fn = iv < info.init_fn_names.size() && !info.init_fn_names[iv].empty();
                                if (iv == 0) {
                                    if (is_fn)
                                        fprintf(out_, "%s DQ OFFSET %s\n",
                                                info.global_label.c_str(), info.init_fn_names[iv].c_str());
                                    else
                                        fprintf(out_, "%s DQ %lld\n",
                                                info.global_label.c_str(), info.init_values[iv]);
                                } else {
                                    if (is_fn)
                                        fprintf(out_, "    DQ OFFSET %s\n", info.init_fn_names[iv].c_str());
                                    else
                                        fprintf(out_, "    DQ %lld\n", info.init_values[iv]);
                                }
                            }
                        }
                    } else if (info.type.base == CType::Array && info.type.pointee &&
                               info.type.pointee->base == CType::Struct &&
                               !info.type.pointee->struct_name.empty()) {
                        // Array-of-struct initializer: emit per-member data with layout-aware padding
                        std::string sn = info.type.pointee->struct_name;
                        auto sit = structs_.find(sn);
                        if (sit == structs_.end()) {
                            // Try with/without "struct " prefix
                            if (sn.substr(0, 7) == "struct ")
                                sit = structs_.find(sn.substr(7));
                            else
                                sit = structs_.find("struct " + sn);
                        }
                        if (sit != structs_.end() && !sit->second.members.empty()) {
                            auto& layout = sit->second;
                            int nMembers = (int)layout.members.size();
                            int nElems = (int)info.init_values.size() / nMembers;
                            if (nElems == 0) nElems = 1;
                            bool first_label = true;
                            for (int e = 0; e < nElems; e++) {
                                int base_idx = e * nMembers;
                                int cur_byte = 0;
                                for (int iv = 0; iv < nMembers && base_idx + iv < (int)info.init_values.size(); iv++) {
                                    int moff = layout.members[iv].offset;
                                    // Emit alignment padding
                                    if (moff > cur_byte) {
                                        int pad = moff - cur_byte;
                                        if (first_label) {
                                            fprintf(out_, "%s DB %d DUP(0)\n", info.global_label.c_str(), pad);
                                            first_label = false;
                                        } else {
                                            fprintf(out_, "    DB %d DUP(0)\n", pad);
                                        }
                                        cur_byte += pad;
                                    }
                                    int msz = layout.members[iv].type.size > 0 ? layout.members[iv].type.size : 8;
                                    // For array/embedded-struct members, emit as zero block if value is 0
                                    bool is_embedded = (layout.members[iv].type.base == CType::Struct ||
                                                       layout.members[iv].type.base == CType::Array) && msz > 8;
                                    const char* mdir = (msz <= 1) ? "DB" : (msz <= 2) ? "DW" : (msz <= 4) ? "DD" : "DQ";
                                    int fidx = base_idx + iv;
                                    bool is_fn = fidx < (int)info.init_fn_names.size() && !info.init_fn_names[fidx].empty();
                                    if (is_embedded && !is_fn && info.init_values[fidx] == 0) {
                                        if (first_label) {
                                            fprintf(out_, "%s DB %d DUP(0)\n", info.global_label.c_str(), msz);
                                            first_label = false;
                                        } else {
                                            fprintf(out_, "    DB %d DUP(0)\n", msz);
                                        }
                                    } else if (first_label) {
                                        if (is_fn)
                                            fprintf(out_, "%s DQ OFFSET %s\n", info.global_label.c_str(), info.init_fn_names[fidx].c_str());
                                        else
                                            fprintf(out_, "%s %s %lld\n", info.global_label.c_str(), mdir, info.init_values[fidx]);
                                        first_label = false;
                                    } else {
                                        if (is_fn)
                                            fprintf(out_, "    DQ OFFSET %s\n", info.init_fn_names[fidx].c_str());
                                        else
                                            fprintf(out_, "    %s %lld\n", mdir, info.init_values[fidx]);
                                    }
                                    cur_byte += msz;
                                }
                                // Tail padding for this struct element
                                if (cur_byte < layout.total_size)
                                    fprintf(out_, "    DB %d DUP(0)\n", layout.total_size - cur_byte);
                            }
                            // If init provides fewer elements than the array size, zero-fill remainder
                            int total_arr_sz = info.type.size > 0 ? info.type.size : 0;
                            int emitted = nElems * layout.total_size;
                            if (total_arr_sz > emitted)
                                fprintf(out_, "    DB %d DUP(0)\n", total_arr_sz - emitted);
                        } else {
                            // Unknown struct layout — fall back to flat DQ emission
                            for (size_t iv = 0; iv < info.init_values.size(); iv++) {
                                bool is_fn = iv < info.init_fn_names.size() && !info.init_fn_names[iv].empty();
                                if (iv == 0) {
                                    if (is_fn) fprintf(out_, "%s DQ OFFSET %s\n", info.global_label.c_str(), info.init_fn_names[iv].c_str());
                                    else fprintf(out_, "%s DQ %lld\n", info.global_label.c_str(), info.init_values[iv]);
                                } else {
                                    if (is_fn) fprintf(out_, "    DQ OFFSET %s\n", info.init_fn_names[iv].c_str());
                                    else fprintf(out_, "    DQ %lld\n", info.init_values[iv]);
                                }
                            }
                        }
                    } else {
                        // Array initializer — emit per element; function-ptr elements use OFFSET.
                        // Non-function elements emitted in chunks of 16 to avoid MASM line-length limit.
                        int elem_sz = info.type.pointee ? info.type.pointee->size : 8;
                        const char* dir = (elem_sz <= 1) ? "DB" : (elem_sz == 2) ? "DW" : (elem_sz <= 4) ? "DD" : "DQ";
                        bool has_fn = false;
                        for (size_t iv = 0; iv < info.init_fn_names.size(); iv++)
                            if (!info.init_fn_names[iv].empty()) { has_fn = true; break; }
                        // If the entire array is zero-initialized, emit DB total_size DUP(0).
                        int total_arr_sz = info.type.size > 0 ? info.type.size : 0;
                        bool is_struct_arr_elem = (info.type.pointee &&
                                                   info.type.pointee->base == CType::Struct &&
                                                   elem_sz > 8);
                        bool all_zeros_single = (info.init_values.size() == 1 && info.init_values[0] == 0 &&
                                                  info.init_fn_names.empty() && info.init_fn_scalar.empty() &&
                                                  total_arr_sz > 8);
                        bool all_zeros_multi = false;
                        if (is_struct_arr_elem && !has_fn && !info.init_values.empty()) {
                            all_zeros_multi = true;
                            for (size_t iv = 0; iv < info.init_values.size(); iv++)
                                if (info.init_values[iv] != 0) { all_zeros_multi = false; break; }
                            if (all_zeros_multi) {
                                for (auto& fn : info.init_fn_names)
                                    if (!fn.empty()) { all_zeros_multi = false; break; }
                            }
                        }
                        if (all_zeros_single || all_zeros_multi) {
                            int emit_sz = total_arr_sz;
                            if (emit_sz <= elem_sz && (int)info.init_values.size() > 1)
                                emit_sz = (int)info.init_values.size() * elem_sz;
                            if (emit_sz > 0)
                                fprintf(out_, "%s DB %d DUP(0)\n",
                                        info.global_label.c_str(), emit_sz);
                            else
                                fprintf(out_, "%s DB %lld DUP(0)\n",
                                        info.global_label.c_str(),
                                        (long long)info.init_values.size() * elem_sz);
                        } else if (has_fn) {
                            // Mixed or all-function-ptr array: emit one element per line
                            for (size_t iv = 0; iv < info.init_values.size(); iv++) {
                                bool is_fn = iv < info.init_fn_names.size() && !info.init_fn_names[iv].empty();
                                if (iv == 0) {
                                    if (is_fn)
                                        fprintf(out_, "%s DQ OFFSET %s\n",
                                                info.global_label.c_str(), info.init_fn_names[iv].c_str());
                                    else
                                        fprintf(out_, "%s %s %lld\n",
                                                info.global_label.c_str(), dir, info.init_values[iv]);
                                } else {
                                    if (is_fn)
                                        fprintf(out_, "    DQ OFFSET %s\n", info.init_fn_names[iv].c_str());
                                    else
                                        fprintf(out_, "    %s %lld\n", dir, info.init_values[iv]);
                                }
                            }
                        } else {
                            // Pure-value array: chunk 16 per line
                            const size_t CHUNK = 16;
                            for (size_t iv = 0; iv < info.init_values.size(); iv += CHUNK) {
                                if (iv == 0)
                                    fprintf(out_, "%s %s ", info.global_label.c_str(), dir);
                                else
                                    fprintf(out_, "    %s ", dir);
                                size_t end = std::min(iv + CHUNK, info.init_values.size());
                                for (size_t jv = iv; jv < end; jv++) {
                                    if (jv > iv) fprintf(out_, ", ");
                                    fprintf(out_, "%lld", info.init_values[jv]);
                                }
                                fprintf(out_, "\n");
                            }
                        }
                    }
                } else {
                    // All scalar globals use DQ (8 bytes) since codegen loads/stores as QWORD
                    // Quality 3.1: scalar function-pointer initializer — emit OFFSET
                    if (!info.init_fn_scalar.empty())
                        fprintf(out_, "%s DQ OFFSET %s\n",
                                info.global_label.c_str(), info.init_fn_scalar.c_str());
                    else
                        fprintf(out_, "%s DQ %lld\n", info.global_label.c_str(), info.init_value);
                }
            }
        }
    }
    // Uninitialized globals (in .data with 0 value, avoids BSS warning)
    {
        bool has_uninit = false;
        for (auto& [name, info] : globals_) {
            if (info.is_global && !info.has_init && !info.is_tls) {
                if (!has_uninit) { fprintf(out_, "\n.data\n"); has_uninit = true; }
                if (info.align_req > 8)
                    fprintf(out_, "ALIGN %d\n", info.align_req);
                // Compute actual size: struct/array types need full allocation
                int alloc_size = 8; // default: pointer/int/long
                if (info.type.base == CType::Struct) {
                    // Try exact struct name first
                    if (!info.type.struct_name.empty()) {
                        auto sit = structs_.find(info.type.struct_name);
                        if (sit == structs_.end()) {
                            // Try with/without "struct " prefix
                            std::string sn = info.type.struct_name;
                            if (sn.substr(0, 7) == "struct ")
                                sit = structs_.find(sn.substr(7));
                            else
                                sit = structs_.find("struct " + sn);
                        }
                        if (sit != structs_.end() && sit->second.total_size > 0)
                            alloc_size = sit->second.total_size;
                    }
                    // Fallback: use type.size if available
                    if (alloc_size <= 8 && info.type.size > 8)
                        alloc_size = info.type.size;
                } else if (info.type.base == CType::Array && info.type.size > 0) {
                    alloc_size = info.type.size;
                } else if (info.type.size > 0 && info.type.size != 8) {
                    alloc_size = info.type.size;
                }
                if (alloc_size <= 8) {
                    fprintf(out_, "%s DQ 0\n", info.global_label.c_str());
                } else {
                    fprintf(out_, "%s DB %d DUP(0)\n", info.global_label.c_str(), alloc_size);
                }
            }
        }
    }

    // P2.3: Once-flag variables for static local first-use initialization guards
    if (!static_once_flags_.empty()) {
        fprintf(out_, "\n; P2.3: first-use once-flags for static locals with non-constant init\n");
        fprintf(out_, ".data\n");
        for (const auto& flag_label : static_once_flags_) {
            fprintf(out_, "%s DB 0\n", flag_label.c_str());
        }
    }

    fprintf(out_, "\nEND\n");
}

void CodeGen::genTranslationUnit(ASTNode* node) {
    for (auto& child : node->children) {
        if (child->kind == NodeKind::FunctionDef)
            genFunctionDef(child.get());
        else if (child->kind == NodeKind::Declaration)
            genDeclaration(child.get(), true);
    }
    // 4.2: Warn about static functions defined but never called
    for (auto& sfn : static_functions_) {
        if (called_functions_.find(sfn) == called_functions_.end()) {
            auto fit = functions_.find(sfn);
            int dline = (fit != functions_.end()) ? fit->second.def_line : 0;
            if (dline > 0)
                fprintf(stderr, "%s%s:%d: warning: static function '%s' defined but not used%s\n",
                        cg_warn(), source_file_.c_str(), dline, sfn.c_str(), cg_reset());
            else
                fprintf(stderr, "%s%s: warning: static function '%s' defined but not used%s\n",
                        cg_warn(), source_file_.c_str(), sfn.c_str(), cg_reset());
        }
    }
}

void CodeGen::genFunctionDef(ASTNode* node) {
    // children[0] = type_spec, children[1] = declarator, children[2] = compound_stmt
    auto* typeNode = node->children[0].get();
    auto* declNode = node->children[1].get();
    auto* bodyNode = node->children[2].get();

    std::string fname = getDeclaratorName(declNode);
    current_func_ = fname;

    // Detect static storage class: functions with static linkage use PROC PRIVATE
    // so that each translation unit can have its own definition without link conflicts.
    bool func_is_static = false;
    for (auto& c : typeNode->children) {
        if (c && c->sval == "static") { func_is_static = true; break; }
    }
    locals_.clear();
    const_locals_.clear();
    locals_used_.clear();
    locals_maybe_unused_.clear();
    param_names_set_.clear();
    locals_decl_line_.clear();
    local_offset_ = 8; // reserve [rbp-8] for saved rbx
    max_align_req_ = 8; // reset per-function alignment requirement

    // Collect parameter info
    FuncInfo fi;
    fi.return_type = resolveType(typeNode);
    // If the declarator has a leading "*" (pointer-return function like "struct S *foo(...)"),
    // wrap the return type in a pointer. Without this, "struct S *foo()" would have
    // return_type = Struct (size 72) instead of Ptr (size 8), triggering wrong large-struct-return.
    // If the declarator has a leading "*" (pointer-return function like "struct S *foo(...)"),
    // wrap the return type in a pointer. Without this, "struct S *foo()" would have
    // return_type = Struct (size 72) instead of Ptr (size 8), triggering wrong large-struct-return.
    if (declNode && declNode->kind == NodeKind::Declarator && declNode->sval == "*") {
        TypeInfo* pe = new TypeInfo(fi.return_type);
        fi.return_type = TypeInfo::make_ptr();
        fi.return_type.pointee = pe;
    }
    // Find the ParamList - it may be nested inside declarator children
    // For pointer-return functions like "char *foo(int x)", the ParamList
    // is nested inside a child declarator, not a direct child of the outer "*" declarator.
    ASTNode* params = nullptr;
    if (isDeclaratorFunc(declNode)) {
        // Search direct children first
        for (auto& c : declNode->children) {
            if (c && c->kind == NodeKind::ParamList) { params = c.get(); break; }
        }
        // If not found, search one level deeper (pointer-return functions)
        if (!params) {
            for (auto& c : declNode->children) {
                if (c && c->kind == NodeKind::Declarator) {
                    for (auto& cc : c->children) {
                        if (cc && cc->kind == NodeKind::ParamList) { params = cc.get(); break; }
                    }
                    if (params) break;
                }
            }
        }
    }
    if (params) {
        for (auto& p : params->children) {
            // C99: Check for variadic "..." marker
            if (p->kind == NodeKind::ParamDecl && p->sval == "...") {
                fi.is_variadic = true;
                continue;
            }
            if (p->kind == NodeKind::ParamDecl && !p->children.empty()) {
                TypeInfo pt = resolveType(p->children[0].get());
                std::string pname;
                if (p->children.size() > 1) {
                    pname = getDeclaratorName(p->children[1].get());
                    // Count pointer levels from the Pointer node's ival
                    auto* pdecl = p->children[1].get();
                    int ptr_levels = 0;
                    // Nameless pointer param (e.g. FuncDef*): Pointer node directly
                    if (pdecl && pdecl->kind == NodeKind::Pointer) {
                        ptr_levels = (int)pdecl->ival;
                        if (ptr_levels < 1) ptr_levels = 1;
                    }
                    // C11 §6.7.6.3p7: array-style param T x[] decays to T *x
                    if (pdecl && pdecl->kind == NodeKind::Declarator && pdecl->sval == "[]") {
                        ptr_levels = 1;
                    }
                    if (pdecl && pdecl->kind == NodeKind::Declarator && pdecl->sval == "*") {
                        // The Pointer child node's ival holds the number of * levels
                        for (auto& pc : pdecl->children) {
                            if (pc->kind == NodeKind::Pointer) {
                                ptr_levels = (int)pc->ival;
                                if (ptr_levels < 1) ptr_levels = 1;
                                break;
                            }
                        }
                        if (ptr_levels == 0) ptr_levels = 1; // fallback
                    }
                    // Build pointer type chain from inside out
                    for (int pl = 0; pl < ptr_levels; pl++) {
                        TypeInfo* pointee = new TypeInfo(pt);
                        pt = TypeInfo::make_ptr();
                        pt.pointee = pointee;
                    }
                }
                fi.param_types.push_back(pt);
                fi.param_names.push_back(pname);
            }
        }
    }
    // C23 [[attribute]] lookup: consume attributes within 3 lines before the function def.
    // Uses take() so each attribute entry is consumed once (prevents bleed to next function).
    {
        int L = node->line;
        std::string dmsg;
        if (AttrRegistry::take(L - 3, L + 1, "nodiscard", dmsg))
            fi.is_nodiscard = true;
        if (AttrRegistry::take(L - 3, L + 1, "deprecated", dmsg)) {
            fi.is_deprecated = true;
            fi.deprecated_msg = dmsg;
        }
        if (AttrRegistry::take(L - 3, L + 1, "noreturn", dmsg))
            fi.is_noreturn = true;
    }
    // 1.1: Struct return > 16 bytes uses hidden pointer (first arg = ptr to result space)
    fi.returns_large_struct = (fi.return_type.base == CType::Struct && fi.return_type.size > 16);
    // 4.2: Record definition line for unused-function warning
    fi.def_line = node->line;
    functions_[fname] = fi;

    // 4.2: Track static functions for unused-fn warning
    if (func_is_static) static_functions_.insert(fname);

    // 1.1: Reset hidden return pointer slot
    hidden_ret_ptr_offset_ = 0;

    // Generate body to a temp buffer so we know the final stack size
    // 1.1: If returns_large_struct, first register (RCX) is the hidden return pointer;
    //      named params shift to start from RDX (index 1 in param_regs).
    int param_reg_start = fi.returns_large_struct ? 1 : 0;
    static const char* param_regs[] = { "rcx", "rdx", "r8", "r9" };
    for (size_t i = 0; i < fi.param_names.size() && (i + param_reg_start) < 4; i++) {
        if (!fi.param_names[i].empty()) {
            // 2.1: Struct params >8B are passed by hidden pointer; allocate full struct space
            int alloc_sz = (fi.param_types[i].base == CType::Struct && fi.param_types[i].size > 8)
                           ? fi.param_types[i].size : 8;
            int off = allocLocal(alloc_sz);
            locals_[fi.param_names[i]] = { fi.param_types[i], off, false, "" };
            param_names_set_.insert(fi.param_names[i]); // W6: track param names
        }
    }
    // 5th+ parameters: allocate local slots; values will be copied from caller's stack
    for (size_t i = 4 - (size_t)param_reg_start; i < fi.param_names.size(); i++) {
        if (!fi.param_names[i].empty()) {
            int off = allocLocal(8);
            locals_[fi.param_names[i]] = { fi.param_types[i], off, false, "" };
            param_names_set_.insert(fi.param_names[i]);
        }
    }

    // Redirect output to temp buffer for body
    FILE* body_buf = reliable_tmpfile();
    if (!body_buf) body_buf = out_; // fallback
    FILE* saved_out = out_;
    out_ = body_buf;

    // 1.1: Save hidden return pointer (RCX) if function returns large struct (>16 bytes)
    if (fi.returns_large_struct) {
        int hslot = allocLocal(8);
        hidden_ret_ptr_offset_ = hslot;
        emit("mov QWORD PTR [rbp%+d], rcx", hslot);
    }

    // Save params to locals — use XMM for float/double parameters (2.1: ABI correctness)
    // If returns_large_struct, named params start at RDX (param_reg_start=1), otherwise RCX.
    static const char* xmm_param_regs[] = { "xmm0", "xmm1", "xmm2", "xmm3" };
    for (size_t i = 0; i < fi.param_names.size() && (i + param_reg_start) < 4; i++) {
        if (!fi.param_names[i].empty()) {
            auto it = locals_.find(fi.param_names[i]);
            if (it != locals_.end()) {
                int ri = (int)i + param_reg_start;
                if (fi.param_types[i].base == CType::Float)
                    emit("movss DWORD PTR [rbp%+d], %s", it->second.stack_offset, xmm_param_regs[ri]);
                else if (fi.param_types[i].base == CType::Double)
                    emit("movsd QWORD PTR [rbp%+d], %s", it->second.stack_offset, xmm_param_regs[ri]);
                else if (fi.param_types[i].base == CType::Struct && fi.param_types[i].size > 8) {
                    // 2.1: Struct >8B: copy from hidden-pointer arg into local slot
                    int sz = fi.param_types[i].size;
                    int off = it->second.stack_offset;
                    emit("mov rbx, %s", param_regs[ri]);  // rbx = source pointer
                    int qwords4 = sz / 8, rem4 = sz % 8;
                    for (int q = 0; q < qwords4; q++) {
                        emit("mov rax, QWORD PTR [rbx+%d]", q * 8);
                        emit("mov QWORD PTR [rbp%+d], rax", off + q * 8);
                    }
                    if (rem4 >= 4) {
                        emit("mov eax, DWORD PTR [rbx+%d]", qwords4 * 8);
                        emit("mov DWORD PTR [rbp%+d], eax", off + qwords4 * 8);
                    }
                } else
                    emit("mov QWORD PTR [rbp%+d], %s", it->second.stack_offset, param_regs[ri]);
            }
        }
    }

    // 5th+ params: copy from caller's stack ([rbp+48], [rbp+56], ...) into local slots.
    // Windows x64: after shadow space [rbp+16..rbp+40], 5th caller arg is at [rbp+48].
    for (size_t i = 4 - (size_t)param_reg_start; i < fi.param_names.size(); i++) {
        if (!fi.param_names[i].empty()) {
            auto it = locals_.find(fi.param_names[i]);
            if (it != locals_.end()) {
                int stack_idx = (int)(i + param_reg_start) - 4;
                int caller_offset = 48 + stack_idx * 8;
                emit("mov rax, QWORD PTR [rbp+%d]", caller_offset);
                emit("mov QWORD PTR [rbp%+d], rax", it->second.stack_offset);
            }
        }
    }

    // C99: For variadic functions, home all register args to shadow space
    // Windows x64 ABI: shadow space at [rbp+16..rbp+40] for args 1-4
    if (fi.is_variadic) {
        emit("mov QWORD PTR [rbp+16], rcx");
        emit("mov QWORD PTR [rbp+24], rdx");
        emit("mov QWORD PTR [rbp+32], r8");
        emit("mov QWORD PTR [rbp+40], r9");
    }

    // Generate body
    genCompoundStmt(bodyNode);

    // C11/C23: [[noreturn]] return-path warning.
    // If the function is marked [[noreturn]] but the body may fall through
    // to the epilogue, warn.  Heuristic: check whether the last statement
    // in the compound body is a ReturnStmt or a call to a known no-return
    // function (exit / abort / _Exit / unreachable).
    if (fi.is_noreturn) {
        bool may_return = true;
        if (!bodyNode->children.empty()) {
            // Walk backwards to find the last statement (skip declarations)
            for (int ci = (int)bodyNode->children.size() - 1; ci >= 0; --ci) {
                auto* last = bodyNode->children[ci].get();
                if (!last) continue;
                if (last->kind == NodeKind::ReturnStmt) { may_return = false; break; }
                if (last->kind == NodeKind::ExprStmt && !last->children.empty()) {
                    auto* expr = last->children[0].get();
                    if (expr && expr->kind == NodeKind::CallExpr && !expr->children.empty()) {
                        auto* callee = expr->children[0].get();
                        if (callee && callee->kind == NodeKind::Ident) {
                            const std::string& fn = callee->sval;
                            if (fn == "exit" || fn == "abort" || fn == "_Exit" ||
                                fn == "_exit" || fn == "unreachable" || fn == "__assume") {
                                may_return = false;
                            }
                            // Also check if the callee is itself [[noreturn]]
                            auto fit = functions_.find(fn);
                            if (fit != functions_.end() && fit->second.is_noreturn)
                                may_return = false;
                        }
                    }
                    if (!may_return) break;
                }
                // Stop at first statement — declaration nodes don't affect control flow here
                if (last->kind != NodeKind::Declaration) break;
            }
        }
        if (may_return)
            fprintf(stderr, "%s%s:%d: warning: [[noreturn]] function '%s' may return%s\n",
                    cg_warn(), source_file_.c_str(), node->line, fname.c_str(), cg_reset());
    }

    // W6: Warn for unused local variables (exclude params and [[maybe_unused]])
    for (auto& kv : locals_) {
        const std::string& vname = kv.first;
        if (param_names_set_.count(vname)) continue;     // skip function parameters
        if (locals_used_.count(vname)) continue;          // skip variables that were read
        if (locals_maybe_unused_.count(vname)) continue; // [[maybe_unused]] suppressed
        int dline = locals_decl_line_.count(vname) ? locals_decl_line_.at(vname) : 0;
        if (dline > 0)
            fprintf(stderr, "%s%s:%d: warning: unused variable '%s'%s\n",
                    cg_warn(), source_file_.c_str(), dline, vname.c_str(), cg_reset());
        else
            fprintf(stderr, "%s%s: warning: unused variable '%s'%s\n",
                    cg_warn(), source_file_.c_str(), vname.c_str(), cg_reset());
    }

    // Implicit return 0 for main
    if (fname == "main") {
        emit("xor eax, eax");
    }

    // Epilogue
    fprintf(out_, "_L%s_ret:\n", fname.c_str());
    emit("lea rsp, [rbp-8]");
    emit("pop rbx");
    emit("pop rbp");
    emit("ret");

    // Switch back to real output
    out_ = saved_out;

    // Now emit prologue with correct stack size
    // +32 for shadow space, +8 to realign after push rbx
    int stack_size = ((local_offset_ + 32 + 15) & ~15) | 8;
    if (fname == "main") {
        fprintf(out_, "PUBLIC main\n");
    }
    // Static functions use PROC PRIVATE (internal linkage) to avoid duplicate-symbol
    // errors when the same static-inline header function is compiled into multiple TUs.
    fprintf(out_, "%s PROC%s\n", fname.c_str(), func_is_static ? " PRIVATE" : "");
    emit("push rbp");
    emit("mov rbp, rsp");
    emit("push rbx");
    emit("sub rsp, %d", stack_size);
    // C11: _Alignas with requirement > 16 — emit additional RSP alignment
    // (rbp-relative variable offsets are already padded; this adjusts RSP for
    //  any code that requires a specific stack pointer alignment, e.g. AVX loads)
    if (max_align_req_ > 16) {
        emit("and rsp, -%d", max_align_req_);
    }

    // Copy body from temp buffer
    if (body_buf != saved_out) {
        fflush(body_buf);
        fseek(body_buf, 0, SEEK_END);
        long bsize = ftell(body_buf);
        fseek(body_buf, 0, SEEK_SET);
        if (bsize > 0) {
            char* bbuf = (char*)malloc(bsize);
            if (bbuf) {
                size_t nread = fread(bbuf, 1, bsize, body_buf);
                fwrite(bbuf, 1, nread, out_);
                free(bbuf);
            }
        }
        cleanup_tmpfile(body_buf);
    }

    fprintf(out_, "%s ENDP\n\n", fname.c_str());
}

void CodeGen::genDeclaration(ASTNode* node, bool global) {
    if (node->children.empty()) return;

    // Register struct layouts when we encounter struct definitions
    ASTNode* typeSpecNode = node->children[0].get();
    if (typeSpecNode) {
        ASTNode* structNode = nullptr;
        if (typeSpecNode->kind == NodeKind::StructDef) structNode = typeSpecNode;
        else if (typeSpecNode->kind == NodeKind::DeclSpecs) {
            for (auto& c : typeSpecNode->children) {
                if (c->kind == NodeKind::StructDef) { structNode = c.get(); break; }
            }
        }
        // For anonymous typedef structs (typedef struct { } Name), extract the typedef name
        // so the layout is registered under a name and member offsets can be found later.
        // Anonymous struct: sval is just "struct" or "union" keyword (no tag appended)
        bool is_anon_struct_node = structNode && (structNode->sval == "struct" || structNode->sval == "union"
            || structNode->sval == "STRUCT" || structNode->sval == "UNION");
        std::string anonTypedefName;
        if (is_anon_struct_node) {
            if (node->children.size() >= 2) {
                ASTNode* declList = node->children[1].get();
                if (declList) {
                    // The declList might be a DeclList, InitDeclarator, or Declarator directly
                    ASTNode* initDecl = nullptr;
                    ASTNode* decl = nullptr;
                    if (declList->kind == NodeKind::InitDeclarator) {
                        initDecl = declList;
                    } else if (declList->kind == NodeKind::Declarator) {
                        decl = declList;
                    } else if (!declList->children.empty()) {
                        initDecl = declList->children[0].get();
                    }
                    if (initDecl && initDecl->kind == NodeKind::InitDeclarator && !initDecl->children.empty())
                        decl = initDecl->children[0].get();
                    else if (initDecl && initDecl->kind == NodeKind::Declarator)
                        decl = initDecl;
                    if (decl) anonTypedefName = getDeclaratorName(decl);
                }
            }
        }
        std::string structLayoutName = structNode ? structNode->sval : "";
        if (structLayoutName.empty() && !anonTypedefName.empty()) structLayoutName = anonTypedefName;
        // Detect anonymous struct: sval is just "struct" or "union" (no tag name appended)
        bool is_anon_struct = structNode && (structNode->sval == "struct" || structNode->sval == "union" || structNode->sval == "STRUCT" || structNode->sval == "UNION");
        if (is_anon_struct && !anonTypedefName.empty()) {
            // Anonymous typedef struct/union — use typedef name as layout name
            // so it goes through the main layout code with full union support.
            structLayoutName = anonTypedefName;
        }
        // Fix: allow re-computation when struct has members (handles forward-declaration followed by full definition)
        bool struct_has_members = structNode && !structNode->children.empty();
        if (structNode && !structLayoutName.empty() && (!is_anon_struct || !anonTypedefName.empty()) && (structs_.find(structLayoutName) == structs_.end() || struct_has_members)) {
            StructLayout layout;
            layout.name = structLayoutName;
            // 5.1: packed if #pragma pack(1) is active
            layout.is_packed = (g_pack_level == 1);
            bool is_union = (layout.name.size() >= 5 &&
                (layout.name.substr(0, 5) == "union" || layout.name.substr(0, 5) == "UNION"))
                || (structNode && (structNode->sval == "union" || structNode->sval == "UNION"));
            int offset = 0;
            // Collect all members (may be nested in StructDeclList)
            std::vector<ASTNode*> allMembers;
            for (auto& child : structNode->children) {
                if (child->kind == NodeKind::StructMember || child->kind == NodeKind::Declaration) {
                    allMembers.push_back(child.get());
                } else if (child->kind == NodeKind::StructDeclList) {
                    for (auto& m : child->children) allMembers.push_back(m.get());
                }
            }
            // Bit-field packing state (per struct, reset per declaration)
            int bf_cur_offset = -1;  // byte offset of current bitfield storage unit (-1 = none)
            int bf_bits_used  = 0;   // bits consumed in current storage unit
            int bf_unit_size  = 0;   // size of current storage unit in bytes
            for (auto* member : allMembers) {
                if (member->kind == NodeKind::StructMember || member->kind == NodeKind::Declaration) {
                    std::string mname;
                    ASTNode* mdnode_raw = nullptr;
                    if (member->children.size() > 1) {
                        auto* mdnode = member->children[1].get();
                        // Unwrap DeclList if present
                        if (mdnode->kind == NodeKind::DeclList && !mdnode->children.empty())
                            mdnode = mdnode->children[0].get();
                        mdnode_raw = mdnode;
                        mname = getDeclaratorName(mdnode);
                    }
                    if (mname.empty() && !member->children.empty())
                        mname = member->sval;

                    // Detect bit-field: sval ends with ":bitfield"
                    static const std::string BF_SUFFIX = ":bitfield";
                    bool is_bitfield = false;
                    int bit_width = 0;
                    if (mname.size() >= BF_SUFFIX.size() &&
                        mname.substr(mname.size() - BF_SUFFIX.size()) == BF_SUFFIX) {
                        is_bitfield = true;
                        mname = mname.substr(0, mname.size() - BF_SUFFIX.size());
                        // Bit width is the last child of the declarator
                        if (mdnode_raw && !mdnode_raw->children.empty()) {
                            bit_width = (int)evalConstExpr(mdnode_raw->children.back().get());
                        }
                    }

                    // C11: Anonymous struct/union — promote its fields into parent namespace.
                    // Also handles injected names (__rcc_anon_*) produced by the preprocessor
                    // for anonymous struct members that lack a declarator in the source.
                    bool is_injected_anon = (mname.size()>=10 && mname.compare(0,10,"__rcc_anon")==0);
                    if ((mname.empty() || is_injected_anon) && !is_bitfield) {
                        // Try to find the StructDef node for the anonymous member
                        ASTNode* anonNode = member->children.empty() ? nullptr : member->children[0].get();
                        ASTNode* anonDef = nullptr;
                        if (anonNode) {
                            if (anonNode->kind == NodeKind::StructDef) {
                                anonDef = anonNode;
                            } else if (anonNode->kind == NodeKind::DeclSpecs) {
                                for (auto& cc : anonNode->children)
                                    if (cc->kind == NodeKind::StructDef) { anonDef = cc.get(); break; }
                            }
                        }
                        if (!anonDef) continue; // truly empty, nothing to promote
                        bool inner_is_union = (!anonDef->sval.empty() &&
                            (anonDef->sval.size() >= 5 &&
                             (anonDef->sval.substr(0,5) == "union" || anonDef->sval.substr(0,5) == "UNION")));
                        // Collect anonymous struct's member AST nodes
                        std::vector<ASTNode*> innerMembers;
                        for (auto& ch : anonDef->children) {
                            if (ch->kind == NodeKind::StructMember || ch->kind == NodeKind::Declaration)
                                innerMembers.push_back(ch.get());
                            else if (ch->kind == NodeKind::StructDeclList)
                                for (auto& m : ch->children) innerMembers.push_back(m.get());
                        }
                        if (innerMembers.empty()) continue;
                        // Determine alignment of the anonymous struct (max of member aligns)
                        int inner_align = 1;
                        for (auto* im : innerMembers) {
                            if (im->children.empty()) continue;
                            TypeInfo it = resolveType(im->children[0].get());
                            int a = it.size; if (a > 8) a = 8; if (a > inner_align) inner_align = a;
                        }
                        // Align outer offset to inner struct's requirements
                        int inner_base = is_union ? 0 : ((inner_align > 0) ? (offset + inner_align - 1) & ~(inner_align - 1) : offset);
                        if (!is_union) offset = inner_base;
                        // Process each inner member inline
                        int inner_off = 0; // byte offset within the anonymous struct
                        int inner_total = 0;
                        static const std::string BF_SFX = ":bitfield";
                        for (auto* im : innerMembers) {
                            if (im->kind != NodeKind::StructMember && im->kind != NodeKind::Declaration) continue;
                            std::string imname;
                            if (im->children.size() > 1) {
                                auto* imd = im->children[1].get();
                                if (imd && imd->kind == NodeKind::DeclList && !imd->children.empty())
                                    imd = imd->children[0].get();
                                if (imd) imname = getDeclaratorName(imd);
                            }
                            if (imname.empty()) imname = im->sval;
                            bool im_bf = (imname.size() >= BF_SFX.size() &&
                                imname.substr(imname.size() - BF_SFX.size()) == BF_SFX);
                            if (im_bf) imname = imname.substr(0, imname.size() - BF_SFX.size());
                            if (imname.empty()) continue; // nested anonymous — skip for now
                            TypeInfo imtype = TypeInfo::make_int();
                            if (!im->children.empty()) imtype = resolveType(im->children[0].get());
                            // Check for pointer or array declarator
                            if (im->children.size() > 1) {
                                auto* imd2 = im->children[1].get();
                                if (imd2 && imd2->kind == NodeKind::DeclList && !imd2->children.empty())
                                    imd2 = imd2->children[0].get();
                                if (imd2 && imd2->kind == NodeKind::InitDeclarator && !imd2->children.empty())
                                    imd2 = imd2->children[0].get();
                                if (imd2 && imd2->kind == NodeKind::Declarator && imd2->sval == "*") {
                                    TypeInfo* pe = new TypeInfo(imtype);
                                    imtype = TypeInfo::make_ptr(); imtype.pointee = pe;
                                    // Check for array child: Type *name[N]
                                    for (auto& pch : imd2->children) {
                                        if (pch && pch->kind == NodeKind::Declarator &&
                                            (pch->sval == "[N]" || pch->sval == "[]")) {
                                            int atotal = 1;
                                            ASTNode* ac = pch.get();
                                            while (ac && ac->kind == NodeKind::Declarator &&
                                                   (ac->sval == "[N]" || ac->sval == "[]")) {
                                                if (ac->sval == "[N]" && ac->children.size() > 1) {
                                                    int adim = (int)evalConstExpr(ac->children.back().get());
                                                    if (adim > 0) atotal *= adim;
                                                }
                                                ac = !ac->children.empty() ? ac->children[0].get() : nullptr;
                                            }
                                            if (atotal > 1) {
                                                int aesz = imtype.size > 0 ? imtype.size : 8;
                                                TypeInfo* aet = new TypeInfo(imtype);
                                                imtype.base = CType::Array;
                                                imtype.size = atotal * aesz;
                                                imtype.pointee = aet;
                                            }
                                            break;
                                        }
                                    }
                                }
                                // Handle function pointer in anonymous struct: "()" wrapping "*"
                                if (imd2 && imd2->kind == NodeKind::Declarator && imd2->sval == "()") {
                                    for (auto& dc2 : imd2->children) {
                                        if (dc2 && dc2->kind == NodeKind::Declarator && dc2->sval == "*") {
                                            TypeInfo* pe2 = new TypeInfo(imtype);
                                            imtype = TypeInfo::make_ptr(); imtype.pointee = pe2;
                                            break;
                                        }
                                    }
                                }
                                // Handle array member in anonymous struct
                                if (imd2 && imd2->kind == NodeKind::Declarator &&
                                    (imd2->sval == "[N]" || imd2->sval == "[]")) {
                                    int atotal = 1;
                                    ASTNode* ac = imd2;
                                    while (ac && ac->kind == NodeKind::Declarator &&
                                           (ac->sval == "[N]" || ac->sval == "[]")) {
                                        if (ac->sval == "[N]" && ac->children.size() > 1) {
                                            int adim = (int)evalConstExpr(ac->children.back().get());
                                            if (adim > 0) atotal *= adim;
                                        }
                                        if (!ac->children.empty())
                                            ac = ac->children[0].get();
                                        else
                                            break;
                                    }
                                    // Check if innermost declarator is a pointer (count all * levels incl Pointer nodes)
                                    if (ac && ac->kind == NodeKind::Declarator && ac->sval == "*") {
                                        int pl = 1;
                                        for (auto& sch : ac->children) {
                                            if (sch && sch->kind == NodeKind::Pointer) { pl = (int)sch->ival; if (pl < 1) pl = 1; break; }
                                            if (sch && sch->kind == NodeKind::Declarator && sch->sval == "*") { pl++; break; }
                                        }
                                        for (int pi = 0; pi < pl; pi++) {
                                            TypeInfo* pe = new TypeInfo(imtype);
                                            imtype = TypeInfo::make_ptr(); imtype.pointee = pe;
                                        }
                                    }
                                    if (ac && ac->kind == NodeKind::Declarator && ac->sval == "()") {
                                        for (auto& dc2 : ac->children) {
                                            if (dc2 && dc2->kind == NodeKind::Declarator && dc2->sval == "*") {
                                                TypeInfo* pe = new TypeInfo(imtype);
                                                imtype = TypeInfo::make_ptr(); imtype.pointee = pe; break;
                                            }
                                        }
                                    }
                                    if (atotal > 1) {
                                        int aesz = imtype.size > 0 ? imtype.size : 8;
                                        TypeInfo* aet = new TypeInfo(imtype);
                                        imtype.base = CType::Array;
                                        imtype.size = atotal * aesz;
                                        imtype.pointee = aet;
                                    }
                                }
                            }
                            // Duplicate field check
                            bool dup = false;
                            for (auto& ex : layout.members) {
                                if (ex.name == imname) {
                                    fprintf(stderr, "error: anonymous struct member '%s' "
                                            "conflicts with existing field in outer struct\n",
                                            imname.c_str());
                                    dup = true; break;
                                }
                            }
                            if (dup) continue;
                            // Compute member offset within anonymous struct (skip bitfields for now)
                            if (!im_bf) {
                                int a = imtype.size; if (a > 8) a = 8;
                                if (!inner_is_union && a > 0)
                                    inner_off = (inner_off + a - 1) & ~(a - 1);
                                StructMemberInfo mi;
                                mi.name   = imname;
                                mi.type   = imtype;
                                mi.offset = inner_is_union ? inner_base : (inner_base + inner_off);
                                layout.members.push_back(mi);
                                int imsize = imtype.size > 0 ? imtype.size : 8;
                                if (inner_is_union) {
                                    if (imsize > inner_total) inner_total = imsize;
                                } else {
                                    inner_off += imsize;
                                    inner_total = inner_off;
                                }
                            }
                        }
                        // Advance outer offset by the anonymous struct/union's footprint
                        if (is_union) {
                            if (inner_total > offset) offset = inner_total;
                        } else {
                            offset = inner_base + inner_total;
                        }
                        continue;
                    }
                    // Skip truly empty names (e.g., unnamed non-bitfield padding with no struct body)
                    if (mname.empty() && !is_bitfield) continue;

                    // Non-bitfield resets the bitfield packing state
                    if (!is_bitfield) {
                        bf_cur_offset = -1;
                        bf_bits_used  = 0;
                        bf_unit_size  = 0;
                    }

                    // Resolve actual member type
                    TypeInfo mtype = TypeInfo::make_int();
                    if (!member->children.empty())
                        mtype = resolveType(member->children[0].get());
                    // Check if member is a pointer or array
                    if (member->children.size() > 1) {
                        auto* mdecl = member->children[1].get();
                        // Unwrap DeclList if present
                        if (mdecl && mdecl->kind == NodeKind::DeclList && !mdecl->children.empty())
                            mdecl = mdecl->children[0].get();
                        // Unwrap InitDeclarator if present
                        if (mdecl && mdecl->kind == NodeKind::InitDeclarator && !mdecl->children.empty())
                            mdecl = mdecl->children[0].get();
                        if (mdecl && mdecl->kind == NodeKind::Declarator && mdecl->sval == "*") {
                            // Count pointer levels. The Pointer child node's ival = total * count.
                            // E.g. T *p → Pointer ival=1 → 1 level; T **p → Pointer ival=2 → 2 levels.
                            int ptr_levels = 1;
                            for (auto& sch : mdecl->children) {
                                if (sch && sch->kind == NodeKind::Pointer) {
                                    ptr_levels = (int)sch->ival;
                                    if (ptr_levels < 1) ptr_levels = 1;
                                    break;
                                }
                                if (sch && sch->kind == NodeKind::Declarator && sch->sval == "*") {
                                    ptr_levels++;
                                    break;
                                }
                            }
                            for (int pl = 0; pl < ptr_levels; pl++) {
                                TypeInfo* pointee = new TypeInfo(mtype);
                                mtype = TypeInfo::make_ptr();
                                mtype.pointee = pointee;
                            }
                            // Check if pointer has an array child: Type *name[N]
                            // AST: Declarator "*" has children: [Ident name, Declarator "[N]"]
                            for (auto& pch : mdecl->children) {
                                if (pch && pch->kind == NodeKind::Declarator &&
                                    (pch->sval == "[N]" || pch->sval == "[]")) {
                                    int total_elems = 1;
                                    ASTNode* acur = pch.get();
                                    while (acur && acur->kind == NodeKind::Declarator &&
                                           (acur->sval == "[N]" || acur->sval == "[]")) {
                                        if (acur->sval == "[N]" && acur->children.size() > 1) {
                                            int dim = (int)evalConstExpr(acur->children.back().get());
                                            if (dim > 0) total_elems *= dim;
                                        }
                                        if (!acur->children.empty())
                                            acur = acur->children[0].get();
                                        else
                                            break;
                                    }
                                    if (total_elems >= 1) {
                                        int elem_sz = mtype.size > 0 ? mtype.size : 8;
                                        TypeInfo* elem_type = new TypeInfo(mtype);
                                        mtype.base = CType::Array;
                                        mtype.size = total_elems * elem_sz;
                                        mtype.pointee = elem_type;
                                    }
                                    break;
                                }
                            }
                        }
                        // Handle function pointer member: declarator sval == "()" with a child "*"
                        // e.g. int (*xSize)(void*) — the outer declarator is "()" wrapping a "*"
                        // All function pointers are 8 bytes regardless of return type.
                        if (mdecl && mdecl->kind == NodeKind::Declarator && mdecl->sval == "()") {
                            for (auto& dc : mdecl->children) {
                                if (dc && dc->kind == NodeKind::Declarator && dc->sval == "*") {
                                    TypeInfo* pointee = new TypeInfo(mtype);
                                    mtype = TypeInfo::make_ptr();
                                    mtype.pointee = pointee;
                                    break;
                                }
                            }
                        }
                        // Handle array member: declarator sval == "[N]" or "[]"
                        // Walk the declarator chain collecting all array dimensions
                        if (mdecl && mdecl->kind == NodeKind::Declarator &&
                            (mdecl->sval == "[N]" || mdecl->sval == "[]")) {
                            int total_elems = 1;
                            ASTNode* acur = mdecl;
                            while (acur && acur->kind == NodeKind::Declarator &&
                                   (acur->sval == "[N]" || acur->sval == "[]")) {
                                if (acur->sval == "[N]" && acur->children.size() > 1) {
                                    int dim = (int)evalConstExpr(acur->children.back().get());
                                    if (dim > 0) total_elems *= dim;
                                }
                                // Move to inner declarator (children[0])
                                if (!acur->children.empty())
                                    acur = acur->children[0].get();
                                else
                                    break;
                            }
                            // Check if innermost declarator after array dims is a pointer
                            if (acur && acur->kind == NodeKind::Declarator && acur->sval == "*") {
                                TypeInfo* pointee = new TypeInfo(mtype);
                                mtype = TypeInfo::make_ptr();
                                mtype.pointee = pointee;
                            }
                            // Also check for function pointer: declarator "()" wrapping "*"
                            if (acur && acur->kind == NodeKind::Declarator && acur->sval == "()") {
                                for (auto& dc : acur->children) {
                                    if (dc && dc->kind == NodeKind::Declarator && dc->sval == "*") {
                                        TypeInfo* pointee = new TypeInfo(mtype);
                                        mtype = TypeInfo::make_ptr();
                                        mtype.pointee = pointee;
                                        break;
                                    }
                                }
                            }
                            if (total_elems >= 1) {
                                int elem_sz = mtype.size > 0 ? mtype.size : 8;
                                TypeInfo* elem_type = new TypeInfo(mtype);
                                mtype.base = CType::Array;
                                mtype.size = total_elems * elem_sz;
                                mtype.pointee = elem_type;
                            }
                        }
                    }

                    if (is_bitfield) {
                        // Determine storage unit size from underlying type
                        int unit_sz = mtype.size > 0 ? mtype.size : 4;
                        if (unit_sz > 8) unit_sz = 8;
                        int unit_bits = unit_sz * 8;
                        // Start a new storage unit if none active, doesn't fit, or type changed
                        if (bf_cur_offset < 0 ||
                            bf_bits_used + bit_width > unit_bits ||
                            bf_unit_size != unit_sz) {
                            if (!is_union) {
                                // Align to unit_sz boundary
                                if (unit_sz > 1)
                                    offset = (offset + unit_sz - 1) & ~(unit_sz - 1);
                                bf_cur_offset = offset;
                                offset += unit_sz;
                            } else {
                                bf_cur_offset = 0;
                            }
                            bf_bits_used = 0;
                            bf_unit_size = unit_sz;
                        }
                        // Only add named bit-fields to the layout (unnamed are just padding)
                        if (!mname.empty()) {
                            StructMemberInfo mi;
                            mi.name       = mname;
                            mi.type       = mtype;
                            mi.offset     = bf_cur_offset;
                            mi.bit_offset = bf_bits_used;
                            mi.bit_width  = bit_width;
                            layout.members.push_back(mi);
                        }
                        bf_bits_used += bit_width;
                        if (is_union) {
                            int msz = unit_sz;
                            if (msz > offset) offset = msz;
                        }
                    } else if (!mname.empty()) {
                        StructMemberInfo mi;
                        mi.name = mname;
                        mi.type = mtype;
                        if (is_union) {
                            mi.offset = 0; // all union members at offset 0
                            layout.members.push_back(mi);
                            int msz = mtype.size > 0 ? mtype.size : 8;
                            if (msz > offset) offset = msz; // track max size
                        } else {
                            // Align offset to element alignment (for arrays, use element size)
                            if (!layout.is_packed) {
                                int align = mtype.size;
                                // For array types, align to element size, not total array size
                                if (mtype.base == CType::Array && mtype.pointee)
                                    align = mtype.pointee->size;
                                if (align > 8) align = 8;
                                if (align > 0) offset = (offset + align - 1) & ~(align - 1);
                            }
                            mi.offset = offset;
                            layout.members.push_back(mi);
                            offset += mtype.size > 0 ? mtype.size : 8;
                        }
                    }
                    // Multi-declarator: int a, b, c; — process remaining DeclList entries
                    if (!is_bitfield && member->children.size() > 1) {
                        auto* dlist = member->children[1].get();
                        if (dlist && dlist->kind == NodeKind::DeclList && dlist->children.size() > 1) {
                            TypeInfo base_mtype = TypeInfo::make_int();
                            if (!member->children.empty())
                                base_mtype = resolveType(member->children[0].get());
                            for (size_t di = 1; di < dlist->children.size(); di++) {
                                auto* xdecl = dlist->children[di].get();
                                if (!xdecl) continue;
                                // Unwrap InitDeclarator
                                if (xdecl->kind == NodeKind::InitDeclarator && !xdecl->children.empty())
                                    xdecl = xdecl->children[0].get();
                                std::string xname = getDeclaratorName(xdecl);
                                if (xname.empty()) continue;
                                // Apply pointer/array modifiers from xdecl
                                TypeInfo xtype = base_mtype;
                                if (xdecl && xdecl->kind == NodeKind::Declarator && xdecl->sval == "*") {
                                    // Count pointer levels incl Pointer nodes
                                    int xpl = 1;
                                    for (auto& xsch : xdecl->children) {
                                        if (xsch && xsch->kind == NodeKind::Pointer) { xpl = (int)xsch->ival; if (xpl < 1) xpl = 1; break; }
                                        if (xsch && xsch->kind == NodeKind::Declarator && xsch->sval == "*") { xpl++; break; }
                                    }
                                    for (int xpi = 0; xpi < xpl; xpi++) {
                                        TypeInfo* pe = new TypeInfo(xtype);
                                        xtype = TypeInfo::make_ptr(); xtype.pointee = pe;
                                    }
                                    // Check for array child: Type *name[N]
                                    for (auto& pch : xdecl->children) {
                                        if (pch && pch->kind == NodeKind::Declarator &&
                                            (pch->sval == "[N]" || pch->sval == "[]")) {
                                            int total_elems = 1;
                                            ASTNode* acur = pch.get();
                                            while (acur && acur->kind == NodeKind::Declarator &&
                                                   (acur->sval == "[N]" || acur->sval == "[]")) {
                                                if (acur->sval == "[N]" && acur->children.size() > 1)
                                                    total_elems *= (int)evalConstExpr(acur->children.back().get());
                                                acur = !acur->children.empty() ? acur->children[0].get() : nullptr;
                                            }
                                            if (total_elems >= 1) {
                                                int esz = xtype.size > 0 ? xtype.size : 8;
                                                TypeInfo* et = new TypeInfo(xtype);
                                                xtype.base = CType::Array;
                                                xtype.size = total_elems * esz;
                                                xtype.pointee = et;
                                            }
                                            break;
                                        }
                                    }
                                } else if (xdecl && xdecl->kind == NodeKind::Declarator && xdecl->sval == "()") {
                                    for (auto& dc : xdecl->children) {
                                        if (dc && dc->kind == NodeKind::Declarator && dc->sval == "*") {
                                            TypeInfo* pe = new TypeInfo(xtype);
                                            xtype = TypeInfo::make_ptr(); xtype.pointee = pe; break;
                                        }
                                    }
                                } else if (xdecl && xdecl->kind == NodeKind::Declarator &&
                                           (xdecl->sval == "[N]" || xdecl->sval == "[]")) {
                                    int total_elems = 1;
                                    ASTNode* acur = xdecl;
                                    while (acur && acur->kind == NodeKind::Declarator &&
                                           (acur->sval == "[N]" || acur->sval == "[]")) {
                                        if (acur->sval == "[N]" && acur->children.size() > 1)
                                            total_elems *= (int)evalConstExpr(acur->children.back().get());
                                        acur = !acur->children.empty() ? acur->children[0].get() : nullptr;
                                    }
                                    // Check if innermost declarator is a pointer
                                    if (acur && acur->kind == NodeKind::Declarator && acur->sval == "*") {
                                        TypeInfo* pe = new TypeInfo(xtype);
                                        xtype = TypeInfo::make_ptr(); xtype.pointee = pe;
                                    }
                                    if (acur && acur->kind == NodeKind::Declarator && acur->sval == "()") {
                                        for (auto& dc : acur->children) {
                                            if (dc && dc->kind == NodeKind::Declarator && dc->sval == "*") {
                                                TypeInfo* pe = new TypeInfo(xtype);
                                                xtype = TypeInfo::make_ptr(); xtype.pointee = pe; break;
                                            }
                                        }
                                    }
                                    if (total_elems >= 1) {
                                        int esz = xtype.size > 0 ? xtype.size : 8;
                                        TypeInfo* et = new TypeInfo(xtype);
                                        xtype.base = CType::Array;
                                        xtype.size = total_elems * esz;
                                        xtype.pointee = et;
                                    }
                                }
                                StructMemberInfo xmi;
                                xmi.name = xname;
                                xmi.type = xtype;
                                if (is_union) {
                                    xmi.offset = 0;
                                    layout.members.push_back(xmi);
                                    int msz = xtype.size > 0 ? xtype.size : 8;
                                    if (msz > offset) offset = msz;
                                } else {
                                    if (!layout.is_packed) {
                                        int align = xtype.size;
                                        if (xtype.base == CType::Array && xtype.pointee)
                                            align = xtype.pointee->size;
                                        if (align > 8) align = 8;
                                        if (align > 0) offset = (offset + align - 1) & ~(align - 1);
                                    }
                                    xmi.offset = offset;
                                    layout.members.push_back(xmi);
                                    offset += xtype.size > 0 ? xtype.size : 8;
                                }
                            }
                        }
                    }
                }
            }
            // Find max alignment for tail padding (skip if packed)
            int max_align = 1;
            if (!layout.is_packed) {
                for (auto& m : layout.members) {
                    int a = m.type.size;
                    // For array types, alignment is based on element size
                    if (m.type.base == CType::Array && m.type.pointee)
                        a = m.type.pointee->size;
                    if (a > 8) a = 8;
                    if (a > max_align) max_align = a;
                }
            }
            layout.total_size = offset > 0 ? ((offset + max_align - 1) & ~(max_align - 1)) : max_align;
            structs_[layout.name] = layout;
        }
    }

    // Register enum constants
    {
        ASTNode* enumNode = nullptr;
        if (typeSpecNode->kind == NodeKind::EnumDef) enumNode = typeSpecNode;
        else if (typeSpecNode->kind == NodeKind::DeclSpecs) {
            for (auto& c : typeSpecNode->children) {
                if (c->kind == NodeKind::EnumDef) { enumNode = c.get(); break; }
            }
        }
        if (enumNode) {
            long long counter = 0;
            for (auto& child : enumNode->children) {
                if (child->kind == NodeKind::EnumList) {
                    for (auto& e : child->children) {
                        if (e->kind == NodeKind::Enumerator) {
                            std::string ename = e->sval;
                            if (!e->children.empty()) {
                                counter = evalConstExpr(e->children[0].get());
                            }
                            if (!ename.empty()) {
                                enum_constants_[ename] = counter;
                            }
                            counter++;
                        }
                    }
                } else if (child->kind == NodeKind::Enumerator) {
                    std::string ename = child->sval;
                    if (!child->children.empty()) {
                        counter = evalConstExpr(child->children[0].get());
                    }
                    if (!ename.empty()) {
                        enum_constants_[ename] = counter;
                    }
                    counter++;
                }
            }
        }
    }

    // Handle typedef declarations
    {
        bool is_typedef = false;
        if (typeSpecNode->kind == NodeKind::DeclSpecs) {
            for (auto& c : typeSpecNode->children) {
                if (c->kind == NodeKind::StorageClassSpec &&
                    (c->sval == "typedef" || c->sval == "TYPEDEF")) {
                    is_typedef = true;
                    break;
                }
            }
        }
        if (is_typedef) {
            TypeInfo type = resolveType(node->children[0].get());
            // Re-find structNode for anonymous typedef struct registration
            ASTNode* structNode2 = nullptr;
            if (typeSpecNode->kind == NodeKind::StructDef) structNode2 = typeSpecNode;
            else if (typeSpecNode->kind == NodeKind::DeclSpecs) {
                for (auto& c2 : typeSpecNode->children)
                    if (c2->kind == NodeKind::StructDef) { structNode2 = c2.get(); break; }
            }
            if (node->children.size() >= 2) {
                auto* declList = node->children[1].get();
                if (declList) {
                    for (auto& initDecl : declList->children) {
                        ASTNode* decl = nullptr;
                        if (initDecl->kind == NodeKind::InitDeclarator && !initDecl->children.empty())
                            decl = initDecl->children[0].get();
                        else if (initDecl->kind == NodeKind::Declarator)
                            decl = initDecl.get();
                        if (decl) {
                            std::string tname = getDeclaratorName(decl);
                            if (!tname.empty()) {
                                TypeInfo ttype = type;
                                // Check if pointer typedef
                                if (decl->sval == "*") {
                                    TypeInfo* pointee = new TypeInfo(ttype);
                                    ttype = TypeInfo::make_ptr();
                                    ttype.pointee = pointee;
                                }
                                // For typedef struct { } Name: if the struct is anonymous (empty sval)
                                // and not yet registered, register it now under the typedef name.
                                // Anonymous struct: sval is just "struct" or "union" (no tag name appended)
                                bool is_anon2 = structNode2 && (structNode2->sval == "struct" || structNode2->sval == "union"
                                    || structNode2->sval == "STRUCT" || structNode2->sval == "UNION");
                                if (is_anon2 && decl->sval != "*" && structs_.find(tname) == structs_.end()) {
                                    // Build struct layout from the anonymous structNode2
                                    StructLayout layout;
                                    layout.name = tname;
                                    bool is_union = (structNode2->sval == "union" || structNode2->sval == "UNION");
                                    int off = 0;
                                    std::vector<ASTNode*> allMem;
                                    for (size_t ci = 0; ci < structNode2->children.size(); ci++) {
                                        auto* ch = structNode2->children[ci].get();
                                        if (ch->kind == NodeKind::StructMember || ch->kind == NodeKind::Declaration)
                                            allMem.push_back(ch);
                                        else if (ch->kind == NodeKind::StructDeclList) {
                                            for (size_t di = 0; di < ch->children.size(); di++)
                                                allMem.push_back(ch->children[di].get());
                                        }
                                    }
                                    for (auto* mem : allMem) {
                                        if (mem->kind != NodeKind::StructMember && mem->kind != NodeKind::Declaration) continue;
                                        std::string mname;
                                        if (mem->children.size() > 1) {
                                            auto* md = mem->children[1].get();
                                            if (md && md->kind == NodeKind::DeclList && !md->children.empty())
                                                md = md->children[0].get();
                                            if (md) mname = getDeclaratorName(md);
                                        }
                                        if (mname.empty()) mname = mem->sval;
                                        if (mname.empty()) continue;
                                        TypeInfo mtype = TypeInfo::make_int();
                                        if (!mem->children.empty()) mtype = resolveType(mem->children[0].get());
                                        // Handle pointer or array member declarator
                                        if (mem->children.size() > 1) {
                                            auto* md2 = mem->children[1].get();
                                            if (md2 && md2->kind == NodeKind::DeclList && !md2->children.empty())
                                                md2 = md2->children[0].get();
                                            if (md2 && md2->kind == NodeKind::InitDeclarator && !md2->children.empty())
                                                md2 = md2->children[0].get();
                                            if (md2 && md2->kind == NodeKind::Declarator && md2->sval == "*") {
                                                // Count all pointer levels incl Pointer nodes
                                                { int pl2 = 1;
                                                for (auto& sch2 : md2->children) {
                                                    if (sch2 && sch2->kind == NodeKind::Pointer) { pl2 = (int)sch2->ival; if (pl2 < 1) pl2 = 1; break; }
                                                    if (sch2 && sch2->kind == NodeKind::Declarator && sch2->sval == "*") { pl2++; break; }
                                                }
                                                for (int pi2 = 0; pi2 < pl2; pi2++) { TypeInfo* pe = new TypeInfo(mtype); mtype = TypeInfo::make_ptr(); mtype.pointee = pe; } }
                                                // Check for array child: Type *name[N]
                                                for (auto& pch : md2->children) {
                                                    if (pch && pch->kind == NodeKind::Declarator &&
                                                        (pch->sval == "[N]" || pch->sval == "[]")) {
                                                        int atot = 1;
                                                        ASTNode* acp = pch.get();
                                                        while (acp && acp->kind == NodeKind::Declarator &&
                                                               (acp->sval == "[N]" || acp->sval == "[]")) {
                                                            if (acp->sval == "[N]" && acp->children.size() > 1) {
                                                                int adm = (int)evalConstExpr(acp->children.back().get());
                                                                if (adm > 0) atot *= adm;
                                                            }
                                                            acp = !acp->children.empty() ? acp->children[0].get() : nullptr;
                                                        }
                                                        if (atot > 1) {
                                                            int aesz = mtype.size > 0 ? mtype.size : 8;
                                                            TypeInfo* aet = new TypeInfo(mtype);
                                                            mtype.base = CType::Array;
                                                            mtype.size = atot * aesz;
                                                            mtype.pointee = aet;
                                                        }
                                                        break;
                                                    }
                                                }
                                            }
                                            // Handle function pointer in typedef struct: "()" wrapping "*"
                                            if (md2 && md2->kind == NodeKind::Declarator && md2->sval == "()") {
                                                for (auto& dc3 : md2->children) {
                                                    if (dc3 && dc3->kind == NodeKind::Declarator && dc3->sval == "*") {
                                                        TypeInfo* pe3 = new TypeInfo(mtype);
                                                        mtype = TypeInfo::make_ptr();
                                                        mtype.pointee = pe3;
                                                        break;
                                                    }
                                                }
                                            }
                                            // Handle array member in typedef struct
                                            if (md2 && md2->kind == NodeKind::Declarator &&
                                                (md2->sval == "[N]" || md2->sval == "[]")) {
                                                int atot = 1;
                                                ASTNode* acp = md2;
                                                while (acp && acp->kind == NodeKind::Declarator &&
                                                       (acp->sval == "[N]" || acp->sval == "[]")) {
                                                    if (acp->sval == "[N]" && acp->children.size() > 1) {
                                                        int adm = (int)evalConstExpr(acp->children.back().get());
                                                        if (adm > 0) atot *= adm;
                                                    }
                                                    if (!acp->children.empty())
                                                        acp = acp->children[0].get();
                                                    else
                                                        break;
                                                }
                                                // Check if innermost declarator is a pointer (count all * levels incl Pointer nodes)
                                                if (acp && acp->kind == NodeKind::Declarator && acp->sval == "*") {
                                                    int pl3 = 1;
                                                    for (auto& sch3 : acp->children) {
                                                        if (sch3 && sch3->kind == NodeKind::Pointer) { pl3 = (int)sch3->ival; if (pl3 < 1) pl3 = 1; break; }
                                                        if (sch3 && sch3->kind == NodeKind::Declarator && sch3->sval == "*") { pl3++; break; }
                                                    }
                                                    for (int pi3 = 0; pi3 < pl3; pi3++) { TypeInfo* pe = new TypeInfo(mtype); mtype = TypeInfo::make_ptr(); mtype.pointee = pe; }
                                                }
                                                if (acp && acp->kind == NodeKind::Declarator && acp->sval == "()") {
                                                    for (auto& dc3 : acp->children) {
                                                        if (dc3 && dc3->kind == NodeKind::Declarator && dc3->sval == "*") {
                                                            TypeInfo* pe = new TypeInfo(mtype);
                                                            mtype = TypeInfo::make_ptr(); mtype.pointee = pe; break;
                                                        }
                                                    }
                                                }
                                                if (atot > 1) {
                                                    int aesz2 = mtype.size > 0 ? mtype.size : 8;
                                                    TypeInfo* aet2 = new TypeInfo(mtype);
                                                    mtype.base = CType::Array;
                                                    mtype.size = atot * aesz2;
                                                    mtype.pointee = aet2;
                                                }
                                            }
                                        }
                                        StructMemberInfo mi;
                                        mi.name = mname; mi.type = mtype;
                                        if (is_union) {
                                            mi.offset = 0;
                                            layout.members.push_back(mi);
                                            int msz = mtype.size > 0 ? mtype.size : 8;
                                            if (msz > off) off = msz;
                                        } else {
                                            int align = mtype.size; if (align > 8) align = 8; if (align < 1) align = 1;
                                            off = (off + align - 1) & ~(align - 1);
                                            mi.offset = off;
                                            layout.members.push_back(mi);
                                            off += mtype.size > 0 ? mtype.size : 8;
                                        }
                                        // Multi-declarator: int a, b; — process remaining DeclList entries
                                        if (mem->children.size() > 1) {
                                            auto* xdl = mem->children[1].get();
                                            if (xdl && xdl->kind == NodeKind::DeclList && xdl->children.size() > 1) {
                                                TypeInfo base_t = TypeInfo::make_int();
                                                if (!mem->children.empty()) base_t = resolveType(mem->children[0].get());
                                                for (size_t xdi = 1; xdi < xdl->children.size(); xdi++) {
                                                    auto* xd = xdl->children[xdi].get();
                                                    if (!xd) continue;
                                                    if (xd->kind == NodeKind::InitDeclarator && !xd->children.empty())
                                                        xd = xd->children[0].get();
                                                    std::string xn = getDeclaratorName(xd);
                                                    if (xn.empty()) continue;
                                                    TypeInfo xt = base_t;
                                                    if (xd->kind == NodeKind::Declarator && xd->sval == "*") {
                                                        TypeInfo* pe = new TypeInfo(xt); xt = TypeInfo::make_ptr(); xt.pointee = pe;
                                                    } else if (xd->kind == NodeKind::Declarator && xd->sval == "()") {
                                                        for (auto& dc4 : xd->children) {
                                                            if (dc4 && dc4->kind == NodeKind::Declarator && dc4->sval == "*") {
                                                                TypeInfo* pe = new TypeInfo(xt); xt = TypeInfo::make_ptr(); xt.pointee = pe; break;
                                                            }
                                                        }
                                                    }
                                                    StructMemberInfo xmi;
                                                    xmi.name = xn; xmi.type = xt;
                                                    if (is_union) {
                                                        xmi.offset = 0;
                                                        layout.members.push_back(xmi);
                                                        int xsz = xt.size > 0 ? xt.size : 8;
                                                        if (xsz > off) off = xsz;
                                                    } else {
                                                        int xa = xt.size; if (xa > 8) xa = 8; if (xa < 1) xa = 1;
                                                        off = (off + xa - 1) & ~(xa - 1);
                                                        xmi.offset = off;
                                                        layout.members.push_back(xmi);
                                                        off += xt.size > 0 ? xt.size : 8;
                                                    }
                                                }
                                            }
                                        }
                                    }
                                    int mx = 1;
                                    for (auto& m : layout.members) { int a = m.type.size; if (a > 8) a = 8; if (a > mx) mx = a; }
                                    layout.total_size = off > 0 ? ((off + mx - 1) & ~(mx - 1)) : mx;
                                    structs_[tname] = layout;
                                    ttype = TypeInfo::make_struct(tname, layout.total_size);
                                } else {
                                    // If this typedef names an already-registered anonymous struct,
                                    // use the correct size from the layout.
                                    auto sit = structs_.find(tname);
                                    if (sit != structs_.end() && ttype.base == CType::Struct && ttype.size <= 8) {
                                        ttype = TypeInfo::make_struct(tname, sit->second.total_size);
                                    }
                                }
                                typedefs_[tname] = ttype;
                            }
                        }
                    }
                }
            }
            return; // typedef doesn't allocate storage
        }
    }

    // Check for static/extern/auto storage class, constexpr, and _Alignas
    bool is_static = false;
    bool is_tls = false;  // C11: _Thread_local
    bool is_extern = false;
    bool is_auto_inference = false;  // C23: auto for type inference / C#-style var
    bool is_constexpr = false;       // C23: constexpr
    bool has_type_spec = false;      // Whether there's an explicit type specifier
    int  align_req = 8;              // C11: _Alignas alignment requirement
    if (typeSpecNode->kind == NodeKind::DeclSpecs) {
        for (auto& c : typeSpecNode->children) {
            if (c->kind == NodeKind::StorageClassSpec) {
                if (c->sval == "static") is_static = true;
                else if (c->sval == "extern") is_extern = true;
                else if (c->sval == "auto" || c->sval == "var") is_auto_inference = true;
                else if (c->sval == "thread_local") {
                    // P4-C: -std= enforcement for _Thread_local (C11 feature)
                    if (g_std_level < 11)
                        fprintf(stderr, "%s%s:%d: warning: '_Thread_local' is a C11 feature"
                                " (use -std=c11 or later)%s\n",
                                cg_warn(), source_file_.c_str(), node->line, cg_reset());
                    is_tls = true;    // C11: emit to _TLS segment
                    is_static = true; // TLS vars are implicitly static (global storage duration)
                }
            }
            if (c->kind == NodeKind::TypeQualifier) {
                // C23: constexpr is lexed as const but with original text preserved
                if (c->sval == "constexpr") {
                    // P4-C: -std= enforcement for constexpr (C23 feature)
                    if (g_std_level < 23)
                        fprintf(stderr, "%s%s:%d: warning: 'constexpr' is a C23 feature"
                                " (use -std=c23)%s\n",
                                cg_warn(), source_file_.c_str(), node->line, cg_reset());
                    is_constexpr = true;
                }
            }
            if (c->kind == NodeKind::TypeSpec) {
                has_type_spec = true;
                // P4-C: -std= enforcement for _Alignas (C11 feature)
                if (c->sval == "_Alignas" && g_std_level < 11)
                    fprintf(stderr, "%s%s:%d: warning: '_Alignas' is a C11 feature"
                            " (use -std=c11 or later)%s\n",
                            cg_warn(), source_file_.c_str(), node->line, cg_reset());
                // C11: _Alignas — enforce alignment; warn only if > 16 (beyond rbp guarantee)
                if (c->sval == "_Alignas" && !c->children.empty()) {
                    auto* alignArg = c->children[0].get();
                    if (alignArg && alignArg->kind == NodeKind::IntLit && alignArg->ival > 1) {
                        long long N = alignArg->ival;
                        // Validate: must be a power of 2
                        if ((N & (N - 1)) == 0) {
                            align_req = (int)N;
                            if (N > 16) {
                                // rbp is only 16-byte aligned; > 16 is best-effort
                                fprintf(stderr, "%s: warning: _Alignas(%lld) cannot be enforced"
                                        " (rbp alignment limit is 16 bytes); best-effort applied\n",
                                        source_file_.c_str(), N);
                            }
                            // N <= 16: silently enforced via padded local_offset_
                        } else {
                            fprintf(stderr, "%s: warning: _Alignas(%lld): not a power of 2;"
                                    " specifier ignored\n", source_file_.c_str(), N);
                        }
                    }
                }
            }
        }
    }

    // C23: auto type inference - if 'auto' is used without a type specifier
    // and there's an initializer, infer the type from the initializer
    TypeInfo type = TypeInfo::make_int();
    if (is_auto_inference && !has_type_spec) {
        // Need to find the initializer to infer type
        if (node->children.size() >= 2) {
            auto* declList = node->children[1].get();
            if (declList && !declList->children.empty()) {
                auto& firstDecl = declList->children[0];
                if (firstDecl->kind == NodeKind::InitDeclarator && firstDecl->children.size() > 1) {
                    // Has initializer - infer type
                    type = inferTypeFromExpr(firstDecl->children[1].get());
                }
            }
        }
    } else {
        type = resolveType(node->children[0].get());
    }

    if (node->children.size() < 2) return; // forward struct decl etc

    auto* declList = node->children[1].get();
    if (!declList) return;

    for (auto& initDecl : declList->children) {
        if (initDecl->kind != NodeKind::InitDeclarator) continue;
        if (initDecl->children.empty()) continue;

        auto* decl = initDecl->children[0].get();
        std::string name = getDeclaratorName(decl);
        if (name.empty()) continue;

        // Function prototype: register in functions_ for arg-count checking.
        // Only applies to direct "()" declarators at global scope (not function pointers).
        // Use isDeclaratorFunc to detect function prototypes regardless of pointer return type.
        // For void* malloc(size_t), the outer declarator sval is "*" but isDeclaratorFunc
        // returns true because its immediate child is "()".
        // For void (*fp)(void) (fn-ptr variable), isDeclaratorFunc returns false because
        // the immediate child of "*" is another "*", not "()".
        if (isDeclaratorFunc(decl) && global) {
            if (functions_.find(name) == functions_.end()) {
                FuncInfo pfi;
                pfi.return_type = type;
                // Collect ParamList from declarator (same search as genFunctionDef)
                ASTNode* params = nullptr;
                for (auto& c : decl->children)
                    if (c && c->kind == NodeKind::ParamList) { params = c.get(); break; }
                if (!params) {
                    for (auto& c : decl->children) {
                        if (c && c->kind == NodeKind::Declarator) {
                            for (auto& cc : c->children)
                                if (cc && cc->kind == NodeKind::ParamList) { params = cc.get(); break; }
                            if (params) break;
                        }
                    }
                }
                if (params) {
                    for (auto& p : params->children) {
                        if (p->kind == NodeKind::ParamDecl && p->sval == "...") {
                            pfi.is_variadic = true; continue;
                        }
                        if (p->kind == NodeKind::ParamDecl && !p->children.empty()) {
                            TypeInfo pt = resolveType(p->children[0].get());
                            if (p->children.size() > 1) {
                                auto* pdecl = p->children[1].get();
                                int ptr_levels = 0;
                                // Nameless pointer param (e.g. FuncDef*): Pointer node directly
                                if (pdecl && pdecl->kind == NodeKind::Pointer) {
                                    ptr_levels = (int)pdecl->ival;
                                    if (ptr_levels < 1) ptr_levels = 1;
                                }
                                // C11 §6.7.6.3p7: array-style param T x[] decays to T *x
                                if (pdecl && pdecl->kind == NodeKind::Declarator && pdecl->sval == "[]") {
                                    ptr_levels = 1;
                                }
                                if (pdecl && pdecl->kind == NodeKind::Declarator && pdecl->sval == "*") {
                                    for (auto& pc : pdecl->children)
                                        if (pc->kind == NodeKind::Pointer) { ptr_levels = (int)pc->ival; if (ptr_levels < 1) ptr_levels = 1; break; }
                                    if (ptr_levels == 0) ptr_levels = 1;
                                }
                                for (int pl = 0; pl < ptr_levels; pl++) {
                                    TypeInfo* pointee = new TypeInfo(pt);
                                    pt = TypeInfo::make_ptr();
                                    pt.pointee = pointee;
                                }
                            }
                            pfi.param_types.push_back(pt);
                        }
                    }
                    // C99 §6.7.6.3p10: f(void) declares a function taking no arguments.
                    // If the only registered param is plain void, clear the list.
                    if (pfi.param_types.size() == 1 &&
                        pfi.param_types[0].base == CType::Void &&
                        pfi.param_types[0].pointee == nullptr)
                        pfi.param_types.clear();
                }
                functions_[name] = pfi;
            }
            continue; // No code or allocation for a function prototype
        }

        // C23: constexpr handling - store value in compile-time constant table
        if (is_constexpr && initDecl->children.size() > 1) {
            auto* initNode = initDecl->children[1].get();
            if (initNode->kind == NodeKind::InitializerList) {
                // GAP-6: C23 §6.7.1 — constexpr array with integer element initializers.
                // Fold all integer elements into constexpr_array_values_ so subscript
                // expressions like arr[0] can be evaluated at compile time.
                // GAP-D: C23 §6.7.1 — constexpr struct: fold integer members into
                // constexpr_struct_values_ so member access can be evaluated at compile time.
                if (type.base == CType::Struct && !type.struct_name.empty()) {
                    auto sit = structs_.find(type.struct_name);
                    if (sit != structs_.end()) {
                        std::map<std::string, long long> member_vals;
                        bool all_const = true;
                        size_t nc = std::min(sit->second.members.size(), initNode->children.size());
                        for (size_t mi = 0; mi < nc; mi++) {
                            auto* ch = initNode->children[mi].get();
                            if (!isConstExpr(ch)) { all_const = false; break; }
                            member_vals[sit->second.members[mi].name] = evalConstExpr(ch);
                        }
                        if (all_const && !member_vals.empty())
                            constexpr_struct_values_[name] = std::move(member_vals);
                        else if (!all_const)
                            // Quality 3.6: constexpr enforcement — struct elements must be constants
                            fprintf(stderr, "%s%s:%d: warning: constexpr struct '%s' has "
                                    "non-constant member initializer%s\n",
                                    cg_warn(), source_file_.c_str(), node->line,
                                    name.c_str(), cg_reset());
                    }
                } else {
                    std::vector<long long> arr_vals;
                    bool all_const = true;
                    for (auto& ch : initNode->children) {
                        if (!isConstExpr(ch.get())) { all_const = false; break; }
                        arr_vals.push_back(evalConstExpr(ch.get()));
                    }
                    if (all_const && !arr_vals.empty())
                        constexpr_array_values_[name] = std::move(arr_vals);
                    else if (!all_const)
                        // Quality 3.6: constexpr enforcement — array elements must be constants
                        fprintf(stderr, "%s%s:%d: warning: constexpr array '%s' has "
                                "non-constant element initializer%s\n",
                                cg_warn(), source_file_.c_str(), node->line,
                                name.c_str(), cg_reset());
                }
            } else if (isConstExpr(initNode)) {
                // Detect whether initializer involves floating-point values
                bool has_float = false;
                std::function<bool(ASTNode*)> check = [&](ASTNode* n) -> bool {
                    if (!n) return false;
                    if (n->kind == NodeKind::FloatLit) return true;
                    for (auto& c : n->children) if (check(c.get())) return true;
                    return false;
                };
                has_float = check(initNode) ||
                            (type.base == CType::Float || type.base == CType::Double);
                if (has_float) {
                    // C23: store float/double constexpr in dedicated map so it can
                    // participate in other constexpr float initializers.
                    double fval = evalConstExprDouble(initNode);
                    constexpr_float_values_[name] = fval;
                } else {
                    long long val = evalConstExpr(initNode);
                    constexpr_values_[name] = val;
                }
                // Also register as a normal variable so sizeof etc. work
            } else {
                fprintf(stderr, "%s:%d:%d: warning: constexpr variable '%s' must have constant initializer\n",
                        source_file_.c_str(), node->line, node->col, name.c_str());
            }
        }

        // Check if pointer or array
        TypeInfo vtype = type;
        bool is_array = false;
        int array_size = 0;
        {
            // Count pointer levels from the Pointer node's ival
            int ptr_levels = 0;
            if (decl->sval == "*") {
                for (auto& dc : decl->children) {
                    if (dc->kind == NodeKind::Pointer) {
                        ptr_levels = (int)dc->ival;
                        if (ptr_levels < 1) ptr_levels = 1;
                        break;
                    }
                }
                if (ptr_levels == 0) ptr_levels = 1;
            }
            for (int pl = 0; pl < ptr_levels; pl++) {
                TypeInfo* pointee = new TypeInfo(vtype);
                vtype = TypeInfo::make_ptr();
                vtype.pointee = pointee;
            }
        }
        bool is_vla = false;  // C99: Variable-Length Array
        ASTNode* vla_size_expr = nullptr;
        std::vector<int> array_dims;  // Multi-dim array dimensions
        if (decl->sval == "[N]" && decl->children.size() > 1) {
            is_array = true;
            // Collect all dimensions from nested [N] declarators
            ASTNode* cur = decl;
            while (cur && cur->sval == "[N]" && cur->children.size() > 1) {
                if (cur->children[1] && isConstExpr(cur->children[1].get())) {
                    array_dims.push_back((int)evalConstExpr(cur->children[1].get()));
                } else if (cur->children[1]) {
                    // C99 VLA for the outermost dimension
                    is_vla = true;
                    vla_size_expr = cur->children[1].get();
                    break;
                }
                cur = cur->children[0].get();
            }
            // Dimensions are collected outermost-to-innermost (reverse of logical order)
            std::reverse(array_dims.begin(), array_dims.end());
            // Compute total array size (product of all dimensions)
            array_size = 1;
            for (int d : array_dims) array_size *= d;
        }
        if (decl->sval == "[]") is_array = true;
        // Fix: for "char *arr[]" the outer decl is "*" but array marker is in children.
        // Check if any child (or the decl itself) has the "[]" array marker.
        if (!is_array && decl->sval == "*") {
            for (auto& dc : decl->children) {
                if (dc && (dc->sval == "[]" || dc->sval == "[N]")) {
                    is_array = true;
                    if (dc->sval == "[N]" && dc->children.size() > 1 && isConstExpr(dc->children[1].get()))
                        array_size = (int)evalConstExpr(dc->children[1].get());
                    break;
                }
            }
        }

        if (global || (is_static && !global)) {
            // Skip function declarations (extern prototypes)
            if (isDeclaratorFunc(decl)) continue;
            // Set up array type for globals
            // Fix: also handle [] (unspecified size) arrays � array_size == 0 here,
            // but we still must mark type as Array so lea is used instead of mov,
            // and DB/DD/DQ is chosen by element size rather than defaulting to DQ.
            if (is_array) {
                int elem_sz = vtype.size > 0 ? vtype.size : 8;
                TypeInfo* elem_type = new TypeInfo(vtype);
                vtype.base = CType::Array;
                vtype.size = (array_size > 0) ? (array_size * elem_sz) : elem_sz;
                vtype.pointee = elem_type;
                vtype.array_dims = array_dims;
            }
            std::string label = is_tls   ? ("__tls_" + name) :
                               global   ? ("_" + name) :
                               ("__static_" + current_func_ + "_" + name);
            VarInfo gvar = { vtype, 0, true, label };
            gvar.align_req = align_req; // C11: propagate _Alignas requirement
            gvar.is_tls = is_tls;      // C11: _Thread_local flag
            // Check for initializer
            if (initDecl->children.size() > 1) {
                auto* initNode = initDecl->children[1].get();
                gvar.has_init = true;
                if (initNode->kind == NodeKind::InitializerList) {
                    // Determine if this is an array-of-struct init (flatten nested lists)
                    // vs a plain struct init (don't flatten nested lists)
                    bool is_array_of_struct = is_array && vtype.base == CType::Array &&
                        vtype.pointee && vtype.pointee->base == CType::Struct;
                    // Helper lambda: process a single init element
                    // depth: 0=top-level, 1+=inside nested list
                    std::function<void(ASTNode*, int)> processInitElem = [&](ASTNode* elem, int depth) {
                        if (!elem) { gvar.init_fn_names.push_back(""); gvar.init_values.push_back(0); return; }
                        if (elem->kind == NodeKind::StrLit) {
                            int cw = 1;
                            if (!elem->sval.empty() && elem->sval[0] == 'L') cw = 2;
                            else if (!elem->sval.empty() && elem->sval[0] == 'U') cw = 4;
                            std::string str_lbl = addStringLiteral(elem->sval, cw);
                            gvar.init_fn_names.push_back(str_lbl);
                            gvar.init_values.push_back(0);
                        } else if (elem->kind == NodeKind::Ident &&
                            !constexpr_values_.count(elem->sval) &&
                            !constexpr_float_values_.count(elem->sval) &&
                            !enum_constants_.count(elem->sval) &&
                            !const_locals_.count(elem->sval)) {
                            // Check if this ident refers to a known global variable (e.g. static const char[])
                            auto git = findGlobal(elem->sval);
                            if (git != globals_.end()) {
                                gvar.init_fn_names.push_back(git->second.global_label);
                            } else {
                                gvar.init_fn_names.push_back(elem->sval);
                                called_functions_.insert(elem->sval); // ensure EXTERN emitted
                            }
                            gvar.init_values.push_back(0); // placeholder
                        } else if (elem->kind == NodeKind::InitializerList) {
                            if (is_array_of_struct && depth == 0) {
                                // Array of struct: top-level nested list = one struct element → flatten
                                for (auto& sch : elem->children)
                                    processInitElem(sch.get(), depth + 1);
                            } else if (is_array_of_struct && depth >= 1) {
                                // Deeper nesting (union/embedded struct inside array element)
                                // Take first value or treat as single 0
                                if (!elem->children.empty())
                                    processInitElem(elem->children[0].get(), depth + 1);
                                else {
                                    gvar.init_fn_names.push_back("");
                                    gvar.init_values.push_back(0);
                                }
                            } else {
                                // Simple struct init: nested list = embedded struct/union member → single value
                                gvar.init_fn_names.push_back("");
                                gvar.init_values.push_back(evalConstExpr(elem));
                            }
                        } else if (elem->kind == NodeKind::CastExpr && !elem->children.empty()) {
                            // Cast expression in initializer: check if inner is a function name
                            // e.g. (sqlite3_syscall_ptr)GetSystemInfo → DQ OFFSET GetSystemInfo
                            ASTNode* inner = elem->children.back().get();
                            if (inner && inner->kind == NodeKind::Ident &&
                                !constexpr_values_.count(inner->sval) &&
                                !enum_constants_.count(inner->sval)) {
                                auto git = findGlobal(inner->sval);
                                if (git != globals_.end()) {
                                    gvar.init_fn_names.push_back(git->second.global_label);
                                } else {
                                    gvar.init_fn_names.push_back(inner->sval);
                                    called_functions_.insert(inner->sval);
                                }
                                gvar.init_values.push_back(0);
                            } else {
                                gvar.init_fn_names.push_back("");
                                gvar.init_values.push_back(evalConstExpr(elem));
                            }
                        } else {
                            gvar.init_fn_names.push_back("");
                            gvar.init_values.push_back(evalConstExpr(elem));
                        }
                    };
                    for (auto& ch : initNode->children)
                        processInitElem(ch.get(), 0);
                } else if (initNode->kind == NodeKind::StrLit) {
                    // C11/R3: global string pointer init — register the literal
                    // Fix: char arr[] = "..." inlines bytes; char *p = "..." uses DQ OFFSET
                    int char_width = 1;
                    const std::string& sv = initNode->sval;
                    if (!sv.empty() && sv[0] == 'L') char_width = 2;
                    else if (!sv.empty() && sv[0] == 'U') char_width = 4;
                    if (is_array && char_width == 1) {
                        // char arr[] = "..." -> inline bytes (not DQ OFFSET)
                        auto str_bytes = parseStringLiteralToBytes(sv);
                        for (int sbyte : str_bytes) gvar.init_values.push_back(sbyte);
                        gvar.init_fn_names.assign(gvar.init_values.size(), "");
                    } else {
                        gvar.init_str_label = addStringLiteral(sv, char_width);
                    }
                } else {
                    // Quality 3.1: scalar function-pointer initializer — detect function name.
                    if (initNode->kind == NodeKind::Ident &&
                        !constexpr_values_.count(initNode->sval) &&
                        !constexpr_float_values_.count(initNode->sval) &&
                        !enum_constants_.count(initNode->sval) &&
                        !const_locals_.count(initNode->sval)) {
                        gvar.init_fn_scalar = initNode->sval;
                        called_functions_.insert(initNode->sval); // ensure EXTERN emitted
                        gvar.init_value = 0; // placeholder
                    } else {
                        gvar.init_value = evalConstExpr(initNode);
                    }
                }
            }
            // Fix: for [] (unsized) arrays, update type.size from initializer count
            if (is_array && array_size == 0 && gvar.has_init &&
                !gvar.init_values.empty() && gvar.type.base == CType::Array) {
                int elem_sz = gvar.type.pointee ? gvar.type.pointee->size : 0;
                if (elem_sz <= 0) elem_sz = (gvar.type.size > 0 ? gvar.type.size : 8);
                // For struct elements, look up actual struct size
                if (gvar.type.pointee && gvar.type.pointee->base == CType::Struct &&
                    !gvar.type.pointee->struct_name.empty()) {
                    auto sit = structs_.find(gvar.type.pointee->struct_name);
                    if (sit != structs_.end()) elem_sz = sit->second.total_size;
                }
                int n_init = (int)gvar.init_values.size();
                // For array-of-struct: n_elements = n_init / members_per_struct
                if (gvar.type.pointee && gvar.type.pointee->base == CType::Struct &&
                    !gvar.type.pointee->struct_name.empty()) {
                    auto sit = structs_.find(gvar.type.pointee->struct_name);
                    if (sit != structs_.end() && !sit->second.members.empty()) {
                        int nmembers = (int)sit->second.members.size();
                        n_init = n_init / nmembers;
                    }
                }
                gvar.type.size = n_init * elem_sz;
            }
            globals_[(!global && is_static) ? label : name] = gvar;

            // P2.3: Static local with non-constant initializer — emit first-use guard.
            // Emit a BYTE once-flag in .data and a check-and-initialize sequence
            // at this point in the function body.  Single-threaded correct; avoids
            // double-initialization on repeated calls to the enclosing function.
            if (!global && is_static && initDecl->children.size() > 1) {
                auto* initN = initDecl->children[1].get();
                if (!isConstExpr(initN) &&
                    initN->kind != NodeKind::InitializerList &&
                    initN->kind != NodeKind::StrLit) {
                    std::string once_lbl = "__once_" + label;
                    static_once_flags_.insert(once_lbl);
                    int skip = newLabel();
                    emit("; P2.3: first-use guard for static '%s'", name.c_str());
                    emit("cmp BYTE PTR [%s], 0", once_lbl.c_str());
                    emit("jne .L%d", skip);
                    genExpr(initN);
                    int vsz = vtype.size > 0 ? vtype.size : 8;
                    if (vsz == 1)      emit("mov BYTE PTR [%s], al",  label.c_str());
                    else if (vsz == 4) emit("mov DWORD PTR [%s], eax", label.c_str());
                    else               emit("mov QWORD PTR [%s], rax", label.c_str());
                    emit("mov BYTE PTR [%s], 1", once_lbl.c_str());
                    emitLabel(skip);
                }
            }
        } else {
            int alloc_size = 8;
            int off = 0;

            // C99 VLA: Variable-Length Array handling
            if (is_vla && vla_size_expr) {
                // VLA: allocate dynamically at runtime
                int elem_sz = vtype.size > 0 ? vtype.size : 8;
                TypeInfo* elem_type = new TypeInfo(vtype);
                vtype.base = CType::Array;
                vtype.pointee = elem_type;
                // VLA size is determined at runtime, store as 0
                vtype.size = 0;

                // Generate code to compute array size
                genExpr(vla_size_expr);
                // rax = number of elements
                // Calculate byte size: rax * elem_sz
                if (elem_sz == 8) {
                    emit("shl rax, 3");      // * 8
                } else if (elem_sz == 4) {
                    emit("shl rax, 2");      // * 4
                } else if (elem_sz == 2) {
                    emit("shl rax, 1");      // * 2
                }
                // Align to 16 bytes: (size + 15) & ~15
                emit("add rax, 15");
                emit("and rax, -16");
                // Allocate stack space: sub rsp, rax
                emit("sub rsp, rax");
                // Save the array base pointer to a local slot
                off = allocLocal(8);  // slot to store VLA pointer
                emit("mov QWORD PTR [rbp%+d], rsp", off);
                // The local stores the pointer to the array, not the array itself
                vtype.base = CType::Ptr;
                vtype.size = 8;
                locals_[name] = { vtype, off, false, "" };
            } else if (is_array && array_size > 0) {
                int elem_sz = vtype.size > 0 ? vtype.size : 8;
                TypeInfo* elem_type = new TypeInfo(vtype);
                alloc_size = array_size * elem_sz;
                vtype.base = CType::Array;
                vtype.size = alloc_size;
                vtype.pointee = elem_type;
                vtype.array_dims = array_dims;
                off = allocLocalAligned(alloc_size, align_req);
                locals_[name] = { vtype, off, false, "" };
            } else if (vtype.base == CType::Struct && !vtype.struct_name.empty()) {
                auto sit = structs_.find(vtype.struct_name);
                if (sit != structs_.end() && sit->second.total_size > 0)
                    alloc_size = sit->second.total_size;
                off = allocLocalAligned(alloc_size, align_req);
                locals_[name] = { vtype, off, false, "" };
            } else {
                off = allocLocalAligned(alloc_size, align_req);
                locals_[name] = { vtype, off, false, "" };
            }

            // W6: Track declaration line and [[maybe_unused]] attribute
            if (!global) {
                locals_decl_line_[name] = node->line;
                std::string mu_msg;
                if (AttrRegistry::take(node->line - 2, node->line + 1, "maybe_unused", mu_msg))
                    locals_maybe_unused_.insert(name);
            }

            // If initializer present
            if (initDecl->children.size() > 1) {
                auto* initNode = initDecl->children[1].get();

                // N3: Constant propagation — track scalar integer locals with constant inits.
                // Only for locals (not globals) of integer/char/long type.
                if (!global &&
                    vtype.base != CType::Array && vtype.base != CType::Struct &&
                    vtype.base != CType::Float && vtype.base != CType::Double &&
                    vtype.base != CType::Ptr && isConstExpr(initNode)) {
                    const_locals_[name] = evalConstExpr(initNode);
                }

                // C23 §6.7.10: empty initializer {} — value-initialize to zero
                if (initNode->kind == NodeKind::InitializerList && initNode->children.empty()) {
                    int zi = 0;
                    while (zi + 8 <= alloc_size) { emit("mov QWORD PTR [rbp%+d], 0", off + zi); zi += 8; }
                    if (alloc_size - zi >= 4) { emit("mov DWORD PTR [rbp%+d], 0", off + zi); zi += 4; }
                    while (zi < alloc_size)   { emit("mov BYTE PTR [rbp%+d], 0",  off + zi); zi++;    }
                // Struct initializer list: struct S s = { a, b, c }
                // Zero-init the entire struct first, then set each member from initializer list.
                } else if (vtype.base == CType::Struct && initNode->kind == NodeKind::InitializerList) {
                    // Step 1: zero-initialize entire struct
                    int zi = 0;
                    while (zi + 8 <= alloc_size) { emit("mov QWORD PTR [rbp%+d], 0", off + zi); zi += 8; }
                    if (alloc_size - zi >= 4)     { emit("mov DWORD PTR [rbp%+d], 0", off + zi); zi += 4; }
                    while (zi < alloc_size)        { emit("mov BYTE PTR [rbp%+d], 0",  off + zi); zi++;   }
                    // Step 2: initialize each member from initializer list
                    auto sit = structs_.find(vtype.struct_name);
                    if (sit != structs_.end()) {
                        for (size_t mi = 0; mi < initNode->children.size() && mi < sit->second.members.size(); mi++) {
                            auto* initChild = initNode->children[mi].get();
                            // Handle nested initializer list (e.g., { 0, 0, 0 } for a sub-struct)
                            if (initChild->kind == NodeKind::InitializerList) {
                                // Nested struct: find sub-struct layout
                                auto& mem = sit->second.members[mi];
                                auto subsit = structs_.find(mem.type.struct_name);
                                if (subsit != structs_.end()) {
                                    for (size_t smi = 0; smi < initChild->children.size() && smi < subsit->second.members.size(); smi++) {
                                        genExpr(initChild->children[smi].get());
                                        int moff = off + mem.offset + subsit->second.members[smi].offset;
                                        int msz = subsit->second.members[smi].type.size;
                                        if (msz == 1) emit("mov BYTE PTR [rbp%+d], al", moff);
                                        else if (msz == 4) emit("mov DWORD PTR [rbp%+d], eax", moff);
                                        else emit("mov QWORD PTR [rbp%+d], rax", moff);
                                    }
                                }
                                continue;
                            }
                            genExpr(initChild);
                            int moff = off + sit->second.members[mi].offset;
                            int msz = sit->second.members[mi].type.size;
                            if (msz == 1) emit("mov BYTE PTR [rbp%+d], al", moff);
                            else if (msz == 4) emit("mov DWORD PTR [rbp%+d], eax", moff);
                            else emit("mov QWORD PTR [rbp%+d], rax", moff);
                        }
                    }
                // Array initializer list: int arr[] = {1,2,3}
                } else if (vtype.base == CType::Array && initNode->kind == NodeKind::InitializerList) {
                    int elem_sz = vtype.pointee ? vtype.pointee->size : 8;
                    for (size_t idx = 0; idx < initNode->children.size(); idx++) {
                        genExpr(initNode->children[idx].get());
                        int eoff = off + (int)(idx * elem_sz);
                        if (elem_sz == 1) emit("mov BYTE PTR [rbp%+d], al", eoff);
                        else if (elem_sz == 4) emit("mov DWORD PTR [rbp%+d], eax", eoff);
                        else emit("mov QWORD PTR [rbp%+d], rax", eoff);
                    }
                } else {
                genExpr(initNode);
                if (vtype.base == CType::Float) {
                    if (!last_expr_is_float_) {
                        emit("cvtsi2ss xmm0, eax");
                    }
                    emit("movss DWORD PTR [rbp%+d], xmm0", off);
                } else if (vtype.base == CType::Double) {
                    if (!last_expr_is_float_) {
                        emit("cvtsi2sd xmm0, rax");
                    }
                    emit("movsd QWORD PTR [rbp%+d], xmm0", off);
                } else if (vtype.base == CType::Struct && last_expr_is_ptr_) {
                    // Struct copy: rax has source address, copy to [rbp+off]
                    int copy_size = 8;
                    if (!vtype.struct_name.empty()) {
                        auto sit = structs_.find(vtype.struct_name);
                        if (sit != structs_.end()) copy_size = sit->second.total_size;
                    }
                    emit("mov rsi, rax"); // source
                    emit("lea rdi, [rbp%+d]", off); // dest
                    for (int b = 0; b + 8 <= copy_size; b += 8) {
                        emit("mov rcx, QWORD PTR [rsi+%d]", b);
                        emit("mov QWORD PTR [rdi+%d], rcx", b);
                    }
                    int rem = copy_size % 8;
                    int base = copy_size - rem;
                    for (int b = 0; b < rem; b++) {
                        emit("mov cl, BYTE PTR [rsi+%d]", base + b);
                        emit("mov BYTE PTR [rdi+%d], cl", base + b);
                    }
                } else {
                    if (last_expr_is_float_) {
                        if (last_float_size_ == 8) emit("cvttsd2si rax, xmm0");
                        else emit("cvttss2si eax, xmm0");
                    }
                    emit("mov QWORD PTR [rbp%+d], rax", off);
                }
                } // close else (non-array-init)
            }
        }
    }
}

void CodeGen::genStatement(ASTNode* node) {
    if (!node) return;
    // P4-A: debug info — emit source-location comment before each statement
    if (debug_info_ && node->line > 0 && node->line != last_debug_line_) {
        last_debug_line_ = node->line;
        fprintf(out_, "    ; %s:%d\n", source_file_.c_str(), node->line);
    }
    switch (node->kind) {
    case NodeKind::CompoundStmt: genCompoundStmt(node); break;
    case NodeKind::IfStmt: genIfStmt(node); break;
    case NodeKind::WhileStmt: genWhileStmt(node); break;
    case NodeKind::ForStmt: genForStmt(node); break;
    case NodeKind::ReturnStmt: genReturnStmt(node); break;
    case NodeKind::ExprStmt: genExprStmt(node); break;
    case NodeKind::BreakStmt:
        if (break_label_ >= 0) emit("jmp _L%d", break_label_);
        break;
    case NodeKind::ContinueStmt:
        if (continue_label_ >= 0) emit("jmp _L%d", continue_label_);
        break;
    case NodeKind::DoWhileStmt:
    {
        int top = newLabel();
        int cond = newLabel();
        int end = newLabel();
        int old_break = break_label_;
        int old_cont = continue_label_;
        break_label_ = end;
        continue_label_ = cond;  // continue jumps to condition, not top
        emitLabel(top);
        genStatement(node->children[0].get()); // body
        emitLabel(cond);
        genExpr(node->children[1].get());       // condition
        emit("test rax, rax");
        emit("jne _L%d", top);
        emitLabel(end);
        break_label_ = old_break;
        continue_label_ = old_cont;
        break;
    }
    case NodeKind::SwitchStmt:
    {
        genSwitchStmt(node);
        break;
    }
    case NodeKind::CaseStmt:
        // Should not be reached directly; handled by genSwitchStmt
        if (!node->children.empty())
            genStatement(node->children.back().get());
        break;
    case NodeKind::DefaultStmt:
        if (!node->children.empty())
            genStatement(node->children[0].get());
        break;
    case NodeKind::LabeledStmt:
        fprintf(out_, "_UL_%s:\n", node->sval.c_str());
        if (!node->children.empty())            // C23 §6.8.1: bare label has no child
            genStatement(node->children[0].get());
        break;
    case NodeKind::GotoStmt:
        emit("jmp _UL_%s", node->sval.c_str());
        break;
    case NodeKind::Declaration:
        genDeclaration(node, false);
        break;
    default:
        break;
    }
}

void CodeGen::genCompoundStmt(ASTNode* node) {
    for (auto& child : node->children) {
        if (child->kind == NodeKind::Declaration)
            genDeclaration(child.get(), false);
        else
            genStatement(child.get());
    }
}

void CodeGen::genIfStmt(ASTNode* node) {
    // Dead-code elimination: if the condition is a compile-time constant,
    // emit only the live branch (no labels, no jumps).
    if (!node->children.empty() && isConstExpr(node->children[0].get())) {
        long long val = evalConstExpr(node->children[0].get());
        if (val != 0) {
            // Constant true: emit only then-branch
            if (node->children.size() >= 2)
                genStatement(node->children[1].get());
        } else {
            // Constant false: emit only else-branch (if present)
            if (node->children.size() >= 3)
                genStatement(node->children[2].get());
        }
        return;
    }

    int else_label = newLabel();
    int end_label = newLabel();

    genExpr(node->children[0].get());
    emit("test rax, rax");

    if (node->children.size() > 2) {
        emit("je _L%d", else_label);
        genStatement(node->children[1].get());
        emit("jmp _L%d", end_label);
        emitLabel(else_label);
        genStatement(node->children[2].get());
        emitLabel(end_label);
    } else {
        emit("je _L%d", end_label);
        genStatement(node->children[1].get());
        emitLabel(end_label);
    }
}

void CodeGen::genWhileStmt(ASTNode* node) {
    // Dead-code elimination: while (0) is a no-op; while (constant != 0) is infinite loop.
    if (!node->children.empty() && isConstExpr(node->children[0].get())) {
        long long val = evalConstExpr(node->children[0].get());
        if (val == 0) return; // while (0): no code emitted
        // while (constant != 0): emit as infinite loop (no condition check)
        int top = newLabel();
        int end = newLabel();
        int old_break = break_label_;
        int old_cont  = continue_label_;
        break_label_    = end;
        continue_label_ = top;
        emitLabel(top);
        if (node->children.size() >= 2) genStatement(node->children[1].get());
        emit("jmp _L%d", top);
        emitLabel(end);
        break_label_    = old_break;
        continue_label_ = old_cont;
        return;
    }

    int top = newLabel();
    int end = newLabel();
    int old_break = break_label_;
    int old_cont = continue_label_;
    break_label_ = end;
    continue_label_ = top;

    emitLabel(top);
    genExpr(node->children[0].get());
    emit("test rax, rax");
    emit("je _L%d", end);
    genStatement(node->children[1].get());
    emit("jmp _L%d", top);
    emitLabel(end);

    break_label_ = old_break;
    continue_label_ = old_cont;
}

void CodeGen::genForStmt(ASTNode* node) {
    // children: init, cond, update, body
    int top = newLabel();
    int end = newLabel();
    int cont = newLabel();
    int old_break = break_label_;
    int old_cont = continue_label_;
    break_label_ = end;
    continue_label_ = cont;

    // Init
    genStatement(node->children[0].get());

    emitLabel(top);
    // Condition
    if (node->children[1]->kind == NodeKind::ExprStmt &&
        !node->children[1]->children.empty()) {
        genExpr(node->children[1]->children[0].get());
        emit("test rax, rax");
        emit("je _L%d", end);
    }

    // Body
    genStatement(node->children[3].get());

    emitLabel(cont);
    // Update - C99 for-loop with declaration has expression directly, traditional has ExprStmt
    if (node->children[2]) {
        if (node->children[2]->kind == NodeKind::ExprStmt &&
            !node->children[2]->children.empty()) {
            genExpr(node->children[2]->children[0].get());
        } else if (node->children[2]->kind != NodeKind::ExprStmt) {
            // Direct expression (C99 for-loop with declaration)
            genExpr(node->children[2].get());
        }
    }
    emit("jmp _L%d", top);
    emitLabel(end);

    break_label_ = old_break;
    continue_label_ = old_cont;
}

void CodeGen::collectCaseLabels(ASTNode* node,
    std::vector<std::pair<ASTNode*, int>>& cases, int& defaultLabel)
{
    if (!node) return;
    if (node->kind == NodeKind::CaseStmt) {
        int lbl = newLabel();
        cases.push_back({ node->children[0].get(), lbl });
        // Assign label to this node via ival (reuse field)
        node->ival = lbl;
        // Recurse into body (children[1]) to find nested cases
        if (node->children.size() > 1)
            collectCaseLabels(node->children[1].get(), cases, defaultLabel);
        return;
    }
    if (node->kind == NodeKind::DefaultStmt) {
        int lbl = newLabel();
        defaultLabel = lbl;
        node->ival = lbl;
        if (!node->children.empty())
            collectCaseLabels(node->children[0].get(), cases, defaultLabel);
        return;
    }
    // Do NOT recurse into nested switch statements — their cases belong to the inner switch
    if (node->kind == NodeKind::SwitchStmt) return;
    for (auto& c : node->children)
        collectCaseLabels(c.get(), cases, defaultLabel);
}

// N5a: Returns true if node ends with an unconditional control-flow transfer.
// Used to detect implicit fall-through in switch case bodies.
static bool nodeEndsWithTransfer(ASTNode* node) {
    if (!node) return false;
    switch (node->kind) {
        case NodeKind::BreakStmt:
        case NodeKind::ReturnStmt:
        case NodeKind::GotoStmt:
        case NodeKind::ContinueStmt:
            return true;
        case NodeKind::CaseStmt:
        case NodeKind::DefaultStmt:
            // Stacked case labels: not a fall-through problem
            return true;
        case NodeKind::CompoundStmt:
        case NodeKind::BlockItems: {
            // Check the last non-declaration statement
            for (int i = (int)node->children.size() - 1; i >= 0; i--) {
                auto* c = node->children[i].get();
                if (c && c->kind != NodeKind::Declaration)
                    return nodeEndsWithTransfer(c);
            }
            return false;
        }
        case NodeKind::IfStmt:
            // if+else where both branches transfer is OK; otherwise not
            if (node->children.size() >= 3)
                return nodeEndsWithTransfer(node->children[1].get()) &&
                       nodeEndsWithTransfer(node->children[2].get());
            return false;
        default:
            return false;
    }
}

void CodeGen::genSwitchBody(ASTNode* node) {
    if (!node) return;
    if (node->kind == NodeKind::CaseStmt) {
        emitLabel((int)node->ival);
        // Generate the case body (children[1])
        if (node->children.size() > 1) {
            ASTNode* body = node->children[1].get();
            genSwitchBody(body);
            // N5a: Fall-through warning — only for non-empty bodies not ending with transfer
            // (Stacked empty cases like "case 1: case 2:" are intentional and not warned)
            // W5: [[fallthrough]] attribute suppresses the warning
            if (body &&
                body->kind != NodeKind::CaseStmt &&
                body->kind != NodeKind::DefaultStmt &&
                !nodeEndsWithTransfer(body)) {
                std::string ft_msg;
                bool has_fallthrough = AttrRegistry::take(node->line, node->line + 1000,
                                                          "fallthrough", ft_msg);
                if (!has_fallthrough) {
                    fprintf(stderr, "%s:%d:%d: warning: implicit fall-through in switch "
                            "(add break, return, or [[fallthrough]] to suppress)\n",
                            source_file_.c_str(), node->line, node->col);
                }
            }
        }
        return;
    }
    if (node->kind == NodeKind::DefaultStmt) {
        emitLabel((int)node->ival);
        if (!node->children.empty())
            genSwitchBody(node->children[0].get());
        return;
    }
    if (node->kind == NodeKind::CompoundStmt) {
        // N5a/W5: Use index-based iteration so we can look ahead at siblings.
        // In the flat AST, "break;" is a sibling of its case label, not inside
        // the case body — so we must look ahead to correctly detect fall-through.
        auto& ch = node->children;
        int n = (int)ch.size();
        for (int i = 0; i < n; i++) {
            ASTNode* c = ch[i].get();
            if (c->kind == NodeKind::Declaration) {
                genDeclaration(c, false);
                continue;
            }
            if (c->kind == NodeKind::CaseStmt) {
                emitLabel((int)c->ival);
                ASTNode* body = (c->children.size() > 1) ? c->children[1].get() : nullptr;
                if (body) genSwitchBody(body);
                // Fall-through check: body must not be a CaseStmt/DefaultStmt itself
                if (body &&
                    body->kind != NodeKind::CaseStmt &&
                    body->kind != NodeKind::DefaultStmt) {
                    bool transfers = nodeEndsWithTransfer(body);
                    // Look ahead at siblings until the next case/default
                    if (!transfers) {
                        for (int j = i + 1; j < n; j++) {
                            ASTNode* sib = ch[j].get();
                            if (sib->kind == NodeKind::CaseStmt ||
                                sib->kind == NodeKind::DefaultStmt)
                                break;  // reached next case — no sibling transfer found
                            if (nodeEndsWithTransfer(sib)) {
                                transfers = true;
                                break;
                            }
                        }
                    }
                    if (!transfers) {
                        // Find the line of the next case/default for a precise range
                        int next_case_line = 0x7fffffff;
                        for (int j = i + 1; j < n; j++) {
                            ASTNode* sib = ch[j].get();
                            if (sib->kind == NodeKind::CaseStmt ||
                                sib->kind == NodeKind::DefaultStmt) {
                                next_case_line = sib->line;
                                break;
                            }
                        }
                        std::string ft_msg;
                        bool has_fallthrough = AttrRegistry::take(c->line, next_case_line,
                                                                  "fallthrough", ft_msg);
                        if (!has_fallthrough) {
                            fprintf(stderr, "%s:%d:%d: warning: implicit fall-through in switch "
                                    "(add break, return, or [[fallthrough]] to suppress)\n",
                                    source_file_.c_str(), c->line, c->col);
                        }
                    }
                }
                continue;
            }
            if (c->kind == NodeKind::DefaultStmt) {
                emitLabel((int)c->ival);
                if (!c->children.empty())
                    genSwitchBody(c->children[0].get());
                continue;
            }
            genSwitchBody(c);
        }
        return;
    }
    // Any other statement: generate normally
    genStatement(node);
}

void CodeGen::genSwitchStmt(ASTNode* node) {
    int end = newLabel();
    int old_break = break_label_;
    break_label_ = end;

    // Evaluate switch expression
    genExpr(node->children[0].get());
    emit("mov rcx, rax"); // switch value in rcx

    // Collect case labels
    std::vector<std::pair<ASTNode*, int>> cases; // expr, label
    int defaultLabel = end; // default = end if no default:
    collectCaseLabels(node->children[1].get(), cases, defaultLabel);

    // Emit comparison chain
    for (auto& [expr, lbl] : cases) {
        emit("push rcx");
        genExpr(expr);
        emit("pop rcx");
        emit("cmp rcx, rax");
        emit("je _L%d", lbl);
    }
    emit("jmp _L%d", defaultLabel);

    // Emit case bodies with fall-through
    genSwitchBody(node->children[1].get());

    emitLabel(end);
    break_label_ = old_break;
}

void CodeGen::genReturnStmt(ASTNode* node) {
    if (!node->children.empty()) {
        // 1.1: Struct return by value — Windows x64 ABI
        // ≤8 bytes: return in RAX; 9-16 bytes: RAX:RDX; >16: hidden pointer
        auto fit = functions_.find(current_func_);
        if (fit != functions_.end() && fit->second.return_type.base == CType::Struct) {
            int struct_size = fit->second.return_type.size;
            if (struct_size > 16) {
                // >16 bytes: copy struct to hidden return pointer (saved in hidden_ret_ptr_offset_)
                genLValue(node->children[0].get());
                emit("mov rbx, rax");  // rbx = source struct address
                emit("mov rax, QWORD PTR [rbp%+d]", hidden_ret_ptr_offset_);  // rax = dest (hidden ptr)
                int qwords = struct_size / 8;
                int rem    = struct_size % 8;
                for (int q = 0; q < qwords; q++) {
                    emit("mov rcx, QWORD PTR [rbx+%d]", q * 8);
                    emit("mov QWORD PTR [rax+%d], rcx", q * 8);
                }
                if (rem >= 4) {
                    emit("mov ecx, DWORD PTR [rbx+%d]", qwords * 8);
                    emit("mov DWORD PTR [rax+%d], ecx", qwords * 8);
                }
                // RAX already holds the hidden pointer address (per Windows x64 ABI)
                emit("jmp _L%s_ret", current_func_.c_str());
                return;
            }
            if (struct_size > 8 && struct_size <= 16) {
                // Load struct address, then load RAX:EDX/RDX
                genLValue(node->children[0].get());
                emit("mov rcx, rax");
                emit("mov rax, QWORD PTR [rcx]");       // first 8 bytes → RAX
                int rem = struct_size - 8;
                if (rem <= 4) emit("mov edx, DWORD PTR [rcx+8]");   // bytes 9-12 → EDX
                else          emit("mov rdx, QWORD PTR [rcx+8]");   // bytes 9-16 → RDX
                emit("jmp _L%s_ret", current_func_.c_str());
                return;
            }
        }
        genExpr(node->children[0].get());
    }
    emit("jmp _L%s_ret", current_func_.c_str());
}

void CodeGen::genExprStmt(ASTNode* node) {
    if (node->children.empty()) return;
    ASTNode* expr = node->children[0].get();

    // [[nodiscard]] warning: warn when a call's return value is thrown away
    if (expr && expr->kind == NodeKind::CallExpr && !expr->children.empty()) {
        ASTNode* callee = expr->children[0].get();
        if (callee && callee->kind == NodeKind::Ident) {
            auto it = functions_.find(callee->sval);
            if (it != functions_.end() && it->second.is_nodiscard) {
                fprintf(stderr, "%s:%d:%d: warning: ignoring return value of "
                        "'%s', declared with attribute [[nodiscard]]\n",
                        source_file_.c_str(), expr->line, expr->col, callee->sval.c_str());
            }
        }
    }
    genExpr(expr);
}

// Expression generation - result in RAX
void CodeGen::genExpr(ASTNode* node) {
    if (!node) return;
    switch (node->kind) {
    case NodeKind::IntLit:
        genIntLit(node);
        break;
    case NodeKind::CharLit:
        emit("mov rax, %lld", node->ival);
        break;
    case NodeKind::StrLit:
        genStrLit(node);
        break;
    case NodeKind::Ident:
        genIdent(node);
        break;
    case NodeKind::BinaryExpr:
        genBinary(node);
        break;
    case NodeKind::UnaryExpr:
        genUnary(node);
        break;
    case NodeKind::AssignExpr:
        genAssign(node);
        break;
    case NodeKind::CallExpr:
        genCall(node);
        break;
    case NodeKind::SubscriptExpr:
        genSubscript(node);
        break;
    case NodeKind::MemberExpr:
        genMember(node);
        break;
    case NodeKind::PostfixExpr:
    {
        // N3: post-increment/decrement invalidates the constant-propagation entry
        if (!node->children.empty() && node->children[0]->kind == NodeKind::Ident)
            const_locals_.erase(node->children[0]->sval);
        // 1.4: determine pointer stride for p++/p-- on typed pointers
        int pf_stride = 1;
        if (!node->children.empty()) {
            ASTNode* ch = node->children[0].get();
            if (ch->kind == NodeKind::Ident) {
                auto it = locals_.find(ch->sval);
                if (it != locals_.end() && it->second.type.base == CType::Ptr && it->second.type.pointee)
                    pf_stride = it->second.type.pointee->size;
                else { auto git = findGlobal(ch->sval);
                    if (git != globals_.end() && git->second.type.base == CType::Ptr && git->second.type.pointee)
                        pf_stride = git->second.type.pointee->size; }
            } else {
                // MemberExpr, SubscriptExpr, etc.: use getExprType for stride
                TypeInfo et = getExprType(ch);
                if (et.base == CType::Ptr && et.pointee && et.pointee->size > 1)
                    pf_stride = et.pointee->size;
            }
        }
        genLValue(node->children[0].get());
        {
            int psz = last_lvalue_size_;
            emit("push rax");
            if (psz == 1) emit("movzx ebx, BYTE PTR [rax]");
            else if (psz == 2) emit("movzx ebx, WORD PTR [rax]");
            else if (psz == 4) emit("mov ebx, DWORD PTR [rax]");
            else emit("mov rbx, QWORD PTR [rax]");
            if (pf_stride <= 1) {
                if (node->sval == "++") emit("lea rcx, [rbx+1]");
                else emit("lea rcx, [rbx-1]");
            } else {
                if (node->sval == "++") emit("lea rcx, [rbx+%d]", pf_stride);
                else emit("lea rcx, [rbx-%d]", pf_stride);
            }
            emit("pop rax");
            const char* sz_ptr = (psz == 1) ? "BYTE" : (psz == 2) ? "WORD" : (psz == 4) ? "DWORD" : "QWORD";
            if (psz == 1) emit("mov BYTE PTR [rax], cl");
            else if (psz == 2) emit("mov WORD PTR [rax], cx");
            else if (psz == 4) emit("mov DWORD PTR [rax], ecx");
            else emit("mov QWORD PTR [rax], rcx");
            emit("mov rax, rbx"); // return old value
        }
        break;
    }
    case NodeKind::ConditionalExpr:
    {
        int false_label = newLabel();
        int end_label = newLabel();
        genExpr(node->children[0].get());
        emit("test rax, rax");
        emit("je _L%d", false_label);
        genExpr(node->children[1].get());
        emit("jmp _L%d", end_label);
        emitLabel(false_label);
        genExpr(node->children[2].get());
        emitLabel(end_label);
        break;
    }
    case NodeKind::CommaExpr:
        genExpr(node->children[0].get());
        genExpr(node->children[1].get());
        break;
    case NodeKind::SizeofExpr:
    {
        // Check if this is typeof (C23) vs sizeof
        bool is_typeof = (node->sval == "typeof");

        int sz = 0;
        TypeInfo expr_type = TypeInfo::make_int();
        auto* child = node->children[0].get();


        // C23: Handle sizeof(typeof(x)) - evaluate inner typeof first
        if (child && child->kind == NodeKind::SizeofExpr && child->sval == "typeof") {
            genExpr(child);  // This will set last_typeof_type_
            if (has_typeof_type_) {
                expr_type = last_typeof_type_;
                sz = expr_type.size;
                has_typeof_type_ = false;  // consumed
            }
        }
        // sizeof(string literal): count chars between quotes + 1 for null terminator
        else if (child && child->kind == NodeKind::StrLit) {
            const std::string& sv = child->sval;
            int char_count = 0;
            if (sv.size() >= 2) {
                for (size_t si = 1; si < sv.size() - 1; si++) {
                    if (sv[si] == '\\' && si + 1 < sv.size() - 1) { si++; }
                    char_count++;
                }
            }
            sz = char_count + 1; // +1 for null terminator
            expr_type = TypeInfo::make_char();
            expr_type.is_unsigned = false;
        }
        // sizeof(expr) or typeof(expr) - try to get type from identifier
        else if (child && child->kind == NodeKind::Ident) {
            auto lit = locals_.find(child->sval);
            if (lit != locals_.end()) {
                expr_type = lit->second.type;
                if (lit->second.type.base == CType::Array)
                    sz = lit->second.type.size; // full array size
                else if (lit->second.type.base == CType::Struct) {
                    auto sit = structs_.find(lit->second.type.struct_name);
                    if (sit != structs_.end()) sz = sit->second.total_size;
                } else
                    sz = lit->second.type.size;
            } else {
                auto git = findGlobal(child->sval);
                if (git != globals_.end()) {
                    expr_type = git->second.type;
                    if (git->second.type.base == CType::Array)
                        sz = git->second.type.size;
                    else if (git->second.type.base == CType::Struct) {
                        auto sit = structs_.find(git->second.type.struct_name);
                        if (sit != structs_.end()) sz = sit->second.total_size;
                        else sz = git->second.type.size;
                    } else
                        sz = git->second.type.size;
                }
            }
        }
        // sizeof(*ptr): dereference pointer to get pointee size
        else if (child && child->kind == NodeKind::UnaryExpr &&
                 (child->sval == "*" || child->sval == "STAR") &&
                 !child->children.empty()) {
            auto* inner = child->children[0].get();
            TypeInfo inner_type = getExprType(inner);
            if (inner_type.base == CType::Ptr && inner_type.pointee) {
                expr_type = *inner_type.pointee;
                if (expr_type.base == CType::Struct && !expr_type.struct_name.empty()) {
                    auto sit = structs_.find(expr_type.struct_name);
                    if (sit != structs_.end()) sz = sit->second.total_size;
                } else
                    sz = expr_type.size;
            }
        }
        // sizeof(arr[i]): get element type from array variable
        else if (child && child->kind == NodeKind::SubscriptExpr &&
                 !child->children.empty()) {
            auto* base = child->children[0].get();
            TypeInfo base_type = getExprType(base);
            if (base_type.pointee) {
                expr_type = *base_type.pointee;
                if (expr_type.base == CType::Struct && !expr_type.struct_name.empty()) {
                    auto sit = structs_.find(expr_type.struct_name);
                    if (sit != structs_.end()) sz = sit->second.total_size;
                } else
                    sz = expr_type.size;
            }
            // Array member stored as struct type (e.g., inner struct): dereference
            else if (base_type.base == CType::Struct && !base_type.struct_name.empty()) {
                auto sit = structs_.find(base_type.struct_name);
                if (sit != structs_.end()) { sz = sit->second.total_size; expr_type = base_type; }
                else {
                    std::string bare = base_type.struct_name;
                    if (bare.substr(0, 7) == "struct ") bare = bare.substr(7);
                    sit = structs_.find(bare);
                    if (sit != structs_.end()) { sz = sit->second.total_size; expr_type = base_type; }
                    else if (base_type.size > 0) { sz = base_type.size; expr_type = base_type; }
                }
            }
        }
        // sizeof(expr->member) or sizeof(expr.member): resolve member type
        else if (child && child->kind == NodeKind::MemberExpr && !child->children.empty()) {
            // MemberExpr stores member name in sval: "->member" or ".member"
            std::string member_name = child->sval;
            if (member_name.size() >= 2 && member_name[0] == '-' && member_name[1] == '>')
                member_name = member_name.substr(2);
            else if (!member_name.empty() && member_name[0] == '.')
                member_name = member_name.substr(1);
            // Resolve the struct type of the LHS
            TypeInfo lhs_type = getExprType(child->children[0].get());
            // Dereference pointer if -> access
            if (lhs_type.base == CType::Ptr && lhs_type.pointee)
                lhs_type = *lhs_type.pointee;
            if (lhs_type.base == CType::Struct && !lhs_type.struct_name.empty()) {
                auto sit = structs_.find(lhs_type.struct_name);
                if (sit != structs_.end()) {
                    for (auto& mem : sit->second.members) {
                        if (mem.name == member_name) {
                            expr_type = mem.type;
                            if (mem.type.base == CType::Array && mem.type.size > 0)
                                sz = mem.type.size;
                            else if (mem.type.base == CType::Struct && !mem.type.struct_name.empty()) {
                                auto sit2 = structs_.find(mem.type.struct_name);
                                if (sit2 != structs_.end()) sz = sit2->second.total_size;
                                else sz = mem.type.size;
                            } else
                                sz = mem.type.size;
                            break;
                        }
                    }
                }
            }
        }
        if (sz == 0) {
            // Check for pointer type (e.g. sizeof(int *))
            bool has_ptr = false;
            if (child && child->children.size() > 1) {
                auto* adecl = child->children[1].get();
                if (adecl && (adecl->sval == "*" || adecl->kind == NodeKind::Pointer)) has_ptr = true;
            }
            if (has_ptr) {
                sz = 8;
                expr_type = TypeInfo::make_ptr();
            } else {
                TypeInfo t = resolveType(child);
                expr_type = t;
                sz = t.size ? t.size : 8;
                if (t.base == CType::Struct && !t.struct_name.empty()) {
                    auto sit = structs_.find(t.struct_name);
                    if (sit != structs_.end()) sz = sit->second.total_size;
                }
                // Also check: if child is a TypeSpec/DeclSpecs/Ident containing a typedef name,
                // that typedef might point to "struct TAGNAME" registered in structs_.
                if (t.base != CType::Struct || t.struct_name.empty()) {
                    std::string lookup_name;
                    if (child->kind == NodeKind::TypeSpec) lookup_name = child->sval;
                    else if (child->kind == NodeKind::Ident) lookup_name = child->sval;
                    else if (child->kind == NodeKind::DeclSpecs && !child->children.empty()) {
                        auto* c0 = child->children[0].get();
                        if (c0 && (c0->kind == NodeKind::TypeSpec || c0->kind == NodeKind::Ident))
                            lookup_name = c0->sval;
                    }
                    if (!lookup_name.empty()) {
                        // Try struct lookup: "struct TAGNAME", "STRUCT TAGNAME", "union TAGNAME", "UNION TAGNAME"
                        for (const char* prefix : {"struct ", "STRUCT ", "union ", "UNION "}) {
                            auto sn = std::string(prefix) + lookup_name;
                            auto sit2 = structs_.find(sn);
                            if (sit2 != structs_.end() && sit2->second.total_size > 0) {
                                sz = sit2->second.total_size;
                                expr_type = TypeInfo::make_struct(sn, sz);
                                break;
                            }
                        }
                    }
                }
            }
        }

        if (is_typeof) {
            // C23 typeof: store the type for use with auto or in other contexts
            last_typeof_type_ = expr_type;
            has_typeof_type_ = true;
            // typeof in expression context returns the size (like sizeof)
            // This allows sizeof(typeof(x)) to work
        }
        emit("mov rax, %d", sz);
        break;
    }
    case NodeKind::CastExpr:
        genCast(node);
        break;
    case NodeKind::FloatLit:
        genFloatLit(node);
        break;
    default:
        // Try children
        for (auto& c : node->children) genExpr(c.get());
        break;
    }
}

void CodeGen::genIntLit(ASTNode* node) {
    if (node->ival >= 0 && node->ival <= 2147483647LL) {
        emit("mov eax, %lld", node->ival);
    } else {
        emit("mov rax, %lld", node->ival);
    }
}

void CodeGen::genStrLit(ASTNode* node) {
    char label[32];
    snprintf(label, sizeof(label), "__str%d", string_count_++);
    int char_width = (node->ival == 2 || node->ival == 4) ? (int)node->ival : 1;
    string_literals_.emplace_back(std::string(label), node->sval, char_width);
    emit("lea rax, %s", label);
}

void CodeGen::genFloatLit(ASTNode* node) {
    // Store float constant in .data, load with movss
    char label[32];
    snprintf(label, sizeof(label), "__flt%d", float_count_++);
    float fval = 0.0f;
    if (!node->sval.empty()) fval = (float)atof(node->sval.c_str());
    else fval = (float)node->ival;
    float_literals_.push_back({label, fval});
    emit("movss xmm0, DWORD PTR [%s]", label);
    last_expr_is_float_ = true;
    last_expr_is_ptr_ = false;
}

void CodeGen::genCast(ASTNode* node) {
    if (node->children.size() < 2) return;
    // For cast expressions, children[0] contains the type specification
    TypeInfo target = resolveType(node->children[0].get());

    // Check if the cast type includes pointer declarators
    // Traverse children[0] to find Pointer or AbstractDeclarator nodes
    auto* typeNode = node->children[0].get();
    int ptr_levels = 0;

    // Helper function to count pointers recursively
    std::function<void(ASTNode*)> countPointers = [&](ASTNode* n) {
        if (!n) return;
        if (n->kind == NodeKind::Pointer) {
            ptr_levels++;
        }
        if (n->kind == NodeKind::AbstractDeclarator && n->sval == "*") {
            // Some grammar versions mark pointer in sval
            ptr_levels++;
        }
        for (auto& child : n->children) {
            countPointers(child.get());
        }
    };

    countPointers(typeNode);

    // Build pointer type chain if pointers found
    for (int i = 0; i < ptr_levels; i++) {
        TypeInfo* pointee = new TypeInfo(target);
        target = TypeInfo::make_ptr();
        target.pointee = pointee;
    }

    // Check for compound literal: (Type){ initializer_list }
    if (node->children[1]->kind == NodeKind::InitializerList && target.base == CType::Struct) {
        auto sit = structs_.find(target.struct_name);
        if (sit != structs_.end()) {
            auto& layout = sit->second;
            int off = allocLocal(layout.total_size > 0 ? layout.total_size : 8);
            auto* init = node->children[1].get();
            for (size_t i = 0; i < init->children.size() && i < layout.members.size(); i++) {
                genExpr(init->children[i].get());
                int moff = off + layout.members[i].offset;
                int msz = layout.members[i].type.size;
                if (msz == 1) emit("mov BYTE PTR [rbp%+d], al", moff);
                else if (msz == 4) emit("mov DWORD PTR [rbp%+d], eax", moff);
                else emit("mov QWORD PTR [rbp%+d], rax", moff);
            }
            emit("lea rax, [rbp%+d]", off);
            last_expr_is_ptr_ = true;
            last_expr_is_float_ = false;
            return;
        }
    }

    // Normal cast
    genExpr(node->children[1].get());

    if (target.base == CType::Char) {
        if (target.is_unsigned)
            emit("movzx eax, al");
        else
            emit("movsx eax, al");
        last_expr_is_ptr_ = false;
        last_expr_is_float_ = false;
    } else if (target.base == CType::Short) {
        if (target.is_unsigned)
            emit("movzx eax, ax");
        else
            emit("movsx eax, ax");
        last_expr_is_ptr_ = false;
        last_expr_is_float_ = false;
    } else if (target.base == CType::Float) {
        if (!last_expr_is_float_) {
            emit("cvtsi2ss xmm0, eax");
            last_expr_is_float_ = true;
            last_float_size_ = 4;
        } else if (last_float_size_ == 8) {
            emit("cvtsd2ss xmm0, xmm0"); // double->float narrowing
            last_float_size_ = 4;
        }
    } else if (target.base == CType::Double) {
        if (!last_expr_is_float_) {
            emit("cvtsi2sd xmm0, rax");
            last_expr_is_float_ = true;
            last_float_size_ = 8;
        } else if (last_float_size_ == 4) {
            emit("cvtss2sd xmm0, xmm0"); // float->double widening
            last_float_size_ = 8;
        }
    } else if (target.base == CType::Int || target.base == CType::Long) {
        if (last_expr_is_float_) {
            if (last_float_size_ == 8) emit("cvttsd2si rax, xmm0");
            else emit("cvttss2si eax, xmm0");
            last_expr_is_float_ = false;
        }
        last_expr_is_ptr_ = false;  // ptr-to-int cast: no longer a pointer
    } else if (target.base == CType::Ptr) {
        // Cast to pointer type: track pointee size for correct dereference
        if (target.pointee) {
            last_pointee_size_ = target.pointee->size;
        } else {
            last_pointee_size_ = 1;  // void* defaults to byte access
        }
        last_expr_is_ptr_ = true;
    }
    // Ptr<->Int: no-op, value stays in rax
}

TypeInfo CodeGen::getExprType(ASTNode* node) {
    if (!node) return TypeInfo::make_int();
    if (node->kind == NodeKind::Ident) {
        auto it = locals_.find(node->sval);
        if (it != locals_.end()) return it->second.type;
        auto git = findGlobal(node->sval);
        if (git != globals_.end()) return git->second.type;
        auto eit = enum_constants_.find(node->sval);
        if (eit != enum_constants_.end()) return TypeInfo::make_int();
    }
    if (node->kind == NodeKind::IntLit || node->kind == NodeKind::CharLit) return TypeInfo::make_int();
    if (node->kind == NodeKind::StrLit) {
        TypeInfo t = TypeInfo::make_ptr();
        t.pointee = new TypeInfo(TypeInfo::make_char());
        return t;
    }
    if (node->kind == NodeKind::FloatLit) return TypeInfo::make_float();
    // CastExpr: resolve target type including pointer levels (used by offsetof pattern)
    if (node->kind == NodeKind::CastExpr && !node->children.empty()) {
        TypeInfo target = resolveType(node->children[0].get());
        int ptr_levels = 0;
        std::function<void(ASTNode*)> countPtrs = [&](ASTNode* n) {
            if (!n) return;
            if (n->kind == NodeKind::Pointer) ptr_levels++;
            if (n->kind == NodeKind::AbstractDeclarator && n->sval == "*") ptr_levels++;
            for (auto& c : n->children) countPtrs(c.get());
        };
        countPtrs(node->children[0].get());
        for (int i = 0; i < ptr_levels; i++) {
            TypeInfo* pointee = new TypeInfo(target);
            target = TypeInfo::make_ptr();
            target.pointee = pointee;
        }
        return target;
    }
    // MemberExpr: look up member type in the parent struct
    if (node->kind == NodeKind::MemberExpr && !node->children.empty()) {
        std::string member;
        bool is_arrow = false;
        if (node->sval.size() > 2 && node->sval.substr(0, 2) == "->") {
            member = node->sval.substr(2);
            is_arrow = true;
        } else if (node->sval.size() > 1 && node->sval[0] == '.') {
            member = node->sval.substr(1);
        } else {
            member = node->sval;
        }
        // Determine the struct type of the LHS
        TypeInfo lhs_type = getExprType(node->children[0].get());
        if (is_arrow && lhs_type.base == CType::Ptr && lhs_type.pointee)
            lhs_type = *lhs_type.pointee;
        if (lhs_type.base == CType::Struct && !lhs_type.struct_name.empty()) {
            auto sit = structs_.find(lhs_type.struct_name);
            if (sit != structs_.end()) {
                for (auto& m : sit->second.members) {
                    if (m.name == member) return m.type;
                }
            }
        }
        // Fallback: search all structs for this member name
        for (auto& [sname, layout] : structs_) {
            for (auto& m : layout.members) {
                if (m.name == member) return m.type;
            }
        }
        return TypeInfo::make_int();
    }
    // SubscriptExpr: arr[i] — dereference array/pointer element type
    if (node->kind == NodeKind::SubscriptExpr && !node->children.empty()) {
        TypeInfo base_type = getExprType(node->children[0].get());
        if (base_type.base == CType::Ptr && base_type.pointee)
            return *base_type.pointee;
        if (base_type.base == CType::Array && base_type.pointee)
            return *base_type.pointee;
    }
    // UnaryExpr '*': dereference pointer to get pointee type
    if (node->kind == NodeKind::UnaryExpr &&
        (node->sval == "*" || node->sval == "STAR") &&
        !node->children.empty()) {
        TypeInfo inner = getExprType(node->children[0].get());
        if (inner.base == CType::Ptr && inner.pointee)
            return *inner.pointee;
    }
    // 1.1: CallExpr — look up function return type so struct assignments work
    if (node->kind == NodeKind::CallExpr && !node->children.empty() &&
        node->children[0]->kind == NodeKind::Ident) {
        auto fit = functions_.find(node->children[0]->sval);
        if (fit != functions_.end()) return fit->second.return_type;
    }
    return TypeInfo::make_int();
}

// C23: Infer type from initializer expression for auto declarations
TypeInfo CodeGen::inferTypeFromExpr(ASTNode* node) {
    if (!node) return TypeInfo::make_int();

    switch (node->kind) {
    case NodeKind::IntLit:
        // Check if value fits in int or needs long
        if (node->ival > 2147483647LL || node->ival < -2147483648LL)
            return TypeInfo::make_long();
        return TypeInfo::make_int();

    case NodeKind::CharLit:
        return TypeInfo::make_char();

    case NodeKind::FloatLit:
        return TypeInfo::make_float();

    case NodeKind::StrLit: {
        TypeInfo t = TypeInfo::make_ptr();
        t.pointee = new TypeInfo(TypeInfo::make_char());
        return t;
    }

    case NodeKind::Ident: {
        // Look up the identifier's type
        auto it = locals_.find(node->sval);
        if (it != locals_.end()) return it->second.type;
        auto git = findGlobal(node->sval);
        if (git != globals_.end()) return git->second.type;
        auto eit = enum_constants_.find(node->sval);
        if (eit != enum_constants_.end()) return TypeInfo::make_int();
        // Assume function - return int (default return type)
        return TypeInfo::make_int();
    }

    case NodeKind::UnaryExpr:
        if (node->sval == "&" || node->sval == "AMPERSAND") {
            // Address-of: result is pointer to operand type
            TypeInfo base = inferTypeFromExpr(node->children[0].get());
            TypeInfo t = TypeInfo::make_ptr();
            t.pointee = new TypeInfo(base);
            return t;
        }
        if (node->sval == "*" || node->sval == "STAR") {
            // Dereference: result is pointee type
            TypeInfo base = inferTypeFromExpr(node->children[0].get());
            if (base.pointee) return *base.pointee;
            return TypeInfo::make_int();
        }
        return inferTypeFromExpr(node->children[0].get());

    case NodeKind::BinaryExpr: {
        TypeInfo left = inferTypeFromExpr(node->children[0].get());
        TypeInfo right = node->children.size() > 1 ?
            inferTypeFromExpr(node->children[1].get()) : TypeInfo::make_int();
        // Comparison operators return int
        if (node->sval == "==" || node->sval == "!=" || node->sval == "<" ||
            node->sval == ">" || node->sval == "<=" || node->sval == ">=" ||
            node->sval == "&&" || node->sval == "||")
            return TypeInfo::make_int();
        // Pointer arithmetic
        if (left.base == CType::Ptr) return left;
        if (right.base == CType::Ptr) return right;
        // Float promotion
        if (left.base == CType::Float || left.base == CType::Double) return left;
        if (right.base == CType::Float || right.base == CType::Double) return right;
        // Long promotion
        if (left.base == CType::Long || right.base == CType::Long)
            return TypeInfo::make_long();
        return TypeInfo::make_int();
    }

    case NodeKind::CallExpr: {
        // Try to find function return type
        if (!node->children.empty() && node->children[0]->kind == NodeKind::Ident) {
            std::string fname = node->children[0]->sval;
            auto it = functions_.find(fname);
            if (it != functions_.end()) return it->second.return_type;
        }
        return TypeInfo::make_int();
    }

    case NodeKind::CastExpr:
        // Cast to the target type
        if (!node->children.empty())
            return resolveType(node->children[0].get());
        return TypeInfo::make_int();

    case NodeKind::SubscriptExpr: {
        // Array subscript: result is element type
        TypeInfo base = inferTypeFromExpr(node->children[0].get());
        if (base.pointee) return *base.pointee;
        return TypeInfo::make_int();
    }

    case NodeKind::MemberExpr: {
        // Struct member access: find member type
        std::string member;
        if (node->sval.size() > 2 && node->sval.substr(0, 2) == "->")
            member = node->sval.substr(2);
        else if (node->sval.size() > 1 && node->sval[0] == '.')
            member = node->sval.substr(1);
        for (auto& [sname, layout] : structs_) {
            for (auto& m : layout.members) {
                if (m.name == member) return m.type;
            }
        }
        return TypeInfo::make_int();
    }

    case NodeKind::ConditionalExpr:
        // Ternary: type of true branch
        if (node->children.size() >= 2)
            return inferTypeFromExpr(node->children[1].get());
        return TypeInfo::make_int();

    case NodeKind::SizeofExpr:
        // sizeof always returns size_t (treat as long)
        return TypeInfo::make_long();

    default:
        // Single child passthrough
        if (node->children.size() == 1)
            return inferTypeFromExpr(node->children[0].get());
        return TypeInfo::make_int();
    }
}

void CodeGen::genIdent(ASTNode* node) {
    // C23: Check constexpr float values first — emit as double literal in xmm0
    auto cfit = constexpr_float_values_.find(node->sval);
    if (cfit != constexpr_float_values_.end()) {
        std::string lbl = "__cxfd_" + std::to_string(double_count_++) + "__";
        double_literals_.push_back({lbl, cfit->second});
        emit("movsd xmm0, QWORD PTR [%s]", lbl.c_str());
        last_expr_is_float_ = true;
        last_float_size_ = 8;
        last_expr_is_ptr_ = false;
        return;
    }
    // C23: Check constexpr integer values — emit as immediate
    auto cit = constexpr_values_.find(node->sval);
    if (cit != constexpr_values_.end()) {
        if (cit->second >= 0 && cit->second <= 2147483647LL)
            emit("mov eax, %lld", cit->second);
        else
            emit("mov rax, %lld", cit->second);
        last_expr_is_ptr_ = false;
        last_expr_is_float_ = false;
        last_pointee_size_ = 8;
        return;
    }

    // N3: Constant propagation was disabled for genExpr(Ident) because it incorrectly
    // folded loop-modified variables (e.g., loop counter j=0 in while(j<n) always
    // generates cmp 0,n instead of reloading j from memory on each iteration).
    // const_locals_ is only used in evalConstExpr for compile-time evaluation contexts.

    auto it = locals_.find(node->sval);
    if (it != locals_.end()) {
        locals_used_.insert(node->sval); // W6: mark as read
        if (it->second.type.base == CType::Array) {
            // Array decays to pointer - return address of first element
            emit("lea rax, [rbp%+d]", it->second.stack_offset);
            last_expr_is_ptr_ = true;
            last_pointee_size_ = it->second.type.pointee ? it->second.type.pointee->size : 8;
        } else if (it->second.type.base == CType::Ptr) {
            emit("mov rax, QWORD PTR [rbp%+d]", it->second.stack_offset);
            last_expr_is_ptr_ = true;
            last_expr_is_float_ = false;
            last_pointee_size_ = it->second.type.pointee ? it->second.type.pointee->size : 8;
        } else if (it->second.type.base == CType::Float) {
            emit("movss xmm0, DWORD PTR [rbp%+d]", it->second.stack_offset);
            last_expr_is_ptr_ = false;
            last_expr_is_float_ = true;
            last_float_size_ = 4;
        } else if (it->second.type.base == CType::Double) {
            emit("movsd xmm0, QWORD PTR [rbp%+d]", it->second.stack_offset);
            last_expr_is_ptr_ = false;
            last_expr_is_float_ = true;
            last_float_size_ = 8;
        } else if (it->second.type.base == CType::Struct) {
            if (it->second.type.size <= 8) {
                emit("mov rax, QWORD PTR [rbp%+d]", it->second.stack_offset);
            } else {
                emit("lea rax, [rbp%+d]", it->second.stack_offset);
            }
            last_expr_is_ptr_ = false;
            last_expr_is_float_ = false;
            last_pointee_size_ = 8;
        } else {
            int vsz = it->second.type.size;
            if (vsz == 1) {
                if (it->second.type.is_unsigned)
                    emit("movzx eax, BYTE PTR [rbp%+d]", it->second.stack_offset);
                else
                    emit("movsx rax, BYTE PTR [rbp%+d]", it->second.stack_offset);
            } else if (vsz == 2) {
                if (it->second.type.is_unsigned)
                    emit("movzx eax, WORD PTR [rbp%+d]", it->second.stack_offset);
                else
                    emit("movsx rax, WORD PTR [rbp%+d]", it->second.stack_offset);
            } else if (vsz == 4) {
                if (it->second.type.is_unsigned)
                    emit("mov eax, DWORD PTR [rbp%+d]", it->second.stack_offset);
                else
                    emit("movsxd rax, DWORD PTR [rbp%+d]", it->second.stack_offset);
            } else {
                emit("mov rax, QWORD PTR [rbp%+d]", it->second.stack_offset);
            }
            last_expr_is_ptr_ = false;
            last_expr_is_float_ = false;
            last_pointee_size_ = 8;
            last_expr_is_unsigned_ = it->second.type.is_unsigned; // 1.3
        }
    } else {
        auto git = findGlobal(node->sval);
        if (git != globals_.end()) {
            if (git->second.type.base == CType::Array) {
                emit("lea rax, [%s]", git->second.global_label.c_str());
                last_expr_is_ptr_ = true;
            } else {
                int gsz = git->second.type.size;
                if (gsz == 1) {
                    if (git->second.type.is_unsigned)
                        emit("movzx eax, BYTE PTR [%s]", git->second.global_label.c_str());
                    else
                        emit("movsx rax, BYTE PTR [%s]", git->second.global_label.c_str());
                } else if (gsz == 2) {
                    if (git->second.type.is_unsigned)
                        emit("movzx eax, WORD PTR [%s]", git->second.global_label.c_str());
                    else
                        emit("movsx rax, WORD PTR [%s]", git->second.global_label.c_str());
                } else if (gsz == 4) {
                    if (git->second.type.is_unsigned)
                        emit("mov eax, DWORD PTR [%s]", git->second.global_label.c_str());
                    else
                        emit("movsxd rax, DWORD PTR [%s]", git->second.global_label.c_str());
                } else {
                    emit("mov rax, QWORD PTR [%s]", git->second.global_label.c_str());
                }
                last_expr_is_ptr_ = (git->second.type.base == CType::Ptr);
            }
            last_pointee_size_ = git->second.type.pointee ? git->second.type.pointee->size : 8;
            last_expr_is_float_ = false;
            last_expr_is_unsigned_ = git->second.type.is_unsigned; // 1.3
        } else {
            // Check enum constants
            auto eit = enum_constants_.find(node->sval);
            if (eit != enum_constants_.end()) {
                if (eit->second >= 0 && eit->second <= 2147483647LL)
                    emit("mov eax, %lld", eit->second);
                else
                    emit("mov rax, %lld", eit->second);
                last_expr_is_ptr_ = false;
                last_pointee_size_ = 8;
                return;
            }
            // Check if it's a known function (defined or declared as prototype)
            auto fit = functions_.find(node->sval);
            if (fit != functions_.end()) {
                // Function used as a value (function pointer) — emit its address
                emit("lea rax, %s", node->sval.c_str());
                called_functions_.insert(node->sval); // ensure EXTERN emitted for fn-ptr use
                last_expr_is_ptr_ = true;
                last_expr_is_float_ = false;
            } else {
                // Truly undeclared identifier — warn and emit safe zero
                fprintf(stderr, "%s%s:%d:%d: warning: use of undeclared identifier '%s'%s\n",
                        cg_warn(), source_file_.c_str(), node->line, node->col, node->sval.c_str(), cg_reset());
                emit("xor rax, rax");
                last_expr_is_ptr_ = false;
                last_expr_is_float_ = false;
                last_pointee_size_ = 8;
            }
        }
    }
}

void CodeGen::genLValue(ASTNode* node) {
    if (!node) { last_lvalue_size_ = 8; last_lvalue_is_float_ = false; return; }
    last_lvalue_is_float_ = false;
    last_lvalue_is_bitfield_ = false;
    last_lvalue_bit_offset_ = 0;
    last_lvalue_bit_width_ = 0;
    if (node->kind == NodeKind::Ident) {
        auto it = locals_.find(node->sval);
        if (it != locals_.end()) {
            locals_used_.insert(node->sval); // W6: &x and ++x count as a use
            emit("lea rax, [rbp%+d]", it->second.stack_offset);
            if (it->second.type.base == CType::Float) {
                last_lvalue_size_ = 4;
                last_lvalue_is_float_ = true;
            } else if (it->second.type.base == CType::Double) {
                last_lvalue_size_ = 8;
                last_lvalue_is_float_ = true;
            } else {
                last_lvalue_size_ = 8;
            }
        } else {
            auto git = findGlobal(node->sval);
            if (git != globals_.end())
                emit("lea rax, [%s]", git->second.global_label.c_str());
            else
                emit("lea rax, [_%s]", node->sval.c_str());
            last_lvalue_size_ = 8;
        }
    } else if (node->kind == NodeKind::UnaryExpr && node->sval == "*") {
        genExpr(node->children[0].get());
        // Dereferencing a pointer: the store size is the pointee size
        last_lvalue_size_ = getExprPointeeSize(node->children[0].get());
    } else if (node->kind == NodeKind::SubscriptExpr) {
        // Check for multi-dimensional array (same logic as genSubscript)
        std::vector<ASTNode*> indices;
        ASTNode* base = node;
        while (base->kind == NodeKind::SubscriptExpr && base->children.size() >= 2) {
            indices.push_back(base->children[1].get());
            base = base->children[0].get();
        }
        std::reverse(indices.begin(), indices.end());

        std::vector<int> dims;
        if (base->kind == NodeKind::Ident) {
            auto it = locals_.find(base->sval);
            if (it != locals_.end() && it->second.type.array_dims.size() > 1) {
                dims = it->second.type.array_dims;
            } else {
                auto git = findGlobal(base->sval);
                if (git != globals_.end() && git->second.type.array_dims.size() > 1) {
                    dims = git->second.type.array_dims;
                }
            }
        }

        if (dims.size() > 1 && indices.size() == dims.size()) {
            // Multi-dim array lvalue: compute flat address
            int elem_size = getExprPointeeSize(base);
            genExpr(indices[0]);
            for (size_t k = 1; k < indices.size(); k++) {
                emit("push rax");
                emit("mov rax, %d", dims[k]);
                emit("pop rcx");
                emit("imul rax, rcx");
                emit("push rax");
                genExpr(indices[k]);
                emit("pop rcx");
                emit("add rax, rcx");
            }
            emit("push rax");
            genLValue(base); // base address (LEA, no deref)
            emit("pop rcx");
            emitPtrScale("rcx", elem_size); // 1.4: correct stride for struct arrays
            emit("add rax, rcx");
            last_lvalue_size_ = elem_size;
        } else {
            // Single-dimension array
            int elem_size = getExprPointeeSize(node->children[0].get());
            genExpr(node->children[1].get()); // index
            emit("push rax");
            genExpr(node->children[0].get()); // base (array decays to ptr)
            emit("pop rcx");
            emitPtrScale("rcx", elem_size); // 1.4: correct stride for struct arrays
            emit("add rax, rcx");
            last_lvalue_size_ = elem_size;
        }
    } else if (node->kind == NodeKind::MemberExpr) {
        std::string member;
        bool is_arrow = false;
        if (node->sval.size() > 2 && node->sval.substr(0, 2) == "->") {
            member = node->sval.substr(2);
            is_arrow = true;
        } else if (node->sval.size() > 1 && node->sval[0] == '.') {
            member = node->sval.substr(1);
        } else if (node->children.size() > 1 && node->children[1]->kind == NodeKind::Ident) {
            member = node->children[1]->sval;
        } else {
            member = node->sval;
        }
        int offset = 0;
        int msz = 8;
        int lv_bit_offset = 0;
        int lv_bit_width  = 0;

        // Determine lhs struct type to find the correct member offset
        // Use getExprType() which recursively resolves type through chains.
        std::string lv_struct_name;
        {
            ASTNode* lhs = !node->children.empty() ? node->children[0].get() : nullptr;
            if (lhs) {
                TypeInfo lhs_type = getExprType(lhs);
                if (is_arrow && lhs_type.base == CType::Ptr && lhs_type.pointee)
                    lhs_type = *lhs_type.pointee;
                if (lhs_type.base == CType::Struct && !lhs_type.struct_name.empty())
                    lv_struct_name = lhs_type.struct_name;
            }
        }

        // First: look in the specific struct
        if (!lv_struct_name.empty()) {
            auto sit = structs_.find(lv_struct_name);
            if (sit != structs_.end()) {
                for (auto& m : sit->second.members) {
                    if (m.name == member) {
                        offset        = m.offset;
                        msz           = m.type.size;
                        lv_bit_offset = m.bit_offset;
                        lv_bit_width  = m.bit_width;
                        goto lv_found;
                    }
                }
            }
        }
        // Fallback: global search
        for (auto& [sname, layout] : structs_) {
            for (auto& m : layout.members) {
                if (m.name == member) {
                    offset       = m.offset;
                    msz          = m.type.size;
                    lv_bit_offset = m.bit_offset;
                    lv_bit_width  = m.bit_width;
                    goto lv_found;
                }
            }
        }
        lv_found:
        if (is_arrow) {
            genExpr(node->children[0].get());
        } else {
            genLValue(node->children[0].get());
        }
        if (offset != 0) emit("add rax, %d", offset);
        last_lvalue_size_        = msz;
        last_lvalue_is_bitfield_ = (lv_bit_width > 0);
        last_lvalue_bit_offset_  = lv_bit_offset;
        last_lvalue_bit_width_   = lv_bit_width;
    } else {
        last_lvalue_size_ = 8;
    }
}

void CodeGen::genAssign(ASTNode* node) {
    // N3: Constant propagation — any write to a local kills its known constant value
    if (!node->children.empty() && node->children[0]->kind == NodeKind::Ident)
        const_locals_.erase(node->children[0]->sval);

    // Struct-to-struct assignment: copy entire struct by address
    // Detect when either side is a struct type and do a block copy
    // NOTE: skip this path for CallExpr — function-call struct results are handled below
    if (node->sval == "=" && node->children.size() >= 2) {
        bool rhs_is_call = (node->children[1]->kind == NodeKind::CallExpr);
        if (!rhs_is_call) {
            TypeInfo rhs_type = getExprType(node->children[1].get());
            int copy_size = 0;
            // Try RHS type first
            if (rhs_type.base == CType::Struct && !rhs_type.struct_name.empty()) {
                auto sit = structs_.find(rhs_type.struct_name);
                copy_size = (sit != structs_.end()) ? sit->second.total_size : rhs_type.size;
            }
            // Fallback: try LHS type (catches cases where RHS type is unknown,
            // e.g. *va_arg(ap, struct_type*))
            if (copy_size <= 8) {
                TypeInfo lhs_type = getExprType(node->children[0].get());
                if (lhs_type.base == CType::Struct && !lhs_type.struct_name.empty()) {
                    auto sit = structs_.find(lhs_type.struct_name);
                    copy_size = (sit != structs_.end()) ? sit->second.total_size : lhs_type.size;
                }
            }
            if (copy_size > 8) {
                // Get source address (lvalue of RHS)
                genLValue(node->children[1].get());
                emit("mov rsi, rax"); // source address
                // Get destination address (lvalue of LHS)
                genLValue(node->children[0].get());
                emit("mov rdi, rax"); // dest address
                // Copy copy_size bytes in 8-byte chunks
                for (int b = 0; b + 8 <= copy_size; b += 8) {
                    emit("mov rcx, QWORD PTR [rsi+%d]", b);
                    emit("mov QWORD PTR [rdi+%d], rcx", b);
                }
                int rem = copy_size % 8;
                int base = copy_size - rem;
                for (int b = 0; b < rem; b++) {
                    emit("mov cl, BYTE PTR [rsi+%d]", base + b);
                    emit("mov BYTE PTR [rdi+%d], cl", base + b);
                }
                emit("lea rax, [rdi]"); // result = dest address
                return;
            }
        }
    }

    // children[0] = lvalue, children[1] = rvalue
    // Determine if LHS is a pointer for += / -= stride scaling
    int assign_ptr_stride = 0; // 0 = not pointer, >0 = pointee size
    if (node->sval == "+=" || node->sval == "-=") {
        TypeInfo lhs_type = getExprType(node->children[0].get());
        if (lhs_type.base == CType::Ptr && lhs_type.pointee && lhs_type.pointee->size > 1)
            assign_ptr_stride = lhs_type.pointee->size;
    }
    genExpr(node->children[1].get());
    bool rval_is_float = last_expr_is_float_;
    bool rval_is_double = last_expr_is_float_;  // track double separately below
    int  rval_struct_sz = last_expr_struct_size_; // 1.1: struct call result size (0 = not struct)
    // Scale RHS by pointer stride for += / -= on typed pointers
    if (assign_ptr_stride > 0 && !rval_is_float)
        emitPtrScale("rax", assign_ptr_stride);
    if (rval_is_float) {
        emit("sub rsp, 8");
        emit("movsd QWORD PTR [rsp], xmm0");  // always save 8 bytes (covers float and double)
    } else if (rval_struct_sz > 8) {
        // 1.1: struct call result — preserve both RAX and RDX across genLValue
        emit("push rdx");
        emit("push rax");
    } else {
        emit("push rax");
    }
    genLValue(node->children[0].get());
    int lsz = last_lvalue_size_;
    bool lval_is_float        = last_lvalue_is_float_;
    bool lval_is_bitfield     = last_lvalue_is_bitfield_;
    int  lval_bit_offset      = last_lvalue_bit_offset_;
    int  lval_bit_width       = last_lvalue_bit_width_;
    bool lval_is_double = (lval_is_float && lsz == 8);
    emit("mov rbx, rax"); // address in rbx
    if (rval_is_float) {
        emit("movsd xmm0, QWORD PTR [rsp]");   // reload 8 bytes (covers both float and double)
        emit("add rsp, 8");
    } else if (rval_struct_sz > 8) {
        // 1.1: restore both halves of struct call result
        emit("pop rax");
        emit("pop rdx");
    } else {
        emit("pop rax");      // value in rax
    }

    // Bit-field simple assignment: read-modify-write the storage unit
    if (lval_is_bitfield && node->sval == "=") {
        long long mask        = (1LL << lval_bit_width) - 1;
        long long placed_mask = mask << lval_bit_offset;
        emit("and eax, %lld", mask);     // truncate to bit-field width
        emit("push rax");                 // save field value for expression result
        if (lval_bit_offset > 0) emit("shl eax, %d", lval_bit_offset);
        // Read-modify-write: clear old bits and insert new ones
        if (lsz == 1) {
            long long inv = (~placed_mask) & 0xFF;
            emit("movzx ecx, BYTE PTR [rbx]");
            emit("and ecx, %lld", inv);
            emit("or ecx, eax");
            emit("mov BYTE PTR [rbx], cl");
        } else if (lsz == 2) {
            long long inv = (~placed_mask) & 0xFFFF;
            emit("movzx ecx, WORD PTR [rbx]");
            emit("and ecx, %lld", inv);
            emit("or ecx, eax");
            emit("mov WORD PTR [rbx], cx");
        } else {
            long long inv = (~placed_mask) & 0xFFFFFFFFL;
            emit("mov ecx, DWORD PTR [rbx]");
            emit("and ecx, %lld", inv);
            emit("or ecx, eax");
            emit("mov DWORD PTR [rbx], ecx");
        }
        emit("pop rax");   // expression result = the new field value
        return;
    }

    if (node->sval == "=") {
        if (lval_is_float || rval_is_float) {
            if (!rval_is_float) {
                // Integer → float/double conversion
                if (lsz == 8) emit("cvtsi2sd xmm0, rax");
                else          emit("cvtsi2ss xmm0, eax");
            }
            // Store: use movsd for double (8 bytes), movss for float (4 bytes)
            if (lsz == 8) emit("movsd QWORD PTR [rbx], xmm0");
            else          emit("movss DWORD PTR [rbx], xmm0");
        } else if (rval_struct_sz > 8) {
            // 1.1: struct call result in RAX:RDX — store both halves
            emit("mov QWORD PTR [rbx], rax");
            int rem = rval_struct_sz - 8;
            if (rem <= 4) emit("mov DWORD PTR [rbx+8], edx");
            else          emit("mov QWORD PTR [rbx+8], rdx");
            emit("lea rax, [rbx]"); // result = dest address
            last_expr_struct_size_ = 0;
        } else if (lsz == 1) emit("mov BYTE PTR [rbx], al");
        else if (lsz == 2) emit("mov WORD PTR [rbx], ax");
        else if (lsz == 4) emit("mov DWORD PTR [rbx], eax");
        else emit("mov QWORD PTR [rbx], rax");
    } else if (node->sval == "+=") {
        if (lsz == 4) { emit("add DWORD PTR [rbx], eax"); emit("movsxd rax, DWORD PTR [rbx]"); }
        else if (lsz == 2) { emit("add WORD PTR [rbx], ax"); emit("movzx eax, WORD PTR [rbx]"); }
        else if (lsz == 1) { emit("add BYTE PTR [rbx], al"); emit("movsx rax, BYTE PTR [rbx]"); }
        else { emit("add QWORD PTR [rbx], rax"); emit("mov rax, QWORD PTR [rbx]"); }
    } else if (node->sval == "-=") {
        if (lsz == 4) { emit("sub DWORD PTR [rbx], eax"); emit("movsxd rax, DWORD PTR [rbx]"); }
        else if (lsz == 2) { emit("sub WORD PTR [rbx], ax"); emit("movzx eax, WORD PTR [rbx]"); }
        else if (lsz == 1) { emit("sub BYTE PTR [rbx], al"); emit("movsx rax, BYTE PTR [rbx]"); }
        else { emit("sub QWORD PTR [rbx], rax"); emit("mov rax, QWORD PTR [rbx]"); }
    } else if (node->sval == "*=") {
        if (lsz == 4) { emit("movsxd rcx, DWORD PTR [rbx]"); emit("imul ecx, eax"); emit("mov DWORD PTR [rbx], ecx"); emit("movsxd rax, ecx"); }
        else { emit("mov rcx, QWORD PTR [rbx]"); emit("imul rcx, rax"); emit("mov QWORD PTR [rbx], rcx"); emit("mov rax, rcx"); }
    } else if (node->sval == "/=") {
        emit("mov rcx, rax");
        if (lsz == 4) { emit("movsxd rax, DWORD PTR [rbx]"); emit("cdq"); emit("idiv ecx"); emit("mov DWORD PTR [rbx], eax"); }
        else { emit("mov rax, QWORD PTR [rbx]"); emit("cqo"); emit("idiv rcx"); emit("mov QWORD PTR [rbx], rax"); }
    } else if (node->sval == "%=") {
        emit("mov rcx, rax");
        if (lsz == 4) { emit("movsxd rax, DWORD PTR [rbx]"); emit("cdq"); emit("idiv ecx"); emit("mov DWORD PTR [rbx], edx"); emit("movsxd rax, edx"); }
        else { emit("mov rax, QWORD PTR [rbx]"); emit("cqo"); emit("idiv rcx"); emit("mov QWORD PTR [rbx], rdx"); emit("mov rax, rdx"); }
    } else if (node->sval == "<<=") {
        emit("mov rcx, rax");
        if (lsz == 1) { emit("movzx eax, BYTE PTR [rbx]"); emit("shl al, cl"); emit("mov BYTE PTR [rbx], al"); emit("movzx eax, al"); }
        else if (lsz == 2) { emit("movzx eax, WORD PTR [rbx]"); emit("shl ax, cl"); emit("mov WORD PTR [rbx], ax"); emit("movzx eax, ax"); }
        else if (lsz == 4) { emit("mov eax, DWORD PTR [rbx]"); emit("shl eax, cl"); emit("mov DWORD PTR [rbx], eax"); }
        else { emit("mov rax, QWORD PTR [rbx]"); emit("shl rax, cl"); emit("mov QWORD PTR [rbx], rax"); }
    } else if (node->sval == ">>=") {
        emit("mov rcx, rax");
        if (lsz == 1) { emit("movzx eax, BYTE PTR [rbx]"); emit("sar al, cl"); emit("mov BYTE PTR [rbx], al"); emit("movzx eax, al"); }
        else if (lsz == 2) { emit("movzx eax, WORD PTR [rbx]"); emit("sar ax, cl"); emit("mov WORD PTR [rbx], ax"); emit("movzx eax, ax"); }
        else if (lsz == 4) { emit("mov eax, DWORD PTR [rbx]"); emit("sar eax, cl"); emit("mov DWORD PTR [rbx], eax"); }
        else { emit("mov rax, QWORD PTR [rbx]"); emit("sar rax, cl"); emit("mov QWORD PTR [rbx], rax"); }
    } else if (node->sval == "&=") {
        if (lsz == 1) { emit("and BYTE PTR [rbx], al"); emit("movzx eax, BYTE PTR [rbx]"); }
        else if (lsz == 2) { emit("and WORD PTR [rbx], ax"); emit("movzx eax, WORD PTR [rbx]"); }
        else if (lsz == 4) { emit("and DWORD PTR [rbx], eax"); emit("mov eax, DWORD PTR [rbx]"); }
        else { emit("and QWORD PTR [rbx], rax"); emit("mov rax, QWORD PTR [rbx]"); }
    } else if (node->sval == "^=") {
        if (lsz == 1) { emit("xor BYTE PTR [rbx], al"); emit("movzx eax, BYTE PTR [rbx]"); }
        else if (lsz == 2) { emit("xor WORD PTR [rbx], ax"); emit("movzx eax, WORD PTR [rbx]"); }
        else if (lsz == 4) { emit("xor DWORD PTR [rbx], eax"); emit("mov eax, DWORD PTR [rbx]"); }
        else { emit("xor QWORD PTR [rbx], rax"); emit("mov rax, QWORD PTR [rbx]"); }
    } else if (node->sval == "|=") {
        if (lsz == 1) { emit("or BYTE PTR [rbx], al"); emit("movzx eax, BYTE PTR [rbx]"); }
        else if (lsz == 2) { emit("or WORD PTR [rbx], ax"); emit("movzx eax, WORD PTR [rbx]"); }
        else if (lsz == 4) { emit("or DWORD PTR [rbx], eax"); emit("mov eax, DWORD PTR [rbx]"); }
        else { emit("or QWORD PTR [rbx], rax"); emit("mov rax, QWORD PTR [rbx]"); }
    }
}

// Check if expression tree contains only constants (int/char literals, enum/constexpr constants, unary/binary ops)
bool CodeGen::isConstExpr(ASTNode* node) {
    if (!node) return false;
    if (node->kind == NodeKind::IntLit || node->kind == NodeKind::CharLit) return true;
    if (node->kind == NodeKind::FloatLit) return true; // C23: float literals are constant expressions
    if (node->kind == NodeKind::Ident) {
        // C23: constexpr variables (integer and float) are constant expressions
        if (constexpr_values_.count(node->sval) > 0) return true;
        if (constexpr_float_values_.count(node->sval) > 0) return true;
        // N3: const_locals_ are NOT compile-time constant expressions (they are
        // runtime variables with known initial values that may change in loops/branches).
        // Only enum constants and true constexpr variables qualify as constant expressions.
        return enum_constants_.count(node->sval) > 0;
    }
    if (node->kind == NodeKind::UnaryExpr && !node->children.empty())
        return isConstExpr(node->children[0].get());
    if (node->kind == NodeKind::BinaryExpr && node->children.size() >= 2)
        return isConstExpr(node->children[0].get()) && isConstExpr(node->children[1].get());
    if (node->kind == NodeKind::ConditionalExpr && node->children.size() >= 3)
        return isConstExpr(node->children[0].get()) && isConstExpr(node->children[1].get()) && isConstExpr(node->children[2].get());
    // P4-E: sizeof is always a constant expression (C99 §6.5.3.4)
    if (node->kind == NodeKind::SizeofExpr) return true;
    // P4-E: Cast expressions propagate constness from their operand
    if (node->kind == NodeKind::CastExpr && !node->children.empty())
        return isConstExpr(node->children.back().get());
    // GAP-6: constexpr array subscript is a constant expression
    if (node->kind == NodeKind::SubscriptExpr && node->children.size() >= 2) {
        auto* arr = node->children[0].get();
        auto* idx = node->children[1].get();
        if (arr && arr->kind == NodeKind::Ident &&
            constexpr_array_values_.count(arr->sval))
            return isConstExpr(idx);
        return false;
    }
    // GAP-D: constexpr struct member is a constant expression
    if (node->kind == NodeKind::MemberExpr && !node->children.empty()) {
        auto* obj = node->children[0].get();
        if (obj && obj->kind == NodeKind::Ident) {
            if (!constexpr_struct_values_.count(obj->sval)) return false;
            std::string member = node->sval;
            if (member.size() >= 2 && member[0] == '-' && member[1] == '>')
                member = member.substr(2);
            else if (!member.empty() && member[0] == '.')
                member = member.substr(1);
            return constexpr_struct_values_.at(obj->sval).count(member) > 0;
        }
        return false;
    }
    if (node->children.size() == 1) return isConstExpr(node->children[0].get());
    return false;
}

void CodeGen::genBinary(ASTNode* node) {
    const std::string& op = node->sval;

    // Constant folding: if both operands are compile-time constants, fold
    if (isConstExpr(node)) {
        long long val = evalConstExpr(node);
        if (val >= -2147483647LL && val <= 2147483647LL)
            emit("mov eax, %lld", val);
        else
            emit("mov rax, %lld", val);
        last_expr_is_ptr_ = false;
        last_expr_is_float_ = false;
        return;
    }

    // Short-circuit for && and ||
    if (op == "&&" || op == "AND_OP") {
        int false_label = newLabel();
        int end_label = newLabel();
        genExpr(node->children[0].get());
        emit("test rax, rax");
        emit("je _L%d", false_label);
        genExpr(node->children[1].get());
        emit("test rax, rax");
        emit("je _L%d", false_label);
        emit("mov eax, 1");
        emit("jmp _L%d", end_label);
        emitLabel(false_label);
        emit("xor eax, eax");
        emitLabel(end_label);
        return;
    }
    if (op == "||" || op == "OR_OP") {
        int true_label = newLabel();
        int end_label = newLabel();
        genExpr(node->children[0].get());
        emit("test rax, rax");
        emit("jne _L%d", true_label);
        genExpr(node->children[1].get());
        emit("test rax, rax");
        emit("jne _L%d", true_label);
        emit("xor eax, eax");
        emit("jmp _L%d", end_label);
        emitLabel(true_label);
        emit("mov eax, 1");
        emitLabel(end_label);
        return;
    }

    // Evaluate left, push, evaluate right
    genExpr(node->children[0].get());
    bool left_is_ptr = last_expr_is_ptr_;
    bool left_is_float = last_expr_is_float_;
    bool left_is_unsigned = last_expr_is_unsigned_; // 1.3
    int left_pointee = last_pointee_size_;
    int left_float_size = last_float_size_;
    if (left_is_float) {
        // Save xmm0 to stack (8 bytes covers both float and double)
        emit("sub rsp, 8");
        emit("movsd QWORD PTR [rsp], xmm0");
        push_depth_++; // 2.2: track stack displacement
    } else {
        emit("push rax");
        push_depth_++; // 2.2: track stack displacement
    }
    genExpr(node->children[1].get());
    bool right_is_ptr = last_expr_is_ptr_;
    bool right_is_float = last_expr_is_float_;
    bool right_is_unsigned = last_expr_is_unsigned_; // 1.3
    int right_float_size = last_float_size_;
    int right_pointee = last_pointee_size_;

    bool use_float = left_is_float || right_is_float;

    if (use_float) {
        // If either operand is double, promote both (C usual arithmetic conversions)
        bool use_double = (left_is_float && left_float_size == 8) ||
                          (right_is_float && right_float_size == 8);
        // right operand in xmm0 (or convert from integer)
        if (!right_is_float) {
            if (use_double) emit("cvtsi2sd xmm0, rax");
            else            emit("cvtsi2ss xmm0, eax");
        } else if (use_double && right_float_size == 4) {
            emit("cvtss2sd xmm0, xmm0"); // promote right float->double
        }
        if (use_double) emit("movsd xmm1, xmm0"); // right in xmm1
        else            emit("movss xmm1, xmm0");
        // left operand from stack to xmm0
        if (left_is_float) {
            emit("movsd xmm0, QWORD PTR [rsp]"); // load 8 bytes
            emit("add rsp, 8");
            push_depth_--; // 2.2
            if (use_double && left_float_size == 4) {
                emit("cvtss2sd xmm0, xmm0"); // promote left float->double
            }
        } else {
            emit("pop rax");
            push_depth_--; // 2.2
            if (use_double) emit("cvtsi2sd xmm0, rax");
            else            emit("cvtsi2ss xmm0, eax");
        }
        // Now: xmm0=left, xmm1=right in same precision
        if (use_double) {
            if (op == "+" || op == "PLUS") { emit("addsd xmm0, xmm1"); }
            else if (op == "-" || op == "MINUS") { emit("subsd xmm0, xmm1"); }
            else if (op == "*" || op == "STAR") { emit("mulsd xmm0, xmm1"); }
            else if (op == "/" || op == "SLASH") { emit("divsd xmm0, xmm1"); }
            else if (op == "<" || op == "LT") {
                emit("ucomisd xmm0, xmm1"); emit("setb al"); emit("movzx eax, al");
                last_expr_is_float_ = false; return;
            } else if (op == ">" || op == "GT") {
                emit("ucomisd xmm0, xmm1"); emit("seta al"); emit("movzx eax, al");
                last_expr_is_float_ = false; return;
            } else if (op == "<=" || op == "LE_OP") {
                emit("ucomisd xmm0, xmm1"); emit("setbe al"); emit("movzx eax, al");
                last_expr_is_float_ = false; return;
            } else if (op == ">=" || op == "GE_OP") {
                emit("ucomisd xmm0, xmm1"); emit("setae al"); emit("movzx eax, al");
                last_expr_is_float_ = false; return;
            } else if (op == "==" || op == "EQ_OP") {
                emit("ucomisd xmm0, xmm1"); emit("sete al"); emit("movzx eax, al");
                last_expr_is_float_ = false; return;
            } else if (op == "!=" || op == "NE_OP") {
                emit("ucomisd xmm0, xmm1"); emit("setne al"); emit("movzx eax, al");
                last_expr_is_float_ = false; return;
            }
        } else {
            if (op == "+" || op == "PLUS") { emit("addss xmm0, xmm1"); }
            else if (op == "-" || op == "MINUS") { emit("subss xmm0, xmm1"); }
            else if (op == "*" || op == "STAR") { emit("mulss xmm0, xmm1"); }
            else if (op == "/" || op == "SLASH") { emit("divss xmm0, xmm1"); }
            else if (op == "<" || op == "LT") {
                emit("ucomiss xmm0, xmm1"); emit("setb al"); emit("movzx eax, al");
                last_expr_is_float_ = false; return;
            } else if (op == ">" || op == "GT") {
                emit("ucomiss xmm0, xmm1"); emit("seta al"); emit("movzx eax, al");
                last_expr_is_float_ = false; return;
            } else if (op == "<=" || op == "LE_OP") {
                emit("ucomiss xmm0, xmm1"); emit("setbe al"); emit("movzx eax, al");
                last_expr_is_float_ = false; return;
            } else if (op == ">=" || op == "GE_OP") {
                emit("ucomiss xmm0, xmm1"); emit("setae al"); emit("movzx eax, al");
                last_expr_is_float_ = false; return;
            } else if (op == "==" || op == "EQ_OP") {
                emit("ucomiss xmm0, xmm1"); emit("sete al"); emit("movzx eax, al");
                last_expr_is_float_ = false; return;
            } else if (op == "!=" || op == "NE_OP") {
                emit("ucomiss xmm0, xmm1"); emit("setne al"); emit("movzx eax, al");
                last_expr_is_float_ = false; return;
            }
        }
        last_expr_is_float_ = true;
        last_float_size_ = use_double ? 8 : 4;
        last_expr_is_ptr_ = false;
        return;
    }

    emit("mov rcx, rax"); // right in rcx
    emit("pop rax");      // left in rax
    push_depth_--;        // 2.2: balance the push from before right evaluation

    if (op == "+" || op == "PLUS") {
        // Scale for pointer arithmetic: ptr + int or int + ptr
        if (left_is_ptr && !right_is_ptr) {
            emitPtrScale("rcx", left_pointee);  // 1.4: ptr+int: scale integer by stride
        } else if (!left_is_ptr && right_is_ptr) {
            emitPtrScale("rax", right_pointee); // 1.4: int+ptr: scale integer by stride
        }
        emit("add rax, rcx");
        last_expr_is_ptr_ = left_is_ptr || right_is_ptr;
        last_pointee_size_ = left_is_ptr ? left_pointee : right_pointee;
    } else if (op == "-" || op == "MINUS") {
        if (left_is_ptr && !right_is_ptr) {
            emitPtrScale("rcx", left_pointee);  // 1.4: ptr-int: scale integer by stride
        }
        emit("sub rax, rcx");
        if (left_is_ptr && right_is_ptr) {
            // 1.4: ptr-ptr = element count: divide byte difference by stride
            int _ps = ptrSizeShift(left_pointee);
            if (left_pointee > 1) {
                if (_ps > 0) emit("sar rax, %d", _ps);          // power-of-2: arithmetic shift
                else { emit("mov rcx, %d", left_pointee); emit("cqo"); emit("idiv rcx"); }
            }
            last_expr_is_ptr_ = false;
        } else {
            last_expr_is_ptr_ = left_is_ptr;
            last_pointee_size_ = left_pointee;
        }
    } else if (op == "*" || op == "STAR") {
        emit("imul rax, rcx");
    } else if (op == "/" || op == "SLASH") {
        // 1.3: unsigned division uses div/xor rdx instead of idiv/cqo
        if (left_is_unsigned) { emit("xor rdx, rdx"); emit("div rcx"); }
        else                  { emit("cqo"); emit("idiv rcx"); }
    } else if (op == "%%" || op == "PERCENT") {
        if (left_is_unsigned) { emit("xor rdx, rdx"); emit("div rcx"); emit("mov rax, rdx"); }
        else                  { emit("cqo"); emit("idiv rcx"); emit("mov rax, rdx"); }
    } else if (op == "&" || op == "AMPERSAND") {
        emit("and rax, rcx");
    } else if (op == "|" || op == "PIPE") {
        emit("or rax, rcx");
    } else if (op == "^" || op == "CARET") {
        emit("xor rax, rcx");
    } else if (op == "==" || op == "EQ_OP") {
        emit("cmp rax, rcx");
        emit("sete al");
        emit("movzx rax, al");
    } else if (op == "!=" || op == "NE_OP") {
        emit("cmp rax, rcx");
        emit("setne al");
        emit("movzx rax, al");
    } else if (op == "<" || op == "LT") {
        // 1.3: unsigned comparisons use setb/seta/setbe/setae instead of setl/setg/setle/setge
        // 4.4: sign comparison warning — mixed signed/unsigned operands
        if (left_is_unsigned != right_is_unsigned)
            fprintf(stderr, "%s%s:%d:%d: warning: comparison between signed and unsigned integer%s\n",
                    cg_warn(), source_file_.c_str(), node->line, node->col, cg_reset());
        emit("cmp rax, rcx");
        emit((left_is_unsigned || right_is_unsigned) ? "setb al" : "setl al");
        emit("movzx rax, al");
    } else if (op == ">" || op == "GT") {
        if (left_is_unsigned != right_is_unsigned)
            fprintf(stderr, "%s%s:%d:%d: warning: comparison between signed and unsigned integer%s\n",
                    cg_warn(), source_file_.c_str(), node->line, node->col, cg_reset());
        emit("cmp rax, rcx");
        emit((left_is_unsigned || right_is_unsigned) ? "seta al" : "setg al");
        emit("movzx rax, al");
    } else if (op == "<=" || op == "LE_OP") {
        if (left_is_unsigned != right_is_unsigned)
            fprintf(stderr, "%s%s:%d:%d: warning: comparison between signed and unsigned integer%s\n",
                    cg_warn(), source_file_.c_str(), node->line, node->col, cg_reset());
        emit("cmp rax, rcx");
        emit((left_is_unsigned || right_is_unsigned) ? "setbe al" : "setle al");
        emit("movzx rax, al");
    } else if (op == ">=" || op == "GE_OP") {
        if (left_is_unsigned != right_is_unsigned)
            fprintf(stderr, "%s%s:%d:%d: warning: comparison between signed and unsigned integer%s\n",
                    cg_warn(), source_file_.c_str(), node->line, node->col, cg_reset());
        emit("cmp rax, rcx");
        emit((left_is_unsigned || right_is_unsigned) ? "setae al" : "setge al");
        emit("movzx rax, al");
    } else if (op == "<<" || op == "LEFT_OP") {
        emit("shl rax, cl");
    } else if (op == ">>" || op == "RIGHT_OP") {
        // 1.3: unsigned right shift is logical (shr), signed is arithmetic (sar)
        emit(left_is_unsigned ? "shr rax, cl" : "sar rax, cl");
    }
}

void CodeGen::genUnary(ASTNode* node) {
    if (node->sval == "&" || node->sval == "AMPERSAND") {
        genLValue(node->children[0].get());
        last_expr_is_float_ = false;  // address-of always yields a pointer, not float
        last_expr_is_ptr_ = true;
        return;
    }
    if (node->sval == "*" || node->sval == "STAR") {
        genExpr(node->children[0].get());
        int elem_size = last_pointee_size_;
        bool elem_unsigned = false;
        {
            TypeInfo et = getExprType(node);
            elem_unsigned = et.is_unsigned;
        }
        if (elem_size == 1) {
            if (elem_unsigned) emit("movzx eax, BYTE PTR [rax]");
            else emit("movsx rax, BYTE PTR [rax]");
        }
        else if (elem_size == 4) {
            if (elem_unsigned) emit("mov eax, DWORD PTR [rax]");
            else emit("movsxd rax, DWORD PTR [rax]");
        }
        else emit("mov rax, QWORD PTR [rax]");
        // Dereferenced value is not a pointer (unless pointee is itself a pointer)
        last_expr_is_ptr_ = (elem_size == 8);
        last_expr_is_float_ = false;
        last_expr_is_unsigned_ = elem_unsigned;
        // 1.5: Update last_pointee_size_ for chained dereferences (e.g. **pp).
        // If we just loaded a pointer value, the next dereference needs to know its pointee size.
        if (last_expr_is_ptr_) {
            last_pointee_size_ = getExprPointeeSize(node);
        }
        return;
    }

    // Handle pre-increment/decrement before genExpr to avoid double evaluation
    if (node->sval == "++" || node->sval == "INC_OP") {
        // W3: invalidate const_locals_ entry so subsequent reads reload from stack
        if (!node->children.empty() && node->children[0]->kind == NodeKind::Ident)
            const_locals_.erase(node->children[0]->sval);
        // 1.4: determine pointer stride before genLValue clobbers last_* state
        int inc_stride = 1;
        if (!node->children.empty()) {
            ASTNode* ch = node->children[0].get();
            if (ch->kind == NodeKind::Ident) {
                auto it = locals_.find(ch->sval);
                if (it != locals_.end() && it->second.type.base == CType::Ptr && it->second.type.pointee)
                    inc_stride = it->second.type.pointee->size;
                else { auto git = findGlobal(ch->sval);
                    if (git != globals_.end() && git->second.type.base == CType::Ptr && git->second.type.pointee)
                        inc_stride = git->second.type.pointee->size; }
            } else {
                TypeInfo et = getExprType(ch);
                if (et.base == CType::Ptr && et.pointee && et.pointee->size > 1)
                    inc_stride = et.pointee->size;
            }
        }
        genLValue(node->children[0].get());
        {
            int isz = last_lvalue_size_;
            const char* sz_ptr = (isz == 1) ? "BYTE" : (isz == 2) ? "WORD" : (isz == 4) ? "DWORD" : "QWORD";
            if (inc_stride <= 1) emit("inc %s PTR [rax]", sz_ptr);
            else { emit("mov rcx, %d", inc_stride); emit("add %s PTR [rax], rcx", sz_ptr); }
            if (isz == 1) emit("movzx eax, BYTE PTR [rax]");
            else if (isz == 2) emit("movzx eax, WORD PTR [rax]");
            else if (isz == 4) emit("mov eax, DWORD PTR [rax]");
            else emit("mov rax, QWORD PTR [rax]");
        }
        return;
    }
    if (node->sval == "--" || node->sval == "DEC_OP") {
        // W3: invalidate const_locals_ entry so subsequent reads reload from stack
        if (!node->children.empty() && node->children[0]->kind == NodeKind::Ident)
            const_locals_.erase(node->children[0]->sval);
        // 1.4: determine pointer stride before genLValue clobbers last_* state
        int dec_stride = 1;
        if (!node->children.empty()) {
            ASTNode* ch = node->children[0].get();
            if (ch->kind == NodeKind::Ident) {
                auto it = locals_.find(ch->sval);
                if (it != locals_.end() && it->second.type.base == CType::Ptr && it->second.type.pointee)
                    dec_stride = it->second.type.pointee->size;
                else { auto git = findGlobal(ch->sval);
                    if (git != globals_.end() && git->second.type.base == CType::Ptr && git->second.type.pointee)
                        dec_stride = git->second.type.pointee->size; }
            } else {
                TypeInfo et = getExprType(ch);
                if (et.base == CType::Ptr && et.pointee && et.pointee->size > 1)
                    dec_stride = et.pointee->size;
            }
        }
        genLValue(node->children[0].get());
        {
            int dsz = last_lvalue_size_;
            const char* sz_ptr = (dsz == 1) ? "BYTE" : (dsz == 2) ? "WORD" : (dsz == 4) ? "DWORD" : "QWORD";
            if (dec_stride <= 1) emit("dec %s PTR [rax]", sz_ptr);
            else { emit("mov rcx, %d", dec_stride); emit("sub %s PTR [rax], rcx", sz_ptr); }
            if (dsz == 1) emit("movzx eax, BYTE PTR [rax]");
            else if (dsz == 2) emit("movzx eax, WORD PTR [rax]");
            else if (dsz == 4) emit("mov eax, DWORD PTR [rax]");
            else emit("mov rax, QWORD PTR [rax]");
        }
        return;
    }

    genExpr(node->children[0].get());

    if (node->sval == "-" || node->sval == "MINUS") {
        emit("neg rax");
    } else if (node->sval == "!" || node->sval == "BANG") {
        emit("test rax, rax");
        emit("sete al");
        emit("movzx rax, al");
    } else if (node->sval == "~" || node->sval == "TILDE") {
        emit("not rax");
    }
}

void CodeGen::genCall(ASTNode* node) {
    // children[0] = function expr, children[1] = arg_list (optional)
    std::string fname;
    if (node->children[0]->kind == NodeKind::Ident)
        fname = node->children[0]->sval;

    // Collect args
    std::vector<ASTNode*> args;
    if (node->children.size() > 1 && node->children[1]) {
        for (auto& a : node->children[1]->children)
            args.push_back(a.get());
    }

    // C99: Variadic function support - built-in va_start, va_arg, va_end
    if (fname == "va_start" || fname == "__builtin_va_start") {
        // va_start(ap, last_param): initialize ap to point after last named param
        // Windows x64 ABI: register args homed to shadow space at [rbp+16..rbp+40]
        // For N named params, variadic args start at [rbp + 16 + N*8]
        if (args.size() >= 1) {
            genLValue(args[0]);  // Get address of ap variable
            emit("mov rbx, rax");
            // Compute offset based on number of named parameters in current function
            int num_named = 0;
            auto fit = functions_.find(current_func_);
            if (fit != functions_.end()) {
                num_named = (int)fit->second.param_names.size();
            }
            int va_offset = 16 + num_named * 8;  // Shadow space starts at rbp+16
            emit("lea rax, [rbp+%d]", va_offset);
            emit("mov QWORD PTR [rbx], rax");
        }
        return;
    }

    if (fname == "va_arg" || fname == "__builtin_va_arg") {
        // va_arg(ap, type): get next arg from ap, advance ap by 8
        // Note: The type parameter is for the result type, which we simplify as 8 bytes
        if (args.size() >= 1) {
            genLValue(args[0]);  // Get address of ap variable
            emit("mov rbx, rax");
            emit("mov rax, QWORD PTR [rbx]");  // ap value (pointer)
            emit("mov rcx, QWORD PTR [rax]");  // Load the argument
            emit("add QWORD PTR [rbx], 8");    // Advance ap by 8
            emit("mov rax, rcx");              // Result in rax
        }
        return;
    }

    if (fname == "va_end" || fname == "__builtin_va_end") {
        // va_end(ap): no-op for our simple implementation
        return;
    }

    if (fname == "va_copy" || fname == "__builtin_va_copy") {
        // va_copy(dest, src): copy src to dest
        if (args.size() >= 2) {
            genExpr(args[1]);     // src value
            emit("push rax");
            genLValue(args[0]);   // dest address
            emit("mov rbx, rax");
            emit("pop rax");
            emit("mov QWORD PTR [rbx], rax");
        }
        return;
    }

    // C11/C23: _Static_assert(condition, "message") / static_assert(...)
    // Evaluate condition as a compile-time constant.  Emit an error and no code.
    if (fname == "_Static_assert" || fname == "static_assert") {
        if (!args.empty() && isConstExpr(args[0])) {
            long long val = evalConstExpr(args[0]);
            if (!val) {
                std::string msg = "<no message>";
                if (args.size() >= 2 && args[1]->kind == NodeKind::StrLit)
                    msg = args[1]->sval;
                // Strip surrounding quotes if present
                if (msg.size() >= 2 && msg.front() == '"' && msg.back() == '"')
                    msg = msg.substr(1, msg.size() - 2);
                fprintf(stderr, "%s%s:%d:%d: error: static assertion failed: %s%s\n",
                        cg_err(), source_file_.c_str(), node->line, node->col, msg.c_str(), cg_reset());
            }
            // else: assertion passes, emit nothing
        } else if (!args.empty()) {
            fprintf(stderr, "%s%s:%d:%d: warning: _Static_assert condition is not a "
                    "constant expression - cannot check at compile time%s\n",
                    cg_warn(), source_file_.c_str(), node->line, node->col, cg_reset());
        }
        return;  // Always emit no code
    }

    // Windows x64 ABI: first 4 args in rcx, rdx, r8, r9
    // Args 5+ at [rsp+32], [rsp+40], ... before call

    static const char* arg_regs[] = { "rcx", "rdx", "r8", "r9" };
    int nargs = (int)args.size();

    // Argument count check against known function signature
    if (!fname.empty()) {
        auto fit = functions_.find(fname);
        if (fit != functions_.end() && !fit->second.is_variadic) {
            int expected = (int)fit->second.param_types.size();
            if (nargs > expected) {
                fprintf(stderr, "%s:%d:%d: warning: too many arguments to function '%s' "
                        "(expected %d, got %d)\n",
                        source_file_.c_str(), node->line, node->col, fname.c_str(), expected, nargs);
            } else if (nargs < expected) {
                fprintf(stderr, "%s:%d:%d: warning: too few arguments to function '%s' "
                        "(expected %d, got %d)\n",
                        source_file_.c_str(), node->line, node->col, fname.c_str(), expected, nargs);
            }
        }
    }

    // 1.1/2.1: Pre-scan -- detect large struct return and struct args needing copies
    bool callee_ret_large    = false;
    int  callee_ret_struct_sz = 0;
    int  ret_perm_slot       = 0;  // rbp-relative slot for large struct return value
    if (!fname.empty()) {
        auto ffit = functions_.find(fname);
        if (ffit != functions_.end() && ffit->second.returns_large_struct) {
            callee_ret_large     = true;
            callee_ret_struct_sz = ffit->second.return_type.size;
            ret_perm_slot        = allocLocal(callee_ret_struct_sz);
        }
    }
    int arg_reg_shift = callee_ret_large ? 1 : 0;  // RCX slot used for hidden ptr
    int max_reg_args  = 4 - arg_reg_shift;
    // Pre-scan struct args >8 bytes: build per-arg size and copy-offset arrays
    int struct_copy_total = 0;
    std::vector<int> arg_struct_sz_v(nargs, 0);
    std::vector<int> arg_struct_copy_off(nargs, -1);
    for (int si = 0; si < nargs; si++) {
        TypeInfo spt = getParamType(fname, si);
        if (spt.base == CType::Struct && spt.size > 8) {
            arg_struct_sz_v[si]     = spt.size;
            arg_struct_copy_off[si] = struct_copy_total;
            struct_copy_total += (spt.size + 7) & ~7;
        }
    }
    int stack_args = nargs > max_reg_args ? nargs - max_reg_args : 0;
    int reg_args   = nargs < max_reg_args ? nargs : max_reg_args;

    // Calculate total call frame: shadow(32) + stack_args*8, alignment-corrected.
    // 2.2: push_depth_ tracks active push rax calls from enclosing genBinary expressions.
    // After prologue, RSP ≡ 8 (mod 16). Each active push changes parity.
    // We need: (RSP_before_call - frame) ≡ 8 (mod 16), so CALL makes RSP ≡ 0 in callee.
    // RSP_before_call ≡ (8 - push_depth_*8) mod 16, i.e. 8 if push_depth_ even, 0 if odd.
    // Required frame mod 16: (RSP_before - 8) mod 16 = 0 if even, 8 if odd push_depth_.
    // Detect indirect calls early so we can reserve frame space for the saved fn pointer.
    // Indirect = callee is a complex expression OR a function-pointer variable.
    bool is_indirect_call = false;
    if (fname.empty()) {
        is_indirect_call = true;
    } else {
        auto lit = locals_.find(fname);
        auto git = findGlobal(fname);
        if (lit != locals_.end() || git != globals_.end()) is_indirect_call = true;
    }
    int fptr_save_offset = -1;
    int fptr_extra = 0;
    if (is_indirect_call) {
        fptr_extra = 8;  // reserve 8 bytes in frame for saved function pointer
    }

    int frame = 32 + stack_args * 8 + struct_copy_total + fptr_extra;
    int needed_mod = (push_depth_ % 2 == 0) ? 0 : 8;
    if (frame % 16 != needed_mod) frame += 8;
    int struct_copy_base = 32 + stack_args * 8;  // offset of struct copy area in call frame
    if (is_indirect_call) {
        fptr_save_offset = frame - fptr_extra;  // at end of frame (before alignment padding)
        if (fptr_extra == 8 && (frame - fptr_extra) < struct_copy_base + struct_copy_total)
            fptr_save_offset = frame - 8;  // use last 8 bytes of frame
    }

    // Allocate call frame
    emit("sub rsp, %d", frame);

    // For indirect calls, evaluate the callee expression NOW (before args are loaded
    // into registers) and save the function address to a frame slot. This prevents
    // the callee expression evaluation from clobbering argument registers.
    if (is_indirect_call) {
        genExpr(node->children[0].get());
        emit("mov QWORD PTR [rsp+%d], rax", fptr_save_offset);
    }

    // 2.1: Track which register args are floats for XMM vs integer register placement
    std::vector<bool> arg_is_float(reg_args, false);
    std::vector<int>  arg_float_sz(reg_args, 8);

    // Evaluate and place stack args (beyond register slots) at [rsp+32+(i-max_reg_args)*8]
    for (int i = max_reg_args; i < nargs; i++) {
        int slot = 32 + (i - max_reg_args) * 8;
        if (arg_struct_sz_v[i] > 0) {
            // Struct >8B: copy to frame, store address in slot
            int copy_off = struct_copy_base + arg_struct_copy_off[i];
            genLValue(args[i]);
            emit("mov rbx, rax");
            emit("lea rcx, [rsp+%d]", copy_off);
            int qwords2 = arg_struct_sz_v[i] / 8, rem2 = arg_struct_sz_v[i] % 8;
            for (int q = 0; q < qwords2; q++) {
                emit("mov rdx, QWORD PTR [rbx+%d]", q*8);
                emit("mov QWORD PTR [rcx+%d], rdx", q*8);
            }
            if (rem2 >= 4) { emit("mov edx, DWORD PTR [rbx+%d]", qwords2*8); emit("mov DWORD PTR [rcx+%d], edx", qwords2*8); }
            emit("mov QWORD PTR [rsp+%d], rcx", slot);
        } else {
            TypeInfo spt2 = getParamType(fname, i);
            if (spt2.base == CType::Struct && spt2.size <= 8) {
                // Struct <=8B: load as QWORD
                genLValue(args[i]);
                emit("mov rax, QWORD PTR [rax]");
            } else {
                genExpr(args[i]);
            }
            if (last_expr_is_float_) {
                if (last_float_size_ == 4) emit("movss DWORD PTR [rsp+%d], xmm0", slot);
                else                       emit("movsd QWORD PTR [rsp+%d], xmm0", slot);
            } else {
                emit("mov QWORD PTR [rsp+%d], rax", slot);
            }
        }
    }

    // Evaluate register args right-to-left, store in shifted shadow slots as temp
    for (int i = reg_args - 1; i >= 0; i--) {
        int shadow_slot = (i + arg_reg_shift) * 8;
        if (arg_struct_sz_v[i] > 0) {
            // Struct >8B: copy to frame, store address in shadow slot
            int copy_off = struct_copy_base + arg_struct_copy_off[i];
            genLValue(args[i]);
            emit("mov rbx, rax");
            emit("lea rcx, [rsp+%d]", copy_off);
            int qwords3 = arg_struct_sz_v[i] / 8, rem3 = arg_struct_sz_v[i] % 8;
            for (int q = 0; q < qwords3; q++) {
                emit("mov rdx, QWORD PTR [rbx+%d]", q*8);
                emit("mov QWORD PTR [rcx+%d], rdx", q*8);
            }
            if (rem3 >= 4) { emit("mov edx, DWORD PTR [rbx+%d]", qwords3*8); emit("mov DWORD PTR [rcx+%d], edx", qwords3*8); }
            emit("mov QWORD PTR [rsp+%d], rcx", shadow_slot);
            arg_is_float[i] = false;
        } else {
            TypeInfo spt3 = getParamType(fname, i);
            if (spt3.base == CType::Struct && spt3.size <= 8) {
                // Struct <=8B: load as QWORD, store in shadow slot
                genLValue(args[i]);
                emit("mov rax, QWORD PTR [rax]");
                emit("mov QWORD PTR [rsp+%d], rax", shadow_slot);
                arg_is_float[i] = false;
            } else {
                genExpr(args[i]);
                arg_is_float[i] = last_expr_is_float_;
                arg_float_sz[i] = last_float_size_;
                if (last_expr_is_float_) {
                    if (last_float_size_ == 4) emit("movss DWORD PTR [rsp+%d], xmm0", shadow_slot);
                    else                       emit("movsd QWORD PTR [rsp+%d], xmm0", shadow_slot);
                } else {
                    emit("mov QWORD PTR [rsp+%d], rax", shadow_slot);
                }
            }
        }
    }
    // Load register args into RCX/RDX/R8/R9 (or XMM0-3 for floats), shifted by arg_reg_shift
    static const char* xmm_regs[] = { "xmm0", "xmm1", "xmm2", "xmm3" };
    for (int i = 0; i < reg_args; i++) {
        int ri = i + arg_reg_shift;
        int shadow_slot2 = ri * 8;
        if (arg_is_float[i]) {
            if (arg_float_sz[i] == 4) emit("movss %s, DWORD PTR [rsp+%d]", xmm_regs[ri], shadow_slot2);
            else                      emit("movsd %s, QWORD PTR [rsp+%d]", xmm_regs[ri], shadow_slot2);
        } else {
            emit("mov %s, QWORD PTR [rsp+%d]", arg_regs[ri], shadow_slot2);
        }
    }
    // 1.1: If callee returns large struct, load hidden pointer into RCX (slot 0)
    if (callee_ret_large) {
        emit("lea rcx, [rbp%+d]", ret_perm_slot);
        emit("mov QWORD PTR [rsp+0], rcx");
    }
    // 1.2: For variadic calls, mirror float XMM values to integer registers so
    // the callee can home them to shadow space for va_arg access.
    {
        bool callee_is_variadic = false;
        if (!fname.empty()) {
            auto ffit2 = functions_.find(fname);
            if (ffit2 != functions_.end()) callee_is_variadic = ffit2->second.is_variadic;
        }
        if (callee_is_variadic) {
            for (int i = 0; i < reg_args; i++) {
                int ri = i + arg_reg_shift;
                if (arg_is_float[i])
                    emit("movq %s, %s", arg_regs[ri], xmm_regs[ri]);
            }
        }
    }

    // [[deprecated]] warning: warn on any use of a deprecated function
    if (!fname.empty()) {
        auto fit = functions_.find(fname);
        if (fit != functions_.end() && fit->second.is_deprecated) {
            const std::string& dmsg = fit->second.deprecated_msg;
            if (dmsg.empty())
                fprintf(stderr, "%s:%d:%d: warning: '%s' is deprecated\n",
                        source_file_.c_str(), node->line, node->col, fname.c_str());
            else
                fprintf(stderr, "%s:%d:%d: warning: '%s' is deprecated: %s\n",
                        source_file_.c_str(), node->line, node->col, fname.c_str(), dmsg.c_str());
        }
    }

    // Emit the call: either direct or indirect (through saved function pointer)
    if (!is_indirect_call) {
        // 4.3: Implicit function declaration — warn at c89/c17, error at c99+
        if (functions_.find(fname) == functions_.end()) {
            if (g_std_level >= 99) {
                fprintf(stderr, "%s%s:%d:%d: error: implicit declaration of function '%s' is invalid in C99+%s\n",
                        cg_err(), source_file_.c_str(), node->line, node->col, fname.c_str(), cg_reset());
            } else {
                fprintf(stderr, "%s%s:%d:%d: warning: implicit declaration of function '%s'%s\n",
                        cg_warn(), source_file_.c_str(), node->line, node->col, fname.c_str(), cg_reset());
            }
        }
        called_functions_.insert(fname);
        emit("call %s", fname.c_str());
    } else {
        // Indirect call: load saved function pointer from frame slot
        emit("mov rax, QWORD PTR [rsp+%d]", fptr_save_offset);
        emit("call rax");
    }
    emit("add rsp, %d", frame);

    // Set last_expr_is_float_ based on function return type.
    // After a call, the result is in rax (integer) or xmm0 (float/double).
    // Callers (e.g. genAssign) use last_expr_is_float_ to decide how to handle
    // the return value.  Without this, float-returning functions like strtod()
    // will have their xmm0 result overwritten by cvtsi2ss when assigned to a
    // float/double variable.
    last_expr_is_float_ = false;  // default: integer return
    last_expr_is_ptr_   = false;
    last_expr_struct_size_ = 0;   // 1.1: default: not a struct return
    // 1.1: Large struct return — result is in permanent local slot; load its address into RAX
    if (callee_ret_large) {
        emit("lea rax, [rbp%+d]", ret_perm_slot);
        last_expr_struct_size_ = callee_ret_struct_sz;
    } else if (!fname.empty()) {
        auto fit = functions_.find(fname);
        if (fit != functions_.end()) {
            if (fit->second.return_type.base == CType::Float) {
                last_expr_is_float_ = true;
                last_float_size_ = 4;
            } else if (fit->second.return_type.base == CType::Double) {
                last_expr_is_float_ = true;
                last_float_size_ = 8;
            } else if (fit->second.return_type.base == CType::Ptr) {
                last_expr_is_ptr_ = true;
            } else if (fit->second.return_type.base == CType::Struct) {
                // 1.1: Struct return — track size for RAX:RDX (9-16 byte) handling
                int sz = fit->second.return_type.size;
                if (sz > 0 && sz <= 16) last_expr_struct_size_ = sz;
            }
        }
    }
}

void CodeGen::genSubscript(ASTNode* node) {
    // Check for multi-dimensional array access (e.g., arr[i][j])
    // Walk nested SubscriptExprs to find root array and collect indices
    std::vector<ASTNode*> indices;
    ASTNode* base = node;
    while (base->kind == NodeKind::SubscriptExpr && base->children.size() >= 2) {
        indices.push_back(base->children[1].get());
        base = base->children[0].get();
    }
    // indices are collected from outermost to innermost, reverse for logical order
    std::reverse(indices.begin(), indices.end());

    // Check if the root base is a multi-dim array
    std::vector<int> dims;
    if (base->kind == NodeKind::Ident) {
        auto it = locals_.find(base->sval);
        if (it != locals_.end() && it->second.type.array_dims.size() > 1) {
            dims = it->second.type.array_dims;
        } else {
            auto git = findGlobal(base->sval);
            if (git != globals_.end() && git->second.type.array_dims.size() > 1) {
                dims = git->second.type.array_dims;
            }
        }
    }

    if (dims.size() > 1 && indices.size() == dims.size()) {
        // Multi-dimensional array: compute flat offset
        // For arr[D0][D1]...[Dn], offset = (...((i0*D1 + i1)*D2 + i2)...)*Dn + in) * elem_size
        int elem_size = getExprPointeeSize(base);

        // Compute flat index: start with first index
        genExpr(indices[0]);
        for (size_t k = 1; k < indices.size(); k++) {
            emit("push rax");
            emit("mov rax, %d", dims[k]); // multiply by dimension k
            emit("pop rcx");
            emit("imul rax, rcx");
            emit("push rax");
            genExpr(indices[k]);
            emit("pop rcx");
            emit("add rax, rcx");
        }
        // rax = flat index, now compute byte offset
        emit("push rax");
        genExpr(base); // base address
        emit("pop rcx");
        emitPtrScale("rcx", elem_size); // 1.4: correct stride for struct arrays
        emit("add rax, rcx");
        // Dereference — use movzx for unsigned types, movsx for signed
        bool eu = getExprType(node).is_unsigned;
        if (elem_size == 1) {
            if (eu) emit("movzx eax, BYTE PTR [rax]");
            else emit("movsx rax, BYTE PTR [rax]");
        } else if (elem_size == 2) {
            if (eu) emit("movzx eax, WORD PTR [rax]");
            else emit("movsx eax, WORD PTR [rax]");
        } else if (elem_size == 4) {
            if (eu) emit("mov eax, DWORD PTR [rax]");
            else emit("movsxd rax, DWORD PTR [rax]");
        } else {
            emit("mov rax, QWORD PTR [rax]");
        }
        last_expr_is_ptr_ = (elem_size == 8);
        last_expr_is_float_ = false;
        last_expr_is_unsigned_ = eu;
        return;
    }

    // Single-dimension array or pointer subscript (original logic)
    int elem_size = getExprPointeeSize(node->children[0].get());
    bool elem_is_unsigned = getExprType(node).is_unsigned;
    genExpr(node->children[1].get()); // index
    emit("push rax");
    genExpr(node->children[0].get()); // base
    emit("pop rcx");
    emitPtrScale("rcx", elem_size); // 1.4: correct stride for struct arrays
    emit("add rax, rcx");
    if (elem_size == 1) {
        if (elem_is_unsigned) emit("movzx eax, BYTE PTR [rax]");
        else emit("movsx rax, BYTE PTR [rax]");
    } else if (elem_size == 2) {
        if (elem_is_unsigned) emit("movzx eax, WORD PTR [rax]");
        else emit("movsx eax, WORD PTR [rax]");
    } else if (elem_size == 4) {
        if (elem_is_unsigned) emit("mov eax, DWORD PTR [rax]");
        else emit("movsxd rax, DWORD PTR [rax]");
    } else {
        emit("mov rax, QWORD PTR [rax]");
    }
    // Result is an element value, not a pointer (unless element is a pointer type)
    last_expr_is_ptr_ = (elem_size == 8);
    last_expr_is_float_ = false;
    last_expr_is_unsigned_ = elem_is_unsigned;
}

int CodeGen::findMemberOffset(const std::string& member_name) {
    for (auto& [sname, layout] : structs_) {
        for (auto& m : layout.members) {
            if (m.name == member_name) return m.offset;
        }
    }
    return 0;
}

void CodeGen::genMember(ASTNode* node) {
    std::string member;
    bool is_arrow = false;
    if (node->sval.size() > 2 && node->sval.substr(0, 2) == "->") {
        member = node->sval.substr(2);
        is_arrow = true;
    } else if (node->sval.size() > 1 && node->sval[0] == '.') {
        member = node->sval.substr(1);
    } else if (node->children.size() > 1 && node->children[1]->kind == NodeKind::Ident) {
        member = node->children[1]->sval;
    } else {
        member = node->sval;
    }

    // Find the member info (offset and type)
    int offset = 0;
    int member_size = 8;
    bool member_is_unsigned = false;
    bool member_is_float = false;
    bool member_is_double = false;
    bool member_is_ptr = false;
    int member_pointee_size = 8;
    int member_bit_offset = 0;
    int member_bit_width  = 0;
    bool member_is_array = false;

    // Determine the struct type of the base expression to look up member in the RIGHT struct.
    // Without this, ambiguous member names (same name in multiple structs) pick the wrong one.
    // Use getExprType() which recursively resolves type through MemberExpr/SubscriptExpr chains.
    std::string lhs_struct_name;
    {
        ASTNode* lhs = !node->children.empty() ? node->children[0].get() : nullptr;
        if (lhs) {
            TypeInfo lhs_type = getExprType(lhs);
            // For arrow (->) access, the lhs type is a pointer to the struct
            if (is_arrow && lhs_type.base == CType::Ptr && lhs_type.pointee) {
                lhs_type = *lhs_type.pointee;
            }
            if (lhs_type.base == CType::Struct && !lhs_type.struct_name.empty()) {
                lhs_struct_name = lhs_type.struct_name;
            }
        }
    }

    // First: try to find member in the specific struct we determined for the lhs
    if (!lhs_struct_name.empty()) {
        auto sit = structs_.find(lhs_struct_name);
        if (sit != structs_.end()) {
            for (auto& m : sit->second.members) {
                if (m.name == member) {
                    offset = m.offset;
                    member_size = m.type.size;
                    member_is_unsigned = m.type.is_unsigned;
                    member_is_float = (m.type.base == CType::Float || m.type.base == CType::Double);
                    member_is_double = (m.type.base == CType::Double);
                    member_is_ptr = (m.type.base == CType::Ptr);
                    member_is_array = (m.type.base == CType::Array);
                    if (member_is_ptr && m.type.pointee)
                        member_pointee_size = m.type.pointee->size;
                    if (member_is_array && m.type.pointee)
                        member_pointee_size = m.type.pointee->size;
                    member_bit_offset = m.bit_offset;
                    member_bit_width  = m.bit_width;
                    goto found;
                }
            }
        }
    }

    // Fallback: search all structs (for cases where we can't determine the type)
    for (auto& [sname, layout] : structs_) {
        for (auto& m : layout.members) {
            if (m.name == member) {
                offset = m.offset;
                member_size = m.type.size;
                member_is_unsigned = m.type.is_unsigned;
                member_is_float = (m.type.base == CType::Float || m.type.base == CType::Double);
                member_is_double = (m.type.base == CType::Double);
                member_is_ptr = (m.type.base == CType::Ptr);
                member_is_array = (m.type.base == CType::Array);
                if (member_is_ptr && m.type.pointee)
                    member_pointee_size = m.type.pointee->size;
                if (member_is_array && m.type.pointee)
                    member_pointee_size = m.type.pointee->size;
                member_bit_offset = m.bit_offset;
                member_bit_width  = m.bit_width;
                goto found;
            }
        }
    }
    found:

    if (is_arrow) {
        genExpr(node->children[0].get());
    } else {
        genLValue(node->children[0].get());
    }
    if (offset != 0) emit("add rax, %d", offset);

    // Array member: decay to pointer (return address, don't dereference)
    if (member_is_array) {
        last_expr_is_ptr_ = true;
        last_expr_is_float_ = false;
        last_pointee_size_ = member_pointee_size;
        return;
    }

    // Load with correct width
    if (member_is_double) {
        emit("movsd xmm0, QWORD PTR [rax]");
        last_expr_is_float_ = true;
        last_float_size_ = 8;
    } else if (member_is_float) {
        emit("movss xmm0, DWORD PTR [rax]");
        // Also put in eax for integer contexts
        emit("mov eax, DWORD PTR [rax]");
        last_expr_is_float_ = true;
        last_float_size_ = 4;
    } else if (member_size == 1) {
        if (member_is_unsigned)
            emit("movzx eax, BYTE PTR [rax]");
        else
            emit("movsx eax, BYTE PTR [rax]");
        last_expr_is_float_ = false;
    } else if (member_size == 2) {
        if (member_is_unsigned)
            emit("movzx eax, WORD PTR [rax]");
        else
            emit("movsx eax, WORD PTR [rax]");
        last_expr_is_float_ = false;
    } else if (member_size == 4) {
        if (member_is_unsigned)
            emit("mov eax, DWORD PTR [rax]");
        else
            emit("movsxd rax, DWORD PTR [rax]");
        last_expr_is_float_ = false;
    } else {
        emit("mov rax, QWORD PTR [rax]");
        last_expr_is_float_ = false;
    }

    // Bit-field extraction: shift right by bit_offset, mask to bit_width bits
    if (member_bit_width > 0) {
        if (member_bit_offset > 0)
            emit("shr eax, %d", member_bit_offset);
        long long mask = (1LL << member_bit_width) - 1;
        emit("and eax, %lld", mask);
        // Sign-extend if signed type and width < 32
        if (!member_is_unsigned && member_bit_width < 32) {
            int shift = 32 - member_bit_width;
            emit("shl eax, %d", shift);
            emit("sar eax, %d", shift);
        }
        emit("movsxd rax, eax");
    }

    // Set pointer tracking flags based on the member type
    last_expr_is_ptr_ = member_is_ptr;
    last_pointee_size_ = member_is_ptr ? member_pointee_size : 8;
}
