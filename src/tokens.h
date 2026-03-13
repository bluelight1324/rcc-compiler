#pragma once
// Token IDs matching LALRGen-generated tables (synout0.cpp __SynYy_stok0[])
// Token 0 = __SOT__ (start of tokens), Token 88 = __EOT__ (end of tokens)

enum Token {
    TOK_SOT             = 0,   // __SOT__ (internal)
    TOK_IDENTIFIER      = 1,
    TOK_INTEGER_CONSTANT= 2,
    TOK_FLOATING_CONSTANT = 3,
    TOK_CHARACTER_CONSTANT = 4,
    TOK_STRING_LITERAL  = 5,
    TOK_TYPE_NAME       = 6,   // typedef name used as type
    TOK_AUTO            = 7,
    TOK_BREAK           = 8,
    TOK_CASE            = 9,
    TOK_CHAR            = 10,
    TOK_CONST           = 11,
    TOK_CONTINUE        = 12,
    TOK_DEFAULT         = 13,
    TOK_DO              = 14,
    TOK_DOUBLE          = 15,
    TOK_ELSE            = 16,
    TOK_ENUM            = 17,
    TOK_EXTERN          = 18,
    TOK_FLOAT           = 19,
    TOK_FOR             = 20,
    TOK_GOTO            = 21,
    TOK_IF              = 22,
    TOK_INT             = 23,
    TOK_LONG            = 24,
    TOK_REGISTER        = 25,
    TOK_RETURN          = 26,
    TOK_SHORT           = 27,
    TOK_SIGNED          = 28,
    TOK_SIZEOF          = 29,
    TOK_STATIC          = 30,
    TOK_STRUCT          = 31,
    TOK_SWITCH          = 32,
    TOK_TYPEDEF         = 33,
    TOK_UNION           = 34,
    TOK_UNSIGNED        = 35,
    TOK_VOID            = 36,
    TOK_VOLATILE        = 37,
    TOK_WHILE           = 38,
    TOK_ELLIPSIS        = 39,  // ...
    TOK_RIGHT_ASSIGN    = 40,  // >>=
    TOK_LEFT_ASSIGN     = 41,  // <<=
    TOK_ADD_ASSIGN      = 42,  // +=
    TOK_SUB_ASSIGN      = 43,  // -=
    TOK_MUL_ASSIGN      = 44,  // *=
    TOK_DIV_ASSIGN      = 45,  // /=
    TOK_MOD_ASSIGN      = 46,  // %=
    TOK_AND_ASSIGN      = 47,  // &=
    TOK_XOR_ASSIGN      = 48,  // ^=
    TOK_OR_ASSIGN       = 49,  // |=
    TOK_RIGHT_OP        = 50,  // >>
    TOK_LEFT_OP         = 51,  // <<
    TOK_INC_OP          = 52,  // ++
    TOK_DEC_OP          = 53,  // --
    TOK_PTR_OP          = 54,  // ->
    TOK_AND_OP          = 55,  // &&
    TOK_OR_OP           = 56,  // ||
    TOK_LE_OP           = 57,  // <=
    TOK_GE_OP           = 58,  // >=
    TOK_EQ_OP           = 59,  // ==
    TOK_NE_OP           = 60,  // !=
    TOK_SEMICOLON       = 61,  // ;
    TOK_LBRACE          = 62,  // {
    TOK_RBRACE          = 63,  // }
    TOK_COMMA           = 64,  // ,
    TOK_COLON           = 65,  // :
    TOK_ASSIGN          = 66,  // =
    TOK_LPAREN          = 67,  // (
    TOK_RPAREN          = 68,  // )
    TOK_LBRACKET        = 69,  // [
    TOK_RBRACKET        = 70,  // ]
    TOK_DOT             = 71,  // .
    TOK_AMPERSAND       = 72,  // &
    TOK_BANG            = 73,  // !
    TOK_TILDE           = 74,  // ~
    TOK_MINUS           = 75,  // -
    TOK_PLUS            = 76,  // +
    TOK_STAR            = 77,  // *
    TOK_SLASH           = 78,  // /
    TOK_PERCENT         = 79,  // %
    TOK_LT              = 80,  // <
    TOK_GT              = 81,  // >
    TOK_CARET           = 82,  // ^
    TOK_PIPE            = 83,  // |
    TOK_QUESTION        = 84,  // ?
    TOK_THREAD_LOCAL    = 85,  // _Thread_local (C11 §6.7.1)
    TOK_ATOMIC          = 86,  // _Atomic (C11 §6.7.2.4 / §6.7.3)
    TOK_ALIGNAS         = 87,  // _Alignas (C11 §6.7.5)
    TOK_EOT             = 88,  // __EOT__ (internal)
    TOK_GENERIC         = 89,  // _Generic (C11/C23 §6.5.1.1)
    TOK_EOF             = 0    // End of file = token 0
};

struct TokenInfo {
    Token type;
    const char* text;
    int line;
    int col;
    union {
        long long int_val;
        char char_val;
    };
};
