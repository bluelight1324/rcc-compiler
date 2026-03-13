#pragma once
#include "lexer.h"
#include "ast.h"
#include <vector>
#include <string>

// Forward declarations for LALRGen tables (defined in synout0.cpp)
extern "C" {
    extern const int* __SynAction0[];
    extern const int* __SynGoto0[];
    extern int __SynLhs[];
    extern int __SynReduce0[];
    extern const char* __SynYy_stok0[];
}

struct ParseValue {
    ASTPtr node;
    std::string text;
    long long ival;
    int line, col;
};

class Parser {
public:
    Parser(Lexer& lex);
    ASTPtr parse();

private:
    Lexer& lex_;
    TokenInfo curtok_;

    std::vector<int> stateStack_;
    std::vector<ParseValue> valueStack_;

    int error_count_ = 0;   // 4.1: error recovery — count of syntax errors

    int lookupAction(int state, int token);
    int lookupGoto(int state, int nonterminal);
    ParseValue reduce(int prod);
    void error(const char* msg);
    void panicRecover(); // 4.1: skip to `;` or `}`, reset state stacks
};
