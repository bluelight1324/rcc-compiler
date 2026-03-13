#include "parser.h"
#include <cstdio>
#include <cstdlib>

// Number of productions and states
#define NUM_PRODUCTIONS 240
#define NUM_STATES 403

Parser::Parser(Lexer& lex) : lex_(lex) {
    curtok_ = lex_.next();
}

int Parser::lookupAction(int state, int token) {
    const int* row = __SynAction0[state];
    if (!row) return -9999; // error
    int count = row[0];
    for (int i = 0; i < count; i++) {
        if (row[1 + i * 2] == token)
            return row[1 + i * 2 + 1];
    }
    return -9999; // error - no entry
}

int Parser::lookupGoto(int state, int nt) {
    const int* row = __SynGoto0[state];
    if (!row) return -1;
    int count = row[0];
    for (int i = 0; i < count; i++) {
        if (row[1 + i * 2] == nt)
            return row[1 + i * 2 + 1];
    }
    return -1;
}

void Parser::error(const char* msg) {
    fprintf(stderr, "%s:%d:%d: error: %s (token: %s)\n",
            lex_.filename(), curtok_.line, curtok_.col, msg,
            __SynYy_stok0[curtok_.type]);
    error_count_++;
    if (error_count_ > 20) {
        fprintf(stderr, "error: too many errors (20); stopping compilation\n");
        exit(1);
    }
}

// 4.1: Panic-mode error recovery — skip tokens until `;` or `}`, reset state
void Parser::panicRecover() {
    int depth = 0;
    while (curtok_.type != TOK_EOT && curtok_.type != TOK_SOT) {
        if (curtok_.type == TOK_LBRACE) {
            depth++;
            curtok_ = lex_.next();
        } else if (curtok_.type == TOK_RBRACE) {
            if (depth > 0) {
                depth--;
                curtok_ = lex_.next();
            } else {
                curtok_ = lex_.next(); // consume `}` at depth 0
                break;
            }
        } else if (curtok_.type == TOK_SEMICOLON && depth == 0) {
            curtok_ = lex_.next(); // consume `;`
            break;
        } else {
            curtok_ = lex_.next();
        }
    }
    // Reset LALR stacks to initial state so parsing can resume from the next decl
    stateStack_.clear();
    stateStack_.push_back(0);
    valueStack_.clear();
    valueStack_.push_back(ParseValue{});
}

// Production numbering matches grammar order in c_subset.y (0-239)

