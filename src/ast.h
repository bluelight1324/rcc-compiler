#pragma once
#include <string>
#include <vector>
#include <memory>

enum class NodeKind {
    // Top-level
    TranslationUnit, FunctionDef, Declaration,
    // Types & specifiers
    TypeSpec, StructDef, EnumDef, StructMember,
    StorageClassSpec, TypeQualifier, DeclSpecs,
    // Declarators
    Declarator, DirectDeclarator, ParamDecl, InitDeclarator,
    Pointer, AbstractDeclarator, DirectAbstractDeclarator,
    // Statements
    CompoundStmt, IfStmt, WhileStmt, ForStmt, ReturnStmt,
    BreakStmt, ContinueStmt, ExprStmt,
    SwitchStmt, DoWhileStmt, GotoStmt, LabeledStmt, CaseStmt, DefaultStmt,
    // Expressions
    BinaryExpr, UnaryExpr, PostfixExpr, CallExpr,
    MemberExpr, SubscriptExpr, CastExpr, SizeofExpr,
    ConditionalExpr, AssignExpr, CommaExpr,
    Generic,    // C11/C23 _Generic(ctrl, type:val, ..., default:val)
    // Leaves
    Ident, IntLit, StrLit, CharLit, FloatLit,
    // Lists (internal)
    ParamList, ArgList, DeclList, EnumList, Enumerator,
    BlockItems, InitializerList, StructDeclList,
};

struct ASTNode {
    NodeKind kind;
    std::string sval;          // identifier name, string literal, operator
    long long ival;            // integer value
    int line, col;
    std::vector<std::unique_ptr<ASTNode>> children;

    ASTNode(NodeKind k, int l = 0, int c = 0)
        : kind(k), ival(0), line(l), col(c) {}

    void add(std::unique_ptr<ASTNode> child) {
        if (child) children.push_back(std::move(child));
    }

    static std::unique_ptr<ASTNode> make(NodeKind k, int l = 0, int c = 0) {
        return std::make_unique<ASTNode>(k, l, c);
    }
};

using ASTPtr = std::unique_ptr<ASTNode>;
