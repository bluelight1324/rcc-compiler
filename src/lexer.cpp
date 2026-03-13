#include "lexer.h"
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <algorithm>

// ─── AttrRegistry implementation ───────────────────────────────────────────
namespace AttrRegistry {
    static std::vector<Entry> g_entries;

    void record(int line, const std::string& name, const std::string& msg) {
        g_entries.push_back({line, name, msg});
    }

    bool has(int lineFrom, int lineTo, const std::string& name) {
        for (const auto& e : g_entries)
            if (e.line >= lineFrom && e.line <= lineTo && e.name == name)
                return true;
        return false;
    }

    std::string getMessage(int lineFrom, int lineTo, const std::string& name) {
        for (const auto& e : g_entries)
            if (e.line >= lineFrom && e.line <= lineTo && e.name == name)
                return e.msg;
        return "";
    }

    bool take(int lineFrom, int lineTo, const std::string& name, std::string& msg_out) {
        for (auto it = g_entries.begin(); it != g_entries.end(); ++it) {
            if (it->line >= lineFrom && it->line <= lineTo && it->name == name) {
                msg_out = it->msg;
                g_entries.erase(it);
                return true;
            }
        }
        return false;
    }

    void clear() { g_entries.clear(); }
}

static struct { const char* kw; Token tok; } keywords[] = {
    {"auto",     TOK_AUTO},     {"break",    TOK_BREAK},
    {"case",     TOK_CASE},     {"char",     TOK_CHAR},
    {"const",    TOK_CONST},    {"continue", TOK_CONTINUE},
    {"default",  TOK_DEFAULT},  {"do",       TOK_DO},
    {"double",   TOK_DOUBLE},   {"else",     TOK_ELSE},
    {"enum",     TOK_ENUM},     {"extern",   TOK_EXTERN},
    {"float",    TOK_FLOAT},    {"for",      TOK_FOR},
    {"goto",     TOK_GOTO},     {"if",       TOK_IF},
    {"int",      TOK_INT},      {"long",     TOK_LONG},
    {"register", TOK_REGISTER}, {"return",   TOK_RETURN},
    {"short",    TOK_SHORT},    {"signed",   TOK_SIGNED},
    {"sizeof",   TOK_SIZEOF},   {"static",   TOK_STATIC},
    {"struct",   TOK_STRUCT},   {"switch",   TOK_SWITCH},
    {"typedef",  TOK_TYPEDEF},  {"union",    TOK_UNION},
    {"unsigned", TOK_UNSIGNED}, {"void",     TOK_VOID},
    {"volatile", TOK_VOLATILE}, {"while",    TOK_WHILE},
    // C23: typeof uses sizeof's token (both have same syntax)
    {"typeof",         TOK_SIZEOF},
    {"typeof_unqual",  TOK_SIZEOF},   // C23 §6.7.2: typeof without cv-qualifiers (same semantics in RCC)
    // C23: constexpr uses const's token (handled specially in codegen)
    {"constexpr", TOK_CONST},
    // C11 keywords — real grammar tokens (not preprocessor no-ops)
    {"_Thread_local", TOK_THREAD_LOCAL},
    {"thread_local",  TOK_THREAD_LOCAL},   // C++ / GCC alias
    {"_Atomic",       TOK_ATOMIC},
    {"_Alignas",      TOK_ALIGNAS},
    {"alignas",       TOK_ALIGNAS},        // C23 keyword / C11 macro alias
    {"_Generic",      TOK_GENERIC},        // C11/C23 §6.5.1.1 type-generic expression
    // C99 inline — function specifier; no codegen effect in RCC (maps to static token)
    {"inline",      TOK_STATIC},
    {"__inline",    TOK_STATIC},           // MSVC extension
    {"__inline__",  TOK_STATIC},           // GCC extension
    // C99 restrict — pointer-aliasing qualifier; no-op in RCC (maps to const token)
    {"restrict",    TOK_CONST},
    {"__restrict",  TOK_CONST},            // GCC/Clang extension
    {"__restrict__",TOK_CONST},            // GCC/Clang extension
    {nullptr,    TOK_SOT}
};

Lexer::Lexer(const char* src, const char* filename)
    : src_(src), pos_(src), filename_(filename), line_(1), col_(1) {}

char Lexer::advance() {
    char c = *pos_++;
    if (c == '\n') { line_++; col_ = 1; }
    else { col_++; }
    return c;
}