ParseValue Parser::reduce(int prod) {
    int rhs_len = __SynReduce0[prod];
    int base = (int)valueStack_.size() - rhs_len;
    ParseValue result;
    result.line = base >= 0 && base < (int)valueStack_.size() ? valueStack_[base].line : 0;
    result.col = base >= 0 && base < (int)valueStack_.size() ? valueStack_[base].col : 0;
    result.ival = 0;

    auto& vs = valueStack_;
    #define V(i) vs[base + (i)]

    switch (prod) {
    case 0: // start -> translation_unit
        result.node = std::move(V(0).node);
        break;

    // primary_expr
    case 1: // -> IDENTIFIER
    {
        auto e = ASTNode::make(NodeKind::Ident, V(0).line, V(0).col);
        e->sval = V(0).text;
        result.node = std::move(e);
        break;
    }
    case 2: // -> INTEGER_CONSTANT
    {
        auto e = ASTNode::make(NodeKind::IntLit, V(0).line, V(0).col);
        e->ival = V(0).ival;
        result.node = std::move(e);
        break;
    }
    case 3: // -> FLOATING_CONSTANT
    {
        auto e = ASTNode::make(NodeKind::FloatLit, V(0).line, V(0).col);
        e->sval = V(0).text;
        result.node = std::move(e);
        break;
    }
    case 4: // -> CHARACTER_CONSTANT
    {
        auto e = ASTNode::make(NodeKind::CharLit, V(0).line, V(0).col);
        e->ival = V(0).ival;
        result.node = std::move(e);
        break;
    }
    case 5: // -> STRING_LITERAL
    {
        auto e = ASTNode::make(NodeKind::StrLit, V(0).line, V(0).col);
        e->sval = V(0).text;
        e->ival = V(0).ival; // C11: char width (0/1=byte, 2=UTF-16, 4=UTF-32)
        result.node = std::move(e);
        break;
    }
    case 6: // -> LPAREN expression RPAREN
        result.node = std::move(V(1).node);
        break;

    // postfix_expr
    case 7: // -> primary_expr
        result.node = std::move(V(0).node);
        break;
    case 8: // -> postfix_expr LBRACKET expression RBRACKET
    {
        auto e = ASTNode::make(NodeKind::SubscriptExpr, V(0).line, V(0).col);
        e->add(std::move(V(0).node));
        e->add(std::move(V(2).node));
        result.node = std::move(e);
        break;
    }
    case 9: // -> postfix_expr LPAREN RPAREN
    {
        auto e = ASTNode::make(NodeKind::CallExpr, V(0).line, V(0).col);
        e->add(std::move(V(0).node));
        result.node = std::move(e);
        break;
    }
    case 10: // -> postfix_expr LPAREN arg_expr_list RPAREN
    {
        auto e = ASTNode::make(NodeKind::CallExpr, V(0).line, V(0).col);
        e->add(std::move(V(0).node));
        e->add(std::move(V(2).node));
        result.node = std::move(e);
        break;
    }
    case 11: // -> postfix_expr DOT IDENTIFIER
    {
        auto e = ASTNode::make(NodeKind::MemberExpr, V(0).line, V(0).col);
        e->sval = "." + std::string(V(2).text);
        e->add(std::move(V(0).node));
        result.node = std::move(e);
        break;
    }
    case 12: // -> postfix_expr PTR_OP IDENTIFIER
    {
        auto e = ASTNode::make(NodeKind::MemberExpr, V(0).line, V(0).col);
        e->sval = "->" + std::string(V(2).text);
        e->add(std::move(V(0).node));
        result.node = std::move(e);
        break;
    }
    case 13: // -> postfix_expr INC_OP
    {
        auto e = ASTNode::make(NodeKind::PostfixExpr, V(0).line, V(0).col);
        e->sval = "++";
        e->add(std::move(V(0).node));
        result.node = std::move(e);
        break;
    }
    case 14: // -> postfix_expr DEC_OP
    {
        auto e = ASTNode::make(NodeKind::PostfixExpr, V(0).line, V(0).col);
        e->sval = "--";
        e->add(std::move(V(0).node));
        result.node = std::move(e);
        break;
    }
    case 15: // -> LPAREN type_name RPAREN LBRACE initializer_list RBRACE (compound literal)
    {
        auto node = ASTNode::make(NodeKind::CastExpr, V(0).line, V(0).col);
        node->add(std::move(V(1).node)); // type_name
        node->add(std::move(V(4).node)); // initializer_list
        result.node = std::move(node);
        break;
    }
    case 16: // -> LPAREN type_name RPAREN LBRACE initializer_list COMMA RBRACE (compound literal, trailing comma)
    {
        auto node = ASTNode::make(NodeKind::CastExpr, V(0).line, V(0).col);
        node->add(std::move(V(1).node)); // type_name
        node->add(std::move(V(4).node)); // initializer_list
        result.node = std::move(node);
        break;
    }

    // arg_expr_list
    case 17: // -> assignment_expr
    {
        auto al = ASTNode::make(NodeKind::ArgList, V(0).line, V(0).col);
        al->add(std::move(V(0).node));
        result.node = std::move(al);
        break;
    }
    case 18: // -> arg_expr_list COMMA assignment_expr
        V(0).node->add(std::move(V(2).node));
        result.node = std::move(V(0).node);
        break;

    // unary_expr
    case 19: // -> postfix_expr
        result.node = std::move(V(0).node);
        break;
    case 20: // -> INC_OP unary_expr
    case 21: // -> DEC_OP unary_expr
    {
        auto e = ASTNode::make(NodeKind::UnaryExpr, V(0).line, V(0).col);
        e->sval = V(0).text;
        e->add(std::move(V(1).node));
        result.node = std::move(e);
        break;
    }
    case 22: // -> unary_op cast_expr
    {
        auto e = ASTNode::make(NodeKind::UnaryExpr, V(0).line, V(0).col);
        e->sval = V(0).text;
        e->add(std::move(V(1).node));
        result.node = std::move(e);
        break;
    }
    case 23: // -> SIZEOF unary_expr (or typeof unary_expr)
    {
        auto e = ASTNode::make(NodeKind::SizeofExpr, V(0).line, V(0).col);
        // Check if this is typeof (C23) vs sizeof - store in sval
        if (V(0).text.size() >= 6 && V(0).text.substr(0, 6) == "typeof") {
            e->sval = "typeof";
        } else {
            e->sval = "sizeof";
        }
        e->add(std::move(V(1).node));
        result.node = std::move(e);
        break;
    }
    case 24: // -> SIZEOF LPAREN type_name RPAREN (or typeof)
    {
        auto e = ASTNode::make(NodeKind::SizeofExpr, V(0).line, V(0).col);
        // Check if this is typeof (C23) vs sizeof - store in sval
        if (V(0).text.size() >= 6 && V(0).text.substr(0, 6) == "typeof") {
            e->sval = "typeof";
        } else {
            e->sval = "sizeof";
        }
        e->add(std::move(V(2).node));
        result.node = std::move(e);
        break;
    }

    // unary_op -> terminal (pass through operator text)
    case 25: case 26: case 27: case 28: case 29: case 30:
    {
        static const char* uops[] = { "&", "*", "+", "-", "~", "!" };
        result.text = uops[prod - 25];
        result.line = V(0).line;
        result.col = V(0).col;
        break;
    }

    // cast_expr
    case 31: // -> unary_expr
        result.node = std::move(V(0).node);
        break;
    case 32: // -> LPAREN type_name RPAREN cast_expr
    {
        auto e = ASTNode::make(NodeKind::CastExpr, V(0).line, V(0).col);
        e->add(std::move(V(1).node));
        e->add(std::move(V(3).node));
        result.node = std::move(e);
        break;
    }

    // Binary expression pass-throughs
    case 33: case 37: case 40: case 43: case 48: case 51: case 53: case 55: case 57: case 59: case 61:
        result.node = std::move(V(0).node);
        break;

    // Binary operators (all A op B -> BinaryExpr)
    case 34: case 35: case 36: // multiplicative
    case 38: case 39:          // additive
    case 41: case 42:          // shift
    case 44: case 45: case 46: case 47: // relational
    case 49: case 50:          // equality
    case 52:                   // bitwise and
    case 54:                   // bitwise xor
    case 56:                   // bitwise or
    case 58:                   // logical and
    case 60:                   // logical or
    {
        auto e = ASTNode::make(NodeKind::BinaryExpr, V(0).line, V(0).col);
        e->sval = V(1).text;
        e->add(std::move(V(0).node));
        e->add(std::move(V(2).node));
        result.node = std::move(e);
        break;
    }

    // conditional_expr
    case 62: // -> logical_or QUESTION expression COLON conditional_expr
    {
        auto e = ASTNode::make(NodeKind::ConditionalExpr, V(0).line, V(0).col);
        e->add(std::move(V(0).node));
        e->add(std::move(V(2).node));
        e->add(std::move(V(4).node));
        result.node = std::move(e);
        break;
    }

    // assignment_expr
    case 63: // -> conditional_expr
        result.node = std::move(V(0).node);
        break;
    case 64: // -> unary_expr assign_op assignment_expr
    {
        auto e = ASTNode::make(NodeKind::AssignExpr, V(0).line, V(0).col);
        e->sval = V(1).text;
        e->add(std::move(V(0).node));
        e->add(std::move(V(2).node));
        result.node = std::move(e);
        break;
    }

    // assign_op -> terminal (pass through text)
    case 65: case 66: case 67: case 68: case 69: case 70:
    case 71: case 72: case 73: case 74: case 75:
    {
        static const char* aops[] = { "=", "*=", "/=", "%=", "+=", "-=", "<<=", ">>=", "&=", "^=", "|=" };
        result.text = aops[prod - 65];
        break;
    }

    // expression
    case 76: // -> assignment_expr
        result.node = std::move(V(0).node);
        break;
    case 77: // -> expression COMMA assignment_expr
    {
        auto e = ASTNode::make(NodeKind::CommaExpr, V(0).line, V(0).col);
        e->add(std::move(V(0).node));
        e->add(std::move(V(2).node));
        result.node = std::move(e);
        break;
    }

    // constant_expr
    case 78:
        result.node = std::move(V(0).node);
        break;

    // declaration
    case 79: // -> decl_specs SEMICOLON
    {
        auto decl = ASTNode::make(NodeKind::Declaration, V(0).line, V(0).col);
        decl->add(std::move(V(0).node));
        result.node = std::move(decl);
        break;
    }
    case 80: // -> decl_specs init_declarator_list SEMICOLON
    {
        // Check if this is a typedef declaration and register the name
        bool is_typedef = false;
        if (V(0).node) {
            for (auto& child : V(0).node->children) {
                if (child->kind == NodeKind::StorageClassSpec && child->sval == "typedef") {
                    is_typedef = true;
                    break;
                }
            }
        }
        if (is_typedef && V(1).node) {
            // Extract declared name(s) from init_declarator_list
            for (auto& idecl : V(1).node->children) {
                // idecl is InitDeclarator, first child is Declarator
                ASTNode* d = nullptr;
                if (idecl && !idecl->children.empty())
                    d = idecl->children[0].get();
                // Walk through pointer declarators to find the name
                while (d) {
                    if (d->kind == NodeKind::Declarator && d->sval != "*" &&
                        d->sval != "()" && d->sval != "[]" && d->sval != "[N]" &&
                        !d->sval.empty()) {
                        lex_.addTypeName(d->sval);
                        break;
                    }
                    // pointer declarator: sval="*", children[1] is inner declarator
                    if (d->kind == NodeKind::Declarator && d->sval == "*" && d->children.size() > 1)
                        d = d->children[1].get();
                    else if (!d->children.empty())
                        d = d->children[0].get();
                    else
                        break;
                }
            }
        }
        // Fix lookahead: if current token is IDENTIFIER matching a just-registered typedef, update it
        if (is_typedef && curtok_.type == TOK_IDENTIFIER && curtok_.text) {
            const char* p = curtok_.text;
            size_t len = 0;
            while ((p[len] >= 'a' && p[len] <= 'z') || (p[len] >= 'A' && p[len] <= 'Z') ||
                   (p[len] >= '0' && p[len] <= '9') || p[len] == '_') len++;
            std::string name(curtok_.text, len);
            if (lex_.isTypeName(name)) {
                curtok_.type = TOK_TYPE_NAME;
            }
        }
        auto decl = ASTNode::make(NodeKind::Declaration, V(0).line, V(0).col);
        decl->add(std::move(V(0).node));
        decl->add(std::move(V(1).node));
        result.node = std::move(decl);
        break;
    }

    // decl_specs (81-86 unchanged; 87=alignment_specifier, 88=alignment_specifier decl_specs)
    case 81: case 83: case 85: case 87:
    {
        auto ds = ASTNode::make(NodeKind::DeclSpecs, V(0).line, V(0).col);
        ds->add(std::move(V(0).node));
        result.node = std::move(ds);
        break;
    }
    case 82: case 84: case 86: case 88:
    {
        auto& ds = V(1).node;
        ds->children.insert(ds->children.begin(), std::move(V(0).node));
        result.node = std::move(ds);
        break;
    }

    // init_declarator_list (old 87-88 -> new 89-90)
    case 89:
    {
        auto list = ASTNode::make(NodeKind::DeclList, V(0).line, V(0).col);
        list->add(std::move(V(0).node));
        result.node = std::move(list);
        break;
    }
    case 90:
        V(0).node->add(std::move(V(2).node));
        result.node = std::move(V(0).node);
        break;

    // init_declarator (old 89-90 -> new 91-92)
    case 91:
    {
        auto id = ASTNode::make(NodeKind::InitDeclarator, V(0).line, V(0).col);
        id->add(std::move(V(0).node));
        result.node = std::move(id);
        break;
    }
    case 92:
    {
        auto id = ASTNode::make(NodeKind::InitDeclarator, V(0).line, V(0).col);
        id->add(std::move(V(0).node));
        id->add(std::move(V(2).node));
        result.node = std::move(id);
        break;
    }

    // storage_class_spec (old 91-95 -> new 93-97; new 98 = _THREAD_LOCAL)
    case 93: case 94: case 95: case 96: case 97:
    {
        static const char* scs[] = { "typedef", "extern", "static", "auto", "register" };
        auto n = ASTNode::make(NodeKind::StorageClassSpec, V(0).line, V(0).col);
        n->sval = scs[prod - 93];
        result.node = std::move(n);
        break;
    }
    case 98: // storage_class_spec -> _THREAD_LOCAL (C11 §6.7.1)
    {
        auto n = ASTNode::make(NodeKind::StorageClassSpec, V(0).line, V(0).col);
        n->sval = "thread_local";
        result.node = std::move(n);
        break;
    }

    // type_spec (old 96-107 -> new 99-110; new 111 = _ATOMIC LPAREN type_name RPAREN)
    case 99: case 100: case 101: case 102: case 103: case 104: case 105: case 106: case 107:
    {
        auto ts = ASTNode::make(NodeKind::TypeSpec, V(0).line, V(0).col);
        ts->sval = V(0).text;
        result.node = std::move(ts);
        break;
    }
    case 108: case 109: // -> struct_or_union_spec | enum_spec
        result.node = std::move(V(0).node);
        break;
    case 110: // -> TYPE_NAME
    {
        auto ts = ASTNode::make(NodeKind::TypeSpec, V(0).line, V(0).col);
        ts->sval = V(0).text;
        result.node = std::move(ts);
        break;
    }
    case 111: // type_spec -> _ATOMIC LPAREN type_name RPAREN (C11 §6.7.2.4)
    {
        // Non-atomic stub: _Atomic(T) is treated as T
        // type_name (V(2)) reduces to a DeclSpecs node
        auto ts = ASTNode::make(NodeKind::TypeSpec, V(0).line, V(0).col);
        ts->sval = "_Atomic";
        ts->add(std::move(V(2).node));
        result.node = std::move(ts);
        break;
    }

    // struct_or_union_spec (old 108-112 -> new 112-116)
    case 112: // -> struct_or_union IDENTIFIER LBRACE struct_decl_list RBRACE
    {
        auto sd = ASTNode::make(NodeKind::StructDef, V(0).line, V(0).col);
        sd->sval = V(0).text + std::string(" ") + V(1).text;
        sd->add(std::move(V(3).node));
        result.node = std::move(sd);
        break;
    }
    case 113: // -> struct_or_union LBRACE struct_decl_list RBRACE
    {
        auto sd = ASTNode::make(NodeKind::StructDef, V(0).line, V(0).col);
        sd->sval = V(0).text;
        sd->add(std::move(V(2).node));
        result.node = std::move(sd);
        break;
    }
    case 114: // -> struct_or_union IDENTIFIER
    {
        auto ts = ASTNode::make(NodeKind::TypeSpec, V(0).line, V(0).col);
        ts->sval = V(0).text + std::string(" ") + V(1).text;
        result.node = std::move(ts);
        break;
    }
    case 115: // -> struct_or_union TYPE_NAME LBRACE struct_decl_list RBRACE
    {
        auto sd = ASTNode::make(NodeKind::StructDef, V(0).line, V(0).col);
        sd->sval = V(0).text + std::string(" ") + V(1).text;
        sd->add(std::move(V(3).node));
        result.node = std::move(sd);
        break;
    }
    case 116: // -> struct_or_union TYPE_NAME
    {
        auto ts = ASTNode::make(NodeKind::TypeSpec, V(0).line, V(0).col);
        ts->sval = V(0).text + std::string(" ") + V(1).text;
        result.node = std::move(ts);
        break;
    }

    // struct_or_union -> STRUCT | UNION (old 113-114 -> new 117-118)
    case 117: case 118:
        result.text = V(0).text;
        result.line = V(0).line;
        result.col = V(0).col;
        break;

    // struct_decl_list (old 115-116 -> new 119-120)
    case 119:
    {
        auto list = ASTNode::make(NodeKind::StructDeclList, V(0).line, V(0).col);
        list->add(std::move(V(0).node));
        result.node = std::move(list);
        break;
    }
    case 120:
        V(0).node->add(std::move(V(1).node));
        result.node = std::move(V(0).node);
        break;

    // struct_decl (old 117 -> new 121; new 122 = anonymous member)
    case 121: // spec_qualifier_list struct_declarator_list SEMICOLON
    {
        auto sm = ASTNode::make(NodeKind::StructMember, V(0).line, V(0).col);
        sm->add(std::move(V(0).node));
        sm->add(std::move(V(1).node));
        result.node = std::move(sm);
        break;
    }
    case 122: // spec_qualifier_list SEMICOLON — C11 §6.7.2.1 anonymous member
    {
        auto sm = ASTNode::make(NodeKind::StructMember, V(0).line, V(0).col);
        sm->add(std::move(V(0).node));  // DeclSpecs only — no declarator list
        result.node = std::move(sm);
        break;
    }

    // spec_qualifier_list (old 118-121 -> new 123-126)
    case 123: case 125:  // type_spec/type_qualifier + spec_qualifier_list (prepend)
    {
        auto& ds = V(1).node;
        ds->children.insert(ds->children.begin(), std::move(V(0).node));
        result.node = std::move(ds);
        break;
    }
    case 124: case 126:  // type_spec/type_qualifier alone (create DeclSpecs)
    {
        auto ds = ASTNode::make(NodeKind::DeclSpecs, V(0).line, V(0).col);
        ds->add(std::move(V(0).node));
        result.node = std::move(ds);
        break;
    }

    // struct_declarator_list (old 122-123 -> new 127-128)
    case 127:
    {
        auto list = ASTNode::make(NodeKind::DeclList, V(0).line, V(0).col);
        list->add(std::move(V(0).node));
        result.node = std::move(list);
        break;
    }
    case 128:
        V(0).node->add(std::move(V(2).node));
        result.node = std::move(V(0).node);
        break;

    // struct_declarator (old 124-126 -> new 129-131)
    case 129:
        result.node = std::move(V(0).node);
        break;
    case 130: // -> COLON constant_expr
    {
        auto d = ASTNode::make(NodeKind::Declarator, V(0).line, V(0).col);
        d->sval = ":bitfield";
        d->add(std::move(V(1).node));
        result.node = std::move(d);
        break;
    }
    case 131: // -> declarator COLON constant_expr
    {
        V(0).node->sval += ":bitfield";
        V(0).node->add(std::move(V(2).node));
        result.node = std::move(V(0).node);
        break;
    }

    // enum_spec (old 127-134 -> new 132-139)
    case 132: // -> ENUM LBRACE enumerator_list RBRACE
    {
        auto ed = ASTNode::make(NodeKind::EnumDef, V(0).line, V(0).col);
        ed->add(std::move(V(2).node));
        result.node = std::move(ed);
        break;
    }
    case 133: // -> ENUM IDENTIFIER LBRACE enumerator_list RBRACE
    {
        auto ed = ASTNode::make(NodeKind::EnumDef, V(0).line, V(0).col);
        ed->sval = V(1).text;
        ed->add(std::move(V(3).node));
        result.node = std::move(ed);
        break;
    }
    case 134: // -> ENUM IDENTIFIER
    {
        auto ts = ASTNode::make(NodeKind::TypeSpec, V(0).line, V(0).col);
        ts->sval = "enum " + std::string(V(1).text);
        result.node = std::move(ts);
        break;
    }
    case 135: // -> ENUM LBRACE enumerator_list COMMA RBRACE (trailing comma)
    {
        auto ed = ASTNode::make(NodeKind::EnumDef, V(0).line, V(0).col);
        ed->add(std::move(V(2).node));
        result.node = std::move(ed);
        break;
    }
    case 136: // -> ENUM IDENTIFIER LBRACE enumerator_list COMMA RBRACE (trailing comma)
    {
        auto ed = ASTNode::make(NodeKind::EnumDef, V(0).line, V(0).col);
        ed->sval = V(1).text;
        ed->add(std::move(V(3).node));
        result.node = std::move(ed);
        break;
    }
    case 137: // -> ENUM TYPE_NAME LBRACE enumerator_list RBRACE
    {
        auto ed = ASTNode::make(NodeKind::EnumDef, V(0).line, V(0).col);
        ed->sval = V(1).text;
        ed->add(std::move(V(3).node));
        result.node = std::move(ed);
        break;
    }
    case 138: // -> ENUM TYPE_NAME LBRACE enumerator_list COMMA RBRACE (trailing comma)
    {
        auto ed = ASTNode::make(NodeKind::EnumDef, V(0).line, V(0).col);
        ed->sval = V(1).text;
        ed->add(std::move(V(3).node));
        result.node = std::move(ed);
        break;
    }
    case 139: // -> ENUM TYPE_NAME
    {
        auto ts = ASTNode::make(NodeKind::TypeSpec, V(0).line, V(0).col);
        ts->sval = "enum " + std::string(V(1).text);
        result.node = std::move(ts);
        break;
    }

    // enumerator_list (old 135-136 -> new 140-141)
    case 140:
    {
        auto el = ASTNode::make(NodeKind::EnumList, V(0).line, V(0).col);
        el->add(std::move(V(0).node));
        result.node = std::move(el);
        break;
    }
    case 141:
        V(0).node->add(std::move(V(2).node));
        result.node = std::move(V(0).node);
        break;

    // enumerator (old 137-138 -> new 142-143)
    case 142:
    {
        auto e = ASTNode::make(NodeKind::Enumerator, V(0).line, V(0).col);
        e->sval = V(0).text;
        result.node = std::move(e);
        break;
    }
    case 143:
    {
        auto e = ASTNode::make(NodeKind::Enumerator, V(0).line, V(0).col);
        e->sval = V(0).text;
        e->add(std::move(V(2).node));
        result.node = std::move(e);
        break;
    }

    // type_qualifier (old 139-140 -> new 144-145; new 146 = _ATOMIC qualifier)
    case 144: case 145:
    {
        auto tq = ASTNode::make(NodeKind::TypeQualifier, V(0).line, V(0).col);
        tq->sval = V(0).text;
        result.node = std::move(tq);
        break;
    }
    case 146: // type_qualifier -> _ATOMIC (C11 §6.7.3 qualifier form)
    {
        auto tq = ASTNode::make(NodeKind::TypeQualifier, V(0).line, V(0).col);
        tq->sval = "_Atomic";
        result.node = std::move(tq);
        break;
    }

    // declarator (old 141-142 -> new 147-148)
    case 147: // -> pointer direct_declarator
    {
        auto d = ASTNode::make(NodeKind::Declarator, V(0).line, V(0).col);
        d->sval = "*";
        d->add(std::move(V(0).node));
        d->add(std::move(V(1).node));
        result.node = std::move(d);
        break;
    }
    case 148: // -> direct_declarator
        result.node = std::move(V(0).node);
        break;

    // direct_declarator (old 143-150 -> new 149-156)
    case 149: // -> IDENTIFIER
    {
        auto d = ASTNode::make(NodeKind::Declarator, V(0).line, V(0).col);
        d->sval = V(0).text;
        result.node = std::move(d);
        break;
    }
    case 150: // -> TYPE_NAME (as declarator)
    {
        auto d = ASTNode::make(NodeKind::Declarator, V(0).line, V(0).col);
        d->sval = V(0).text;
        result.node = std::move(d);
        break;
    }
    case 151: // -> LPAREN declarator RPAREN
        result.node = std::move(V(1).node);
        break;
    case 152: // -> direct_declarator LBRACKET constant_expr RBRACKET
    {
        auto d = ASTNode::make(NodeKind::Declarator, V(0).line, V(0).col);
        d->sval = "[N]";
        d->add(std::move(V(0).node));
        d->add(std::move(V(2).node));
        result.node = std::move(d);
        break;
    }
    case 153: // -> direct_declarator LBRACKET RBRACKET
    {
        auto d = ASTNode::make(NodeKind::Declarator, V(0).line, V(0).col);
        d->sval = "[]";
        d->add(std::move(V(0).node));
        result.node = std::move(d);
        break;
    }
    case 154: // -> direct_declarator LPAREN param_type_list RPAREN
    {
        auto d = ASTNode::make(NodeKind::Declarator, V(0).line, V(0).col);
        d->sval = "()";
        d->add(std::move(V(0).node));
        d->add(std::move(V(2).node));
        result.node = std::move(d);
        break;
    }
    case 155: // -> direct_declarator LPAREN ident_list RPAREN (K&R)
    {
        auto d = ASTNode::make(NodeKind::Declarator, V(0).line, V(0).col);
        d->sval = "()";
        d->add(std::move(V(0).node));
        d->add(std::move(V(2).node));
        result.node = std::move(d);
        break;
    }
    case 156: // -> direct_declarator LPAREN RPAREN
    {
        auto d = ASTNode::make(NodeKind::Declarator, V(0).line, V(0).col);
        d->sval = "()";
        d->add(std::move(V(0).node));
        result.node = std::move(d);
        break;
    }

    // pointer (old 151-154 -> new 157-160)
    case 157: // -> STAR
    {
        auto p = ASTNode::make(NodeKind::Pointer, V(0).line, V(0).col);
        p->ival = 1;
        result.node = std::move(p);
        break;
    }
    case 158: // -> STAR type_qualifier_list
    {
        auto p = ASTNode::make(NodeKind::Pointer, V(0).line, V(0).col);
        p->ival = 1;
        p->add(std::move(V(1).node));
        result.node = std::move(p);
        break;
    }
    case 159: // -> STAR pointer
    {
        V(1).node->ival++;
        result.node = std::move(V(1).node);
        break;
    }
    case 160: // -> STAR type_qualifier_list pointer
    {
        V(2).node->ival++;
        result.node = std::move(V(2).node);
        break;
    }

    // type_qualifier_list (old 155-156 -> new 161-162)
    case 161:
    {
        auto list = ASTNode::make(NodeKind::DeclList, V(0).line, V(0).col);
        list->add(std::move(V(0).node));
        result.node = std::move(list);
        break;
    }
    case 162:
        V(0).node->add(std::move(V(1).node));
        result.node = std::move(V(0).node);
        break;

    // param_type_list (old 157-158 -> new 163-164)
    case 163:
        result.node = std::move(V(0).node);
        break;
    case 164: // -> param_list COMMA ELLIPSIS
    {
        auto va = ASTNode::make(NodeKind::ParamDecl, V(0).line, V(0).col);
        va->sval = "...";
        V(0).node->add(std::move(va));
        result.node = std::move(V(0).node);
        break;
    }

    // C23 §6.7.6: param_type_list → ELLIPSIS (variadic with no named parameter)
    case 165: // -> ELLIPSIS only
    {
        auto pl = ASTNode::make(NodeKind::ParamList, V(0).line, V(0).col);
        auto va = ASTNode::make(NodeKind::ParamDecl, V(0).line, V(0).col);
        va->sval = "...";
        pl->add(std::move(va));
        result.node = std::move(pl);
        break;
    }

    // param_list (old 159-160 -> new 166-167)
    case 166:
    {
        auto pl = ASTNode::make(NodeKind::ParamList, V(0).line, V(0).col);
        pl->add(std::move(V(0).node));
        result.node = std::move(pl);
        break;
    }
    case 167:
        V(0).node->add(std::move(V(2).node));
        result.node = std::move(V(0).node);
        break;

    // param_decl (old 161-163 -> new 167-169)
    case 168: case 169:
    {
        auto pd = ASTNode::make(NodeKind::ParamDecl, V(0).line, V(0).col);
        pd->add(std::move(V(0).node));
        pd->add(std::move(V(1).node));
        result.node = std::move(pd);
        break;
    }
    case 170:
    {
        auto pd = ASTNode::make(NodeKind::ParamDecl, V(0).line, V(0).col);
        pd->add(std::move(V(0).node));
        result.node = std::move(pd);
        break;
    }

    // ident_list K&R (old 164-165 -> new 170-171)
    case 171:
    {
        auto list = ASTNode::make(NodeKind::DeclList, V(0).line, V(0).col);
        auto id = ASTNode::make(NodeKind::Ident, V(0).line, V(0).col);
        id->sval = V(0).text;
        list->add(std::move(id));
        result.node = std::move(list);
        break;
    }
    case 172:
    {
        auto id = ASTNode::make(NodeKind::Ident, V(2).line, V(2).col);
        id->sval = V(2).text;
        V(0).node->add(std::move(id));
        result.node = std::move(V(0).node);
        break;
    }

    // type_name (old 166-167 -> new 172-173)
    case 173:
        result.node = std::move(V(0).node);
        break;
    case 174:
    {
        auto tn = ASTNode::make(NodeKind::DeclSpecs, V(0).line, V(0).col);
        tn->add(std::move(V(0).node));
        tn->add(std::move(V(1).node));
        result.node = std::move(tn);
        break;
    }

    // abstract_declarator (old 168-170 -> new 174-176)
    case 175: case 176:
        result.node = std::move(V(0).node);
        break;
    case 177:
    {
        auto ad = ASTNode::make(NodeKind::AbstractDeclarator, V(0).line, V(0).col);
        ad->add(std::move(V(0).node));
        ad->add(std::move(V(1).node));
        result.node = std::move(ad);
        break;
    }

    // direct_abstract_declarator (old 171-179 -> new 177-185)
    case 178: // -> LPAREN abstract_declarator RPAREN
        result.node = std::move(V(1).node);
        break;
    case 179: case 180: case 181: case 182: case 183: case 184: case 185: case 186:
    {
        auto dad = ASTNode::make(NodeKind::DirectAbstractDeclarator, V(0).line, V(0).col);
        for (int i = 0; i < rhs_len; i++) {
            if (V(i).node) dad->add(std::move(V(i).node));
        }
        result.node = std::move(dad);
        break;
    }

    // alignment_specifier (new 186-187, C11 §6.7.5)
    case 187: // _ALIGNAS LPAREN type_name RPAREN
    case 188: // _ALIGNAS LPAREN constant_expr RPAREN
    {
        auto as = ASTNode::make(NodeKind::TypeSpec, V(0).line, V(0).col);
        as->sval = "_Alignas";
        as->add(std::move(V(2).node));
        result.node = std::move(as);
        break;
    }

    // initializer (old 180-182 -> new 189/191/192)
    case 189:
        result.node = std::move(V(0).node);
        break;
    case 190: // -> LBRACE RBRACE (C23 §6.7.10: empty initializer — zero-initialise)
    {
        // Empty brace initializer: produce an empty InitializerList.
        // Codegen will zero-initialise the variable when it sees an empty list.
        auto il = ASTNode::make(NodeKind::InitializerList, V(0).line, V(0).col);
        result.node = std::move(il);
        break;
    }
    case 191: // -> LBRACE initializer_list RBRACE
        result.node = std::move(V(1).node);
        break;
    case 192: // -> LBRACE initializer_list COMMA RBRACE
        result.node = std::move(V(1).node);
        break;

    // initializer_list (old 183-186 -> new 191-194)
    case 193:
    {
        auto il = ASTNode::make(NodeKind::InitializerList, V(0).line, V(0).col);
        il->add(std::move(V(0).node));
        result.node = std::move(il);
        break;
    }
    case 194:
        V(0).node->add(std::move(V(2).node));
        result.node = std::move(V(0).node);
        break;

    // initializer_list (designated) - C99 (old 185-186 -> new 193-194)
    case 195: // initializer_list -> designation initializer
    {
        auto il = ASTNode::make(NodeKind::InitializerList, V(0).line, V(0).col);
        il->add(std::move(V(1).node));
        result.node = std::move(il);
        break;
    }
    case 196: // initializer_list -> initializer_list COMMA designation initializer
        V(0).node->add(std::move(V(3).node));
        result.node = std::move(V(0).node);
        break;

    // designation, designator_list, designator (old 187-192 -> new 195-200)
    case 197: // designation -> designator_list ASSIGN
    case 198: // designator_list -> designator
    case 199: // designator_list -> designator_list designator
    case 200: // designator -> LBRACKET constant_expr RBRACKET
    case 201: // designator -> DOT IDENTIFIER
    case 202: // designator -> DOT TYPE_NAME
        result.node = ASTNode::make(NodeKind::TypeSpec, V(0).line, V(0).col);
        result.node->sval = "designator";
        break;

    // statement -> pass through (old 193-198 -> new 201-206)
    case 203: case 204: case 205: case 206: case 207: case 208:
        result.node = std::move(V(0).node);
        break;

    // labeled_stmt (old 199-201 -> new 207-209)
    case 209: // -> IDENTIFIER COLON statement
    {
        auto ls = ASTNode::make(NodeKind::LabeledStmt, V(0).line, V(0).col);
        ls->sval = V(0).text;
        ls->add(std::move(V(2).node));
        result.node = std::move(ls);
        break;
    }
    case 210: // -> IDENTIFIER COLON (C23 §6.8.1: bare label before declaration)
    {
        // Label with no following statement — occurs when a goto target immediately
        // precedes a declaration (C23 §6.8.1 allows labels before declarations).
        auto ls = ASTNode::make(NodeKind::LabeledStmt, V(0).line, V(0).col);
        ls->sval = V(0).text;
        // No child node; codegen emits the label with no attached statement.
        result.node = std::move(ls);
        break;
    }
    case 211: // -> CASE constant_expr COLON statement
    {
        auto cs = ASTNode::make(NodeKind::CaseStmt, V(0).line, V(0).col);
        cs->add(std::move(V(1).node));
        cs->add(std::move(V(3).node));
        result.node = std::move(cs);
        break;
    }
    case 212: // -> DEFAULT COLON statement
    {
        auto ds = ASTNode::make(NodeKind::DefaultStmt, V(0).line, V(0).col);
        ds->add(std::move(V(2).node));
        result.node = std::move(ds);
        break;
    }

    // compound_stmt (old 202-203 -> new 210-211)
    case 213:
        result.node = ASTNode::make(NodeKind::CompoundStmt, V(0).line, V(0).col);
        break;
    case 214:
    {
        auto cs = ASTNode::make(NodeKind::CompoundStmt, V(0).line, V(0).col);
        if (V(1).node) {
            for (auto& c : V(1).node->children)
                cs->add(std::move(c));
        }
        result.node = std::move(cs);
        break;
    }

    // block_item_list (old 204-205 -> new 212-213)
    case 215:
    {
        auto bi = ASTNode::make(NodeKind::BlockItems, V(0).line, V(0).col);
        bi->add(std::move(V(0).node));
        result.node = std::move(bi);
        break;
    }
    case 216:
        V(0).node->add(std::move(V(1).node));
        result.node = std::move(V(0).node);
        break;

    // block_item (old 206-207 -> new 214-215)
    case 217: case 218:
        result.node = std::move(V(0).node);
        break;

    // expression_stmt (old 208-209 -> new 216-217)
    case 219:
        result.node = ASTNode::make(NodeKind::ExprStmt, V(0).line, V(0).col);
        break;
    case 220:
    {
        auto es = ASTNode::make(NodeKind::ExprStmt, V(0).line, V(0).col);
        es->add(std::move(V(0).node));
        result.node = std::move(es);
        break;
    }

    // selection_stmt (old 210-212 -> new 218-220)
    case 221: // IF LPAREN expression RPAREN statement
    {
        auto s = ASTNode::make(NodeKind::IfStmt, V(0).line, V(0).col);
        s->add(std::move(V(2).node));
        s->add(std::move(V(4).node));
        result.node = std::move(s);
        break;
    }
    case 222: // IF LPAREN expression RPAREN statement ELSE statement
    {
        auto s = ASTNode::make(NodeKind::IfStmt, V(0).line, V(0).col);
        s->add(std::move(V(2).node));
        s->add(std::move(V(4).node));
        s->add(std::move(V(6).node));
        result.node = std::move(s);
        break;
    }
    case 223: // SWITCH LPAREN expression RPAREN statement
    {
        auto s = ASTNode::make(NodeKind::SwitchStmt, V(0).line, V(0).col);
        s->add(std::move(V(2).node));
        s->add(std::move(V(4).node));
        result.node = std::move(s);
        break;
    }

    // iteration_stmt (old 213-218 -> new 221-226)
    case 224: // WHILE LPAREN expression RPAREN statement
    {
        auto s = ASTNode::make(NodeKind::WhileStmt, V(0).line, V(0).col);
        s->add(std::move(V(2).node));
        s->add(std::move(V(4).node));
        result.node = std::move(s);
        break;
    }
    case 225: // DO statement WHILE LPAREN expression RPAREN SEMICOLON
    {
        auto s = ASTNode::make(NodeKind::DoWhileStmt, V(0).line, V(0).col);
        s->add(std::move(V(1).node));
        s->add(std::move(V(4).node));
        result.node = std::move(s);
        break;
    }
    case 226: // FOR LPAREN expression_stmt expression_stmt RPAREN statement
    {
        auto s = ASTNode::make(NodeKind::ForStmt, V(0).line, V(0).col);
        s->add(std::move(V(2).node));
        s->add(std::move(V(3).node));
        s->add(ASTNode::make(NodeKind::ExprStmt));
        s->add(std::move(V(5).node));
        result.node = std::move(s);
        break;
    }
    case 227: // FOR LPAREN expression_stmt expression_stmt expression RPAREN statement
    {
        auto s = ASTNode::make(NodeKind::ForStmt, V(0).line, V(0).col);
        s->add(std::move(V(2).node));
        s->add(std::move(V(3).node));
        auto upd = ASTNode::make(NodeKind::ExprStmt, V(4).line, V(4).col);
        upd->add(std::move(V(4).node));
        s->add(std::move(upd));
        s->add(std::move(V(6).node));
        result.node = std::move(s);
        break;
    }
    case 228: // FOR LPAREN declaration expression_stmt RPAREN statement (C99 for-loop with decl)
    {
        auto s = ASTNode::make(NodeKind::ForStmt, V(0).line, V(0).col);
        s->add(std::move(V(2).node)); // declaration (as init)
        s->add(std::move(V(3).node)); // condition
        s->add(nullptr);              // no increment
        s->add(std::move(V(5).node)); // body
        result.node = std::move(s);
        break;
    }
    case 229: // FOR LPAREN declaration expression_stmt expression RPAREN statement (C99 for-loop with decl + incr)
    {
        auto s = ASTNode::make(NodeKind::ForStmt, V(0).line, V(0).col);
        s->add(std::move(V(2).node)); // declaration (as init)
        s->add(std::move(V(3).node)); // condition
        s->add(std::move(V(4).node)); // increment
        s->add(std::move(V(6).node)); // body
        result.node = std::move(s);
        break;
    }

    // jump_stmt (old 219-223 -> new 227-231)
    case 230: // GOTO IDENTIFIER SEMICOLON
    {
        auto s = ASTNode::make(NodeKind::GotoStmt, V(0).line, V(0).col);
        s->sval = V(1).text;
        result.node = std::move(s);
        break;
    }
    case 231: // CONTINUE SEMICOLON
        result.node = ASTNode::make(NodeKind::ContinueStmt, V(0).line, V(0).col);
        break;
    case 232: // BREAK SEMICOLON
        result.node = ASTNode::make(NodeKind::BreakStmt, V(0).line, V(0).col);
        break;
    case 233: // RETURN SEMICOLON
        result.node = ASTNode::make(NodeKind::ReturnStmt, V(0).line, V(0).col);
        break;
    case 234: // RETURN expression SEMICOLON
    {
        auto s = ASTNode::make(NodeKind::ReturnStmt, V(0).line, V(0).col);
        s->add(std::move(V(1).node));
        result.node = std::move(s);
        break;
    }

    // translation_unit (old 224-225 -> new 232-233)
    case 235:
    {
        auto tu = ASTNode::make(NodeKind::TranslationUnit, V(0).line, V(0).col);
        tu->add(std::move(V(0).node));
        result.node = std::move(tu);
        break;
    }
    case 236:
        V(0).node->add(std::move(V(1).node));
        result.node = std::move(V(0).node);
        break;

    // external_decl (old 226-227 -> new 234-235)
    case 237: case 238:
        result.node = std::move(V(0).node);
        break;

    // function_def (old 228-229 -> new 236-237)
    case 239: // -> decl_specs declarator declaration_list compound_stmt (K&R)
    {
        auto fn = ASTNode::make(NodeKind::FunctionDef, V(0).line, V(0).col);
        fn->add(std::move(V(0).node));
        fn->add(std::move(V(1).node));
        fn->add(std::move(V(2).node));
        fn->add(std::move(V(3).node));
        result.node = std::move(fn);
        break;
    }
    case 240: // -> decl_specs declarator compound_stmt
    {
        auto fn = ASTNode::make(NodeKind::FunctionDef, V(0).line, V(0).col);
        fn->add(std::move(V(0).node));
        fn->add(std::move(V(1).node));
        fn->add(std::move(V(2).node));
        result.node = std::move(fn);
        break;
    }

    // declaration_list (old 230-231 -> new 238-239)
    case 241:
    {
        auto dl = ASTNode::make(NodeKind::DeclList, V(0).line, V(0).col);
        dl->add(std::move(V(0).node));
        result.node = std::move(dl);
        break;
    }
    case 242:
        V(0).node->add(std::move(V(1).node));
        result.node = std::move(V(0).node);
        break;

    default:
        fprintf(stderr, "Internal error: unhandled production %d\n", prod);
        break;
    }

    #undef V
    return result;
}

