#pragma once
#include "tokens.h"
#include <string>
#include <vector>
#include <set>

// ─── C23 [[attribute]] registry ────────────────────────────────────────────
// Populated by the Lexer when it encounters [[...]] blocks; queried by CodeGen.
namespace AttrRegistry {
    struct Entry {
        int         line;   // source line where the [[...]] appeared
        std::string name;   // attribute name, e.g. "nodiscard", "deprecated"
        std::string msg;    // optional string argument, e.g. deprecation message
    };
    void        record(int line, const std::string& name, const std::string& msg = "");
    bool        has(int lineFrom, int lineTo, const std::string& name);
    std::string getMessage(int lineFrom, int lineTo, const std::string& name);
    // Returns true if found; removes the entry so it cannot be matched again
    bool        take(int lineFrom, int lineTo, const std::string& name, std::string& msg_out);
    void        clear();
}

class Lexer {
public:
    Lexer(const char* src, const char* filename);
    TokenInfo next();

    // Typedef name tracking for TYPE_NAME tokens
    void addTypeName(const std::string& name) { type_names_.insert(name); }
    bool isTypeName(const std::string& name) const { return type_names_.count(name) > 0; }
    std::set<std::string>& typeNames() { return type_names_; }
    int line() const { return line_; }
    int col() const { return col_; }
    const char* filename() const { return filename_; }

private:
    const char* src_;
    const char* pos_;
    const char* filename_;
    int line_;
    int col_;

    void skipWhitespaceAndComments();
    char peek() const { return *pos_; }
    char advance();
    TokenInfo makeToken(Token t, const char* start, int startLine, int startCol);
    TokenInfo readNumber(int startLine, int startCol);
    TokenInfo readIdentOrKeyword(int startLine, int startCol);
    TokenInfo readString(int startLine, int startCol);
    TokenInfo readChar(int startLine, int startCol);

    std::set<std::string> type_names_;
    std::vector<std::string> concat_strings_; // storage for concatenated string literals
};