void Lexer::skipWhitespaceAndComments() {
    for (;;) {
        while (*pos_ == ' ' || *pos_ == '\t' || *pos_ == '\r' || *pos_ == '\n')
            advance();
        if (pos_[0] == '/' && pos_[1] == '/') {
            while (*pos_ && *pos_ != '\n') advance();
            continue;
        }
        if (pos_[0] == '/' && pos_[1] == '*') {
            advance(); advance();
            while (*pos_ && !(pos_[0] == '*' && pos_[1] == '/')) advance();
            if (*pos_) { advance(); advance(); }
            continue;
        }
        // Skip preprocessor directives (#include, #define, #ifdef, etc.)
        if (*pos_ == '#') {
            while (*pos_ && *pos_ != '\n') advance();
            continue;
        }
        break;
    }
}

TokenInfo Lexer::makeToken(Token t, const char* start, int startLine, int startCol) {
    TokenInfo ti;
    ti.type = t;
    ti.line = startLine;
    ti.col = startCol;
    ti.int_val = 0;
    // We store the text pointer as the start of the token in source
    ti.text = start;
    return ti;
}

TokenInfo Lexer::readNumber(int sl, int sc) {
    const char* start = pos_;
    long long val = 0;
    // C23: Binary literals 0b or 0B
    if (pos_[0] == '0' && (pos_[1] == 'b' || pos_[1] == 'B')) {
        advance(); advance();
        while (*pos_ == '0' || *pos_ == '1' || *pos_ == '\'') {
            if (*pos_ == '\'') { advance(); continue; } // C23: digit separator
            char c = advance();
            val = val * 2 + (c - '0');
        }
    } else if (pos_[0] == '0' && (pos_[1] == 'x' || pos_[1] == 'X')) {
        advance(); advance();
        while ((*pos_ >= '0' && *pos_ <= '9') || (*pos_ >= 'a' && *pos_ <= 'f') || (*pos_ >= 'A' && *pos_ <= 'F') || *pos_ == '\'') {
            if (*pos_ == '\'') { advance(); continue; } // C23: digit separator
            char c = advance();
            val = val * 16 + (c <= '9' ? c - '0' : (c | 32) - 'a' + 10);
        }
        // W18: C99 hex float literal: 0x[hex][.[hex]]p[±][dec]
        // e.g. 0x1.fp3 = 1.9375 * 2^3 = 15.5
        if (*pos_ == '.' || *pos_ == 'p' || *pos_ == 'P') {
            if (*pos_ == '.') {
                advance();
                while ((*pos_ >= '0' && *pos_ <= '9') || (*pos_ >= 'a' && *pos_ <= 'f') || (*pos_ >= 'A' && *pos_ <= 'F') || *pos_ == '\'') {
                    if (*pos_ == '\'') { advance(); continue; }
                    advance();
                }
            }
            if (*pos_ == 'p' || *pos_ == 'P') {
                advance();
                if (*pos_ == '+' || *pos_ == '-') advance();
                while (*pos_ >= '0' && *pos_ <= '9') advance();
            }
            if (*pos_ == 'f' || *pos_ == 'F' || *pos_ == 'l' || *pos_ == 'L') advance();
            TokenInfo ti = makeToken(TOK_FLOATING_CONSTANT, start, sl, sc);
            ti.int_val = 0;
            return ti;
        }
    } else if (pos_[0] == '0' && pos_[1] >= '0' && pos_[1] <= '7') {
        // Octal literal
        advance(); // skip leading 0
        while ((*pos_ >= '0' && *pos_ <= '7') || *pos_ == '\'') {
            if (*pos_ == '\'') { advance(); continue; } // C23: digit separator
            val = val * 8 + (advance() - '0');
        }
    } else {
        while ((*pos_ >= '0' && *pos_ <= '9') || *pos_ == '\'') {
            if (*pos_ == '\'') { advance(); continue; } // C23: digit separator
            val = val * 10 + (advance() - '0');
        }
    }
    // Check for floating point: decimal point or exponent
    if (*pos_ == '.' || *pos_ == 'e' || *pos_ == 'E') {
        if (*pos_ == '.') {
            advance();
            while ((*pos_ >= '0' && *pos_ <= '9') || *pos_ == '\'') {
                if (*pos_ == '\'') { advance(); continue; } // C23: digit separator
                advance();
            }
        }
        if (*pos_ == 'e' || *pos_ == 'E') {
            advance();
            if (*pos_ == '+' || *pos_ == '-') advance();
            while ((*pos_ >= '0' && *pos_ <= '9') || *pos_ == '\'') {
                if (*pos_ == '\'') { advance(); continue; } // C23: digit separator
                advance();
            }
        }
        // skip suffix f, F, l, L
        if (*pos_ == 'f' || *pos_ == 'F' || *pos_ == 'l' || *pos_ == 'L') advance();
        TokenInfo ti = makeToken(TOK_FLOATING_CONSTANT, start, sl, sc);
        ti.int_val = 0;
        return ti;
    }
    // skip suffix like L, U, LL, etc
    while (*pos_ == 'l' || *pos_ == 'L' || *pos_ == 'u' || *pos_ == 'U') advance();
    TokenInfo ti = makeToken(TOK_INTEGER_CONSTANT, start, sl, sc);
    ti.int_val = val;
    return ti;
}