ASTPtr Parser::parse() {
    stateStack_.push_back(0);
    valueStack_.push_back(ParseValue{});

    while (true) {
        int state = stateStack_.back();
        int token = curtok_.type;
        int action = lookupAction(state, token);

        if (action > 0) {
            // Shift
            stateStack_.push_back(action);
            ParseValue pv;
            if (curtok_.text) {
                if (curtok_.type == TOK_IDENTIFIER || curtok_.type == TOK_TYPE_NAME) {
                    const char* p = curtok_.text;
                    size_t len = 0;
                    while ((p[len] >= 'a' && p[len] <= 'z') || (p[len] >= 'A' && p[len] <= 'Z') ||
                           (p[len] >= '0' && p[len] <= '9') || p[len] == '_') len++;
                    pv.text = std::string(curtok_.text, len);
                } else if (curtok_.type == TOK_CONST || curtok_.type == TOK_SIZEOF) {
                    // C23: Preserve original text to distinguish const/constexpr and sizeof/typeof
                    const char* p = curtok_.text;
                    size_t len = 0;
                    while ((p[len] >= 'a' && p[len] <= 'z') || (p[len] >= 'A' && p[len] <= 'Z') ||
                           (p[len] >= '0' && p[len] <= '9') || p[len] == '_') len++;
                    pv.text = std::string(curtok_.text, len);
                } else if (curtok_.type == TOK_STRING_LITERAL) {
                    const char* p = curtok_.text;
                    const char* end = p + 1;
                    while (*end && *end != '"') { if (*end == '\\') end++; end++; }
                    if (*end == '"') end++;
                    pv.text = std::string(curtok_.text, end - curtok_.text);
                } else if (curtok_.type == TOK_FLOATING_CONSTANT) {
                    // Extract the numeric text from source
                    const char* p = curtok_.text;
                    size_t len = 0;
                    while (p[len] && (p[len] == '.' || p[len] == 'e' || p[len] == 'E' ||
                           p[len] == '+' || p[len] == '-' ||
                           (p[len] >= '0' && p[len] <= '9'))) len++;
                    if (p[len] == 'f' || p[len] == 'F' || p[len] == 'l' || p[len] == 'L') len++;
                    pv.text = std::string(curtok_.text, len);
                } else {
                    // For keyword tokens (long, int, char, etc.), read actual source text
                    // to preserve case. Grammar table uses uppercase (LONG) but source has
                    // lowercase (long). This distinction matters: lowercase = C keyword,
                    // uppercase = typedef name (e.g., Windows LONG = 4-byte int).
                    const char* p = curtok_.text;
                    if (p && ((p[0] >= 'a' && p[0] <= 'z') || (p[0] >= 'A' && p[0] <= 'Z') || p[0] == '_')) {
                        size_t len = 0;
                        while ((p[len] >= 'a' && p[len] <= 'z') || (p[len] >= 'A' && p[len] <= 'Z') ||
                               (p[len] >= '0' && p[len] <= '9') || p[len] == '_') len++;
                        pv.text = std::string(curtok_.text, len);
                    } else {
                        pv.text = __SynYy_stok0[curtok_.type];
                    }
                }
            }
            pv.ival = curtok_.int_val;
            pv.line = curtok_.line;
            pv.col = curtok_.col;
            valueStack_.push_back(std::move(pv));
            curtok_ = lex_.next();
        }
        else if (action < 0 && action != -9999) {
            // Reduce
            int prod = -action;
            int rhs_len = __SynReduce0[prod];
            ParseValue result = reduce(prod);

            for (int i = 0; i < rhs_len; i++) {
                stateStack_.pop_back();
                valueStack_.pop_back();
            }

            int lhs = __SynLhs[prod];
            int newState = lookupGoto(stateStack_.back(), lhs);
            if (newState < 0) {
                error("internal error: no goto entry");
                if (error_count_ > 20) return nullptr;
                panicRecover(); // 4.1: reset and try to continue
                if (curtok_.type == TOK_EOT || curtok_.type == TOK_SOT) return nullptr;
                continue;
            }
            stateStack_.push_back(newState);
            valueStack_.push_back(std::move(result));
        }
        else if (action == 0) {
            // Accept
            if (valueStack_.size() >= 2) {
                return std::move(valueStack_[1].node);
            }
            return nullptr;
        }
        else {
            // At EOF (token 0): accept whatever AST we have — don't enter error recovery
            if (token == 0) {
                // Find the translation_unit node in the value stack
                for (int vi = (int)valueStack_.size() - 1; vi >= 0; vi--) {
                    if (valueStack_[vi].node &&
                        valueStack_[vi].node->kind == NodeKind::TranslationUnit)
                        return std::move(valueStack_[vi].node);
                }
                if (valueStack_.size() >= 2)
                    return std::move(valueStack_[1].node);
                return nullptr;
            }
            // 4.1: Syntax error — report and try to recover
            error("syntax error");
            if (error_count_ > 20) return nullptr;
            panicRecover();
            if (curtok_.type == TOK_EOT || curtok_.type == TOK_SOT) return nullptr;
        }
    }
}
