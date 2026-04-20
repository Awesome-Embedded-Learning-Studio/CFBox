#include "sh.hpp"

#include <cctype>

namespace cfbox::sh {

Lexer::Lexer(std::string_view input) : input_{input} {}

auto Lexer::advance() -> char {
    return pos_ < input_.size() ? input_[pos_++] : '\0';
}

auto Lexer::peek() const -> char {
    return pos_ < input_.size() ? input_[pos_] : '\0';
}

auto Lexer::at_end() const -> bool {
    return pos_ >= input_.size();
}

auto Lexer::is_keyword(std::string_view word) const -> bool {
    static const char* kws[] = {
        "if", "then", "elif", "else", "fi",
        "while", "until", "do", "done",
        "for", "in", "case", "esac"
    };
    for (auto kw : kws) {
        if (word == kw) return true;
    }
    return false;
}

auto Lexer::skip_spaces_and_comments() -> void {
    while (pos_ < input_.size()) {
        char c = input_[pos_];
        if (c == ' ' || c == '\t') {
            ++pos_;
        } else if (c == '#') {
            while (pos_ < input_.size() && input_[pos_] != '\n') ++pos_;
        } else {
            break;
        }
    }
}

auto Lexer::read_single_quoted() -> std::string {
    // Opening ' already consumed
    std::string result;
    while (pos_ < input_.size() && input_[pos_] != '\'') {
        result += input_[pos_++];
    }
    if (pos_ < input_.size()) ++pos_; // consume closing '
    return result;
}

auto Lexer::read_double_quoted() -> std::string {
    // Opening " already consumed
    std::string result;
    while (pos_ < input_.size() && input_[pos_] != '"') {
        if (input_[pos_] == '\\' && pos_ + 1 < input_.size()) {
            char next = input_[pos_ + 1];
            if (next == '$' || next == '"' || next == '\\' || next == '`' || next == '\n') {
                ++pos_; // skip backslash
                if (input_[pos_] == '\n') { ++pos_; continue; }
                result += input_[pos_++];
            } else {
                result += input_[pos_++];
            }
        } else {
            result += input_[pos_++];
        }
    }
    if (pos_ < input_.size()) ++pos_; // consume closing "
    return result;
}

auto Lexer::read_operator() -> std::optional<Token> {
    char c = peek();
    Token tok;

    switch (c) {
    case '|':
        if (pos_ + 1 < input_.size() && input_[pos_ + 1] == '&') {
            // This shouldn't happen in valid shell, treat as |
            tok.type = TokType::Pipe; pos_ += 1;
        } else if (pos_ + 1 < input_.size() && input_[pos_ + 1] == '|') {
            tok.type = TokType::Or; pos_ += 2;
        } else {
            tok.type = TokType::Pipe; pos_ += 1;
        }
        return tok;
    case '&':
        if (pos_ + 1 < input_.size() && input_[pos_ + 1] == '&') {
            tok.type = TokType::And; pos_ += 2;
        } else {
            // Background & — treat as Semi for now
            tok.type = TokType::Semi; pos_ += 1;
        }
        return tok;
    case ';': tok.type = TokType::Semi; pos_ += 1; return tok;
    case '(': tok.type = TokType::LParen; pos_ += 1; return tok;
    case ')': tok.type = TokType::RParen; pos_ += 1; return tok;
    case '{': tok.type = TokType::LBrace; pos_ += 1; return tok;
    case '}': tok.type = TokType::RBrace; pos_ += 1; return tok;
    case '<': {
        if (pos_ + 1 < input_.size() && input_[pos_ + 1] == '&') {
            tok.type = TokType::LessAnd; pos_ += 2;
        } else if (pos_ + 1 < input_.size() && input_[pos_ + 1] == '<') {
            // Here-document << — treat the delimiter as a word token after
            tok.type = TokType::Less; pos_ += 1; // simplified: no here-doc support yet
        } else {
            tok.type = TokType::Less; pos_ += 1;
        }
        return tok;
    }
    case '>': {
        if (pos_ + 1 < input_.size() && input_[pos_ + 1] == '&') {
            tok.type = TokType::GreatAnd; pos_ += 2;
        } else if (pos_ + 1 < input_.size() && input_[pos_ + 1] == '>') {
            tok.type = TokType::DGreate; pos_ += 2;
        } else {
            tok.type = TokType::Great; pos_ += 1;
        }
        return tok;
    }
    default:
        return std::nullopt;
    }
}

auto Lexer::read_word() -> Token {
    std::string word;

    while (pos_ < input_.size()) {
        char c = input_[pos_];

        if (c == '\'') {
            ++pos_;
            word += read_single_quoted();
            continue;
        }
        if (c == '"') {
            // Keep the quotes so expand_word knows this was double-quoted
            ++pos_;
            word += '"';
            word += read_double_quoted();
            word += '"';
            continue;
        }
        if (c == '\\' && pos_ + 1 < input_.size()) {
            ++pos_;
            if (input_[pos_] == '\n') { ++pos_; continue; } // line continuation
            word += input_[pos_++];
            continue;
        }

        // Unquoted $ for variable expansion
        if (c == '$') {
            word += c;
            ++pos_;
            if (pos_ < input_.size()) {
                char next = input_[pos_];
                if (next == '{') {
                    // ${VAR} — read until }
                    word += next; ++pos_;
                    while (pos_ < input_.size() && input_[pos_] != '}') {
                        word += input_[pos_++];
                    }
                    if (pos_ < input_.size()) { word += '}'; ++pos_; }
                } else if (next == '(') {
                    // $(...) command substitution — simplified: read until matching )
                    word += next; ++pos_;
                    int depth = 1;
                    while (pos_ < input_.size() && depth > 0) {
                        if (input_[pos_] == '(') ++depth;
                        else if (input_[pos_] == ')') --depth;
                        word += input_[pos_++];
                    }
                } else if (next == '?' || next == '$' || next == '!' || next == '#' ||
                           next == '@' || next == '*' ||
                           (next >= '0' && next <= '9')) {
                    word += next; ++pos_;
                } else if (std::isalpha(static_cast<unsigned char>(next)) || next == '_') {
                    while (pos_ < input_.size() &&
                           (std::isalnum(static_cast<unsigned char>(input_[pos_])) || input_[pos_] == '_')) {
                        word += input_[pos_++];
                    }
                }
            }
            continue;
        }

        // Delimiters that end a word
        if (c == ' ' || c == '\t' || c == '\n' || c == '|' || c == ';' ||
            c == '&' || c == '(' || c == ')' || c == '<' || c == '>' ||
            c == '{' || c == '}' || c == '#') {
            break;
        }

        word += c;
        ++pos_;
    }

    Token tok;
    tok.type = TokType::Word;
    tok.value = std::move(word);
    tok.keyword = is_keyword(tok.value);
    return tok;
}

auto Lexer::next_token() -> Token {
    if (has_cached_) {
        has_cached_ = false;
        return std::move(cached_);
    }

    skip_spaces_and_comments();

    if (at_end()) {
        return Token{TokType::Eof, ""};
    }

    char c = peek();
    if (c == '\n') {
        ++pos_;
        return Token{TokType::Newline, "\n"};
    }

    // Try operator first
    auto op = read_operator();
    if (op) return std::move(*op);

    // Otherwise it's a word
    return read_word();
}

auto Lexer::peek_token() -> const Token& {
    if (!has_cached_) {
        cached_ = next_token();
        has_cached_ = true;
    }
    return cached_;
}

} // namespace cfbox::sh