TokenInfo Lexer::readIdentOrKeyword(int sl, int sc) {
    const char* start = pos_;
    while ((*pos_ >= 'a' && *pos_ <= 'z') || (*pos_ >= 'A' && *pos_ <= 'Z') ||
           (*pos_ >= '0' && *pos_ <= '9') || *pos_ == '_')
        advance();
    size_t len = pos_ - start;
    for (int i = 0; keywords[i].kw; i++) {
        if (strlen(keywords[i].kw) == len && memcmp(keywords[i].kw, start, len) == 0)
            return makeToken(keywords[i].tok, start, sl, sc);
    }
    // C++ minimal subset: bool -> int, true -> 1, false -> 0, nullptr -> 0
    if (len == 4 && memcmp(start, "bool", 4) == 0)
        return makeToken(TOK_INT, start, sl, sc);
    if (len == 4 && memcmp(start, "true", 4) == 0) {
        TokenInfo ti = makeToken(TOK_INTEGER_CONSTANT, start, sl, sc);
        ti.int_val = 1;
        return ti;
    }
    if (len == 5 && memcmp(start, "false", 5) == 0) {
        TokenInfo ti = makeToken(TOK_INTEGER_CONSTANT, start, sl, sc);
        ti.int_val = 0;
        return ti;
    }
    if (len == 7 && memcmp(start, "nullptr", 7) == 0) {
        TokenInfo ti = makeToken(TOK_INTEGER_CONSTANT, start, sl, sc);
        ti.int_val = 0;
        return ti;
    }
    // W25: C23 char8_t — alias for unsigned char, treat as char for parsing
    if (len == 7 && memcmp(start, "char8_t", 7) == 0)
        return makeToken(TOK_CHAR, start, sl, sc);
    // C99: va_list is a built-in type
    if (len == 7 && memcmp(start, "va_list", 7) == 0) {
        return makeToken(TOK_TYPE_NAME, start, sl, sc);
    }
    if (len == 18 && memcmp(start, "__builtin_va_list", 18) == 0) {
        return makeToken(TOK_TYPE_NAME, start, sl, sc);
    }
    // Check if identifier is a registered typedef name
    std::string ident(start, len);
    if (type_names_.count(ident))
        return makeToken(TOK_TYPE_NAME, start, sl, sc);
    return makeToken(TOK_IDENTIFIER, start, sl, sc);
}

TokenInfo Lexer::readString(int sl, int sc) {
    const char* start = pos_;
    advance(); // skip opening "
    while (*pos_ && *pos_ != '"') {
        if (*pos_ == '\\') advance();
        advance();
    }
    if (*pos_ == '"') advance();
    const char* end1 = pos_;

    // Check for adjacent string literal concatenation ("abc" "def")
    const char* save_pos = pos_;
    int save_line = line_, save_col = col_;
    skipWhitespaceAndComments();
    if (*pos_ == '"') {
        // Concatenate: build combined string in storage
        std::string combined(start, end1 - start);
        combined.pop_back(); // remove closing "
        while (*pos_ == '"') {
            advance(); // skip opening "
            while (*pos_ && *pos_ != '"') {
                combined += *pos_;
                if (*pos_ == '\\') { advance(); combined += *pos_; }
                advance();
            }
            if (*pos_ == '"') advance();
            const char* save2 = pos_;
            int save2_line = line_, save2_col = col_;
            skipWhitespaceAndComments();
            if (*pos_ != '"') {
                pos_ = save2; line_ = save2_line; col_ = save2_col;
                break;
            }
        }
        combined += '"';
        concat_strings_.push_back(std::move(combined));
        TokenInfo ti;
        ti.type = TOK_STRING_LITERAL;
        ti.line = sl;
        ti.col = sc;
        ti.int_val = 0;
        ti.text = concat_strings_.back().c_str();
        return ti;
    } else {
        pos_ = save_pos; line_ = save_line; col_ = save_col;
        return makeToken(TOK_STRING_LITERAL, start, sl, sc);
    }
}

