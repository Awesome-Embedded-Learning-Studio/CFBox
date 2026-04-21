#include "awk.hpp"
#include <cctype>
#include <cstdlib>

namespace cfbox::awk {

class Lexer {
    std::string src_;
    std::size_t pos_ = 0;
public:
    explicit Lexer(std::string src) : src_(std::move(src)) {}

    auto tokenize() -> std::vector<Token> {
        std::vector<Token> tokens;
        while (pos_ < src_.size()) {
            auto c = src_[pos_];
            if (c == ' ' || c == '\t' || c == '\r') { ++pos_; continue; }
            if (c == '#') { while (pos_ < src_.size() && src_[pos_] != '\n') ++pos_; continue; }
            if (c == '\n') { tokens.push_back({TokType::Newline, "\n"}); ++pos_; continue; }
            if (c == '"') { tokens.push_back(read_string()); continue; }
            if (c == '/' && (tokens.empty() || is_regex_context(tokens))) {
                tokens.push_back(read_regex()); continue;
            }
            if (std::isdigit(static_cast<unsigned char>(c)) || (c == '.' && pos_ + 1 < src_.size() && std::isdigit(static_cast<unsigned char>(src_[pos_ + 1])))) {
                tokens.push_back(read_number()); continue;
            }
            if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
                tokens.push_back(read_ident()); continue;
            }
            tokens.push_back(read_operator());
        }
        tokens.push_back({TokType::Eof, ""});
        return tokens;
    }

private:
    auto is_regex_context(const std::vector<Token>& toks) const -> bool {
        if (toks.empty()) return true;
        auto& last = toks.back();
        return last.type == TokType::Newline || last.type == TokType::Semicolon ||
               last.type == TokType::Comma || last.type == TokType::LParen ||
               last.type == TokType::LBrace || last.type == TokType::Match ||
               last.type == TokType::NotMatch || last.type == TokType::And ||
               last.type == TokType::Or;
    }

    auto read_string() -> Token {
        ++pos_;
        std::string s;
        while (pos_ < src_.size() && src_[pos_] != '"') {
            if (src_[pos_] == '\\' && pos_ + 1 < src_.size()) {
                ++pos_;
                switch (src_[pos_]) {
                    case 'n': s += '\n'; break;
                    case 't': s += '\t'; break;
                    case '\\': s += '\\'; break;
                    case '"': s += '"'; break;
                    default: s += src_[pos_]; break;
                }
            } else {
                s += src_[pos_];
            }
            ++pos_;
        }
        if (pos_ < src_.size()) ++pos_;
        return {TokType::String, s};
    }

    auto read_regex() -> Token {
        ++pos_;
        std::string s;
        while (pos_ < src_.size() && src_[pos_] != '/') {
            if (src_[pos_] == '\\' && pos_ + 1 < src_.size()) {
                s += src_[pos_]; ++pos_;
                s += src_[pos_];
            } else {
                s += src_[pos_];
            }
            ++pos_;
        }
        if (pos_ < src_.size()) ++pos_;
        return {TokType::Regex, s};
    }

    auto read_number() -> Token {
        std::string s;
        while (pos_ < src_.size() && (std::isdigit(static_cast<unsigned char>(src_[pos_])) || src_[pos_] == '.')) {
            s += src_[pos_++];
        }
        return {TokType::Number, s};
    }

    auto read_ident() -> Token {
        std::string s;
        while (pos_ < src_.size() && (std::isalnum(static_cast<unsigned char>(src_[pos_])) || src_[pos_] == '_')) {
            s += src_[pos_++];
        }
        static const std::map<std::string, TokType> keywords = {
            {"BEGIN", TokType::Begin}, {"END", TokType::End},
            {"print", TokType::Print}, {"printf", TokType::Printf},
            {"if", TokType::If}, {"else", TokType::Else},
            {"while", TokType::While}, {"for", TokType::For}, {"in", TokType::In},
            {"function", TokType::Function}, {"return", TokType::Return},
            {"delete", TokType::Delete}, {"exit", TokType::Exit}, {"next", TokType::NextStmt},
        };
        auto it = keywords.find(s);
        return {it != keywords.end() ? it->second : TokType::Ident, s};
    }

    auto read_operator() -> Token {
        auto c = src_[pos_++];
        auto peek = (pos_ < src_.size()) ? src_[pos_] : '\0';
        switch (c) {
            case '+': if (peek == '=') { ++pos_; return {TokType::AddAssign, "+="}; } if (peek == '+') { ++pos_; return {TokType::Inc, "++"}; } return {TokType::Add, "+"};
            case '-': if (peek == '=') { ++pos_; return {TokType::SubAssign, "-="}; } if (peek == '-') { ++pos_; return {TokType::Dec, "--"}; } return {TokType::Sub, "-"};
            case '*': if (peek == '=') { ++pos_; return {TokType::MulAssign, "*="}; } if (peek == '*') { ++pos_; return {TokType::Pow, "**"}; } return {TokType::Mul, "*"};
            case '/': if (peek == '=') { ++pos_; return {TokType::DivAssign, "/="}; } return {TokType::Div, "/"};
            case '%': if (peek == '=') { ++pos_; return {TokType::ModAssign, "%="}; } return {TokType::Mod, "%"};
            case '=': return {TokType::Assign, "="};
            case '!': if (peek == '=') { ++pos_; return {TokType::Ne, "!="}; } if (peek == '~') { ++pos_; return {TokType::NotMatch, "!~"}; } return {TokType::Not, "!"};
            case '<': if (peek == '=') { ++pos_; return {TokType::Le, "<="}; } return {TokType::Lt, "<"};
            case '>': if (peek == '=') { ++pos_; return {TokType::Ge, ">="}; } return {TokType::Gt, ">"};
            case '~': return {TokType::Match, "~"};
            case '&': if (peek == '&') { ++pos_; return {TokType::And, "&&"}; } return {TokType::Concat, "&"};
            case '|': if (peek == '|') { ++pos_; return {TokType::Or, "||"}; } return {TokType::Pipe, "|"};
            case '(': return {TokType::LParen, "("};
            case ')': return {TokType::RParen, ")"};
            case '{': return {TokType::LBrace, "{"};
            case '}': return {TokType::RBrace, "}"};
            case '[': return {TokType::LBracket, "["};
            case ']': return {TokType::RBracket, "]"};
            case ';': return {TokType::Semicolon, ";"};
            case ',': return {TokType::Comma, ","};
            case '$': return {TokType::Dollar, "$"};
            case '?': return {TokType::Question, "?"};
            case ':': return {TokType::Colon, ":"};
            default: return {TokType::Eof, std::string(1, c)};
        }
    }
};

auto awk_tokenize(const std::string& src) -> std::vector<Token> {
    Lexer lex(src);
    return lex.tokenize();
}

} // namespace cfbox::awk