TokenInfo Lexer::readChar(int sl, int sc) {
    const char* start = pos_;
    advance(); // skip '
    char val = 0;
    if (*pos_ == '\\') {
        advance();
        switch (*pos_) {
            case 'n': val = '\n'; break;
            case 't': val = '\t'; break;
            case '\\': val = '\\'; break;
            case '\'': val = '\''; break;
            case '"': val = '"'; break;
            case '0': val = '\0'; break;
            case 'a': val = '\a'; break;
            case 'b': val = '\b'; break;
            case 'f': val = '\f'; break;
            case 'r': val = '\r'; break;
            case 'v': val = '\v'; break;
            case 'x': { // hex escape: \xHH (fixed) or C23 \x{HHH} (delimited)
                advance(); // skip 'x'
                int hval = 0;
                if (*pos_ == '{') { // C23 §6.4.4.4: delimited \x{HHH}
                    advance(); // skip '{'
                    while (*pos_ && *pos_ != '}') {
                        char c2 = *pos_;
                        hval = hval * 16 + (c2 <= '9' ? c2 - '0' : (c2 | 32) - 'a' + 10);
                        advance();
                    }
                    if (*pos_ == '}') advance();
                } else { // fixed-width \xHH
                    for (int i = 0; i < 2 && ((*pos_ >= '0' && *pos_ <= '9') ||
                        (*pos_ >= 'a' && *pos_ <= 'f') || (*pos_ >= 'A' && *pos_ <= 'F')); i++) {
                        char c2 = *pos_;
                        hval = hval * 16 + (c2 <= '9' ? c2 - '0' : (c2 | 32) - 'a' + 10);
                        advance();
                    }
                }
                val = (char)hval;
                goto skip_advance;
            }
            case 'o': { // C23 §6.4.4.4: delimited octal \o{OOO}
                advance(); // skip 'o'
                int oval2 = 0;
                if (*pos_ == '{') {
                    advance(); // skip '{'
                    while (*pos_ && *pos_ != '}' && *pos_ >= '0' && *pos_ <= '7') {
                        oval2 = oval2 * 8 + (*pos_ - '0');
                        advance();
                    }
                    if (*pos_ == '}') advance();
                }
                val = (char)oval2;
                goto skip_advance;
            }
            case 'u': // C23 §6.4.4.4: \u{HHHH} delimited or \uHHHH fixed-width
            case 'U': { // C23 §6.4.4.4: \U{HHHHHHHH} delimited or \UHHHHHHHH fixed-width
                advance(); // skip 'u'/'U'
                int uval = 0;
                if (*pos_ == '{') { // C23 delimited form
                    advance(); // skip '{'
                    while (*pos_ && *pos_ != '}') {
                        char c2 = *pos_;
                        uval = uval * 16 + (c2 <= '9' ? c2 - '0' : (c2 | 32) - 'a' + 10);
                        advance();
                    }
                    if (*pos_ == '}') advance();
                } else { // fixed-width: read up to 8 hex digits
                    for (int i = 0; i < 8 && ((*pos_ >= '0' && *pos_ <= '9') ||
                        (*pos_ >= 'a' && *pos_ <= 'f') || (*pos_ >= 'A' && *pos_ <= 'F')); i++) {
                        char c2 = *pos_;
                        uval = uval * 16 + (c2 <= '9' ? c2 - '0' : (c2 | 32) - 'a' + 10);
                        advance();
                    }
                }
                val = (char)(uval & 0xFF); // truncate to byte for char context
                goto skip_advance;
            }
            default:
                if (*pos_ >= '0' && *pos_ <= '7') { // octal escape \OOO
                    int oval = *pos_ - '0';
                    advance();
                    for (int i = 0; i < 2 && *pos_ >= '0' && *pos_ <= '7'; i++) {
                        oval = oval * 8 + (*pos_ - '0');
                        advance();
                    }
                    val = (char)oval;
                    goto skip_advance;
                }
                val = *pos_; break;
        }
        advance();
        skip_advance:;
    } else {
        val = advance();
    }
    if (*pos_ == '\'') advance();
    TokenInfo ti = makeToken(TOK_CHARACTER_CONSTANT, start, sl, sc);
    ti.char_val = val;
    return ti;
}

TokenInfo Lexer::next() {
    skipWhitespaceAndComments();
    int sl = line_, sc = col_;
    const char* start = pos_;

    if (!*pos_) return makeToken(TOK_EOF, start, sl, sc);

    char c = *pos_;

    // Numbers
    if (c >= '0' && c <= '9') return readNumber(sl, sc);

    // W25: u8"..." UTF-8 string literal prefix (C23) — must check before ident dispatch
    if (c == 'u' && pos_[1] == '8' && pos_[2] == '"') {
        advance(); advance(); // skip 'u' and '8'; pos_ now points to opening '"'
        return readString(sl, sc);
    }
    // C23 §6.4.4.4: u8'a' — UTF-8 character constant (type char8_t = unsigned char)
    if (c == 'u' && pos_[1] == '8' && pos_[2] == '\'') {
        advance(); advance(); // skip 'u' and '8'; pos_ now points to opening '\''
        return readChar(sl, sc); // char value fits in unsigned char (char8_t)
    }
    // C11: u"..." UTF-16LE string literal (char16_t[]) — check BEFORE ident dispatch
    // NOTE: pos_[0] == c (current char), pos_[1] is the NEXT char after 'u'
    if (c == 'u' && pos_[1] == '"') {
        advance(); // skip 'u'; pos_ now points to opening '"'
        TokenInfo ti = readString(sl, sc);
        ti.int_val = 2; // 2 bytes per char unit (UTF-16)
        return ti;
    }
    // C11: U"..." UTF-32LE string literal (char32_t[])
    if (c == 'U' && pos_[1] == '"') {
        advance(); // skip 'U'; pos_ now points to opening '"'
        TokenInfo ti = readString(sl, sc);
        ti.int_val = 4; // 4 bytes per char unit (UTF-32)
        return ti;
    }
    // C89/C99: L"..." wide string literal (wchar_t[] = 2 bytes on Windows x64)
    if (c == 'L' && pos_[1] == '"') {
        advance(); // skip 'L'; pos_ now points to opening '"'
        TokenInfo ti = readString(sl, sc);
        ti.int_val = 2; // wchar_t = 2 bytes (UTF-16LE) on Windows
        return ti;
    }

    // Identifiers/keywords
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_')
        return readIdentOrKeyword(sl, sc);

    // Strings
    if (c == '"') return readString(sl, sc);
    if (c == '\'') return readChar(sl, sc);

    // Multi-char operators
    advance();
    switch (c) {
    case '+':
        if (*pos_ == '+') { advance(); return makeToken(TOK_INC_OP, start, sl, sc); }
        if (*pos_ == '=') { advance(); return makeToken(TOK_ADD_ASSIGN, start, sl, sc); }
        return makeToken(TOK_PLUS, start, sl, sc);
    case '-':
        if (*pos_ == '-') { advance(); return makeToken(TOK_DEC_OP, start, sl, sc); }
        if (*pos_ == '=') { advance(); return makeToken(TOK_SUB_ASSIGN, start, sl, sc); }
        if (*pos_ == '>') { advance(); return makeToken(TOK_PTR_OP, start, sl, sc); }
        return makeToken(TOK_MINUS, start, sl, sc);
    case '*':
        if (*pos_ == '=') { advance(); return makeToken(TOK_MUL_ASSIGN, start, sl, sc); }
        return makeToken(TOK_STAR, start, sl, sc);
    case '/':
        if (*pos_ == '=') { advance(); return makeToken(TOK_DIV_ASSIGN, start, sl, sc); }
        return makeToken(TOK_SLASH, start, sl, sc);
    case '%':
        if (*pos_ == '=') { advance(); return makeToken(TOK_MOD_ASSIGN, start, sl, sc); }
        return makeToken(TOK_PERCENT, start, sl, sc);
    case '=':
        if (*pos_ == '=') { advance(); return makeToken(TOK_EQ_OP, start, sl, sc); }
        return makeToken(TOK_ASSIGN, start, sl, sc);
    case '!':
        if (*pos_ == '=') { advance(); return makeToken(TOK_NE_OP, start, sl, sc); }
        return makeToken(TOK_BANG, start, sl, sc);
    case '<':
        if (*pos_ == '<') { advance(); if (*pos_ == '=') { advance(); return makeToken(TOK_LEFT_ASSIGN, start, sl, sc); } return makeToken(TOK_LEFT_OP, start, sl, sc); }
        if (*pos_ == '=') { advance(); return makeToken(TOK_LE_OP, start, sl, sc); }
        return makeToken(TOK_LT, start, sl, sc);
    case '>':
        if (*pos_ == '>') { advance(); if (*pos_ == '=') { advance(); return makeToken(TOK_RIGHT_ASSIGN, start, sl, sc); } return makeToken(TOK_RIGHT_OP, start, sl, sc); }
        if (*pos_ == '=') { advance(); return makeToken(TOK_GE_OP, start, sl, sc); }
        return makeToken(TOK_GT, start, sl, sc);
    case '&':
        if (*pos_ == '&') { advance(); return makeToken(TOK_AND_OP, start, sl, sc); }
        if (*pos_ == '=') { advance(); return makeToken(TOK_AND_ASSIGN, start, sl, sc); }
        return makeToken(TOK_AMPERSAND, start, sl, sc);
    case '|':
        if (*pos_ == '|') { advance(); return makeToken(TOK_OR_OP, start, sl, sc); }
        if (*pos_ == '=') { advance(); return makeToken(TOK_OR_ASSIGN, start, sl, sc); }
        return makeToken(TOK_PIPE, start, sl, sc);
    case '^':
        if (*pos_ == '=') { advance(); return makeToken(TOK_XOR_ASSIGN, start, sl, sc); }
        return makeToken(TOK_CARET, start, sl, sc);
    case '.':
        if (pos_[0] == '.' && pos_[1] == '.') { advance(); advance(); return makeToken(TOK_ELLIPSIS, start, sl, sc); }
        return makeToken(TOK_DOT, start, sl, sc);
    case ';': return makeToken(TOK_SEMICOLON, start, sl, sc);
    case ',': return makeToken(TOK_COMMA, start, sl, sc);
    case '{': return makeToken(TOK_LBRACE, start, sl, sc);
    case '}': return makeToken(TOK_RBRACE, start, sl, sc);
    case '(': return makeToken(TOK_LPAREN, start, sl, sc);
    case ')': return makeToken(TOK_RPAREN, start, sl, sc);
    case '[':
        // C23 [[attribute]] syntax — capture attribute name then skip.
        if (*pos_ == '[') {
            advance(); // consume second '['
            // Collect text inside [[...]] for attribute name extraction
            std::string attr_text;
            int depth = 2;
            while (*pos_ && depth > 0) {
                if (*pos_ == '[') { depth++; attr_text += *pos_; }
                else if (*pos_ == ']') { depth--; if (depth > 0) attr_text += *pos_; }
                else attr_text += *pos_;
                advance();
            }
            // Extract attribute name (first identifier in attr_text)
            size_t i = 0;
            while (i < attr_text.size() && (attr_text[i] == ' ' || attr_text[i] == '\t')) i++;
            // Skip optional namespace prefix (e.g., "gnu::" "clang::")
            size_t colon = attr_text.find("::", i);
            size_t nameStart = (colon != std::string::npos) ? colon + 2 : i;
            size_t nameEnd = nameStart;
            while (nameEnd < attr_text.size() &&
                   (isalnum((unsigned char)attr_text[nameEnd]) || attr_text[nameEnd] == '_'))
                nameEnd++;
            if (nameEnd > nameStart) {
                std::string attr_name = attr_text.substr(nameStart, nameEnd - nameStart);
                // Extract optional string argument: deprecated("msg")
                std::string attr_msg;
                size_t q = attr_text.find('"', nameEnd);
                if (q != std::string::npos) {
                    size_t q2 = attr_text.find('"', q + 1);
                    if (q2 != std::string::npos)
                        attr_msg = attr_text.substr(q + 1, q2 - q - 1);
                }
                AttrRegistry::record(sl, attr_name, attr_msg);
            }
            return next();
        }
        return makeToken(TOK_LBRACKET, start, sl, sc);
    case ']': return makeToken(TOK_RBRACKET, start, sl, sc);
    case '~': return makeToken(TOK_TILDE, start, sl, sc);
    case '?': return makeToken(TOK_QUESTION, start, sl, sc);
    case ':': return makeToken(TOK_COLON, start, sl, sc);
    default:
        fprintf(stderr, "%s:%d:%d: error: unexpected character '%c'\n", filename_, sl, sc, c);
        return next();
    }
}
