#include "sh.hpp"

#include <cstdio>
#include <utility>

namespace cfbox::sh {

Parser::Parser(Lexer& lexer) : lexer_(lexer) {
    current_ = lexer_.next_token();
}

auto Parser::advance() -> const Token& {
    current_ = lexer_.next_token();
    return current_;
}

auto Parser::peek() const -> const Token& {
    return current_;
}

auto Parser::expect(TokType type) -> bool {
    if (current_.type == type) {
        advance();
        return true;
    }
    return false;
}

auto Parser::expect_keyword(std::string_view kw) -> bool {
    if (current_.type == TokType::Word && current_.value == kw) {
        advance();
        return true;
    }
    return false;
}

auto Parser::parse_program() -> std::unique_ptr<AndOr> {
    auto result = parse_compound_list();
    // Consume any trailing newlines
    while (current_.type == TokType::Newline) advance();
    return result;
}

static auto is_terminator(const Token& tok) -> bool {
    if (tok.type == TokType::Eof || tok.type == TokType::RParen || tok.type == TokType::RBrace)
        return true;
    if (tok.type == TokType::Word && tok.keyword) {
        return tok.value == "done" || tok.value == "fi" || tok.value == "esac" ||
               tok.value == "else" || tok.value == "elif" || tok.value == "then" ||
               tok.value == "do";
    }
    return false;
}

auto Parser::parse_compound_list() -> std::unique_ptr<AndOr> {
    // Skip newlines
    while (current_.type == TokType::Newline) advance();

    if (is_terminator(current_)) return nullptr;

    auto result = parse_and_or();
    if (!result) return nullptr;

    while (current_.type == TokType::Semi || current_.type == TokType::Newline) {
        advance();
        while (current_.type == TokType::Newline) advance();
        if (is_terminator(current_)) break;

        auto next = parse_and_or();
        if (next) {
            auto merged = std::make_unique<AndOr>();
            // Merge: append entries from result, then from next
            for (auto& e : result->entries) {
                merged->entries.push_back(std::move(e));
            }
            for (auto& e : next->entries) {
                merged->entries.back().first = AndOr::Op::Semi; // chain with semi
                merged->entries.push_back(std::move(e));
            }
            // Actually simpler: just extend entries
            // The first entry of 'next' chains after the last of 'result'
            result = std::move(merged);
        }
    }

    return result;
}

auto Parser::parse_and_or() -> std::unique_ptr<AndOr> {
    if (is_terminator(current_)) return nullptr;

    auto result = std::make_unique<AndOr>();

    auto pipe = parse_pipeline();
    if (!pipe) return nullptr;

    result->entries.push_back({AndOr::Op::Semi, std::move(pipe)});

    while (current_.type == TokType::And || current_.type == TokType::Or) {
        auto op = current_.type == TokType::And ? AndOr::Op::And : AndOr::Op::Or;
        advance();
        while (current_.type == TokType::Newline) advance();

        auto next = parse_pipeline();
        if (next) {
            result->entries.push_back({op, std::move(next)});
        }
    }

    return result;
}

auto Parser::parse_pipeline() -> std::unique_ptr<Pipeline> {
    auto result = std::make_unique<Pipeline>();

    // Optional ! for negation
    if (current_.type == TokType::Word && current_.value == "!") {
        result->negated = true;
        advance();
    }

    result->commands.push_back(parse_command());

    while (current_.type == TokType::Pipe) {
        advance();
        while (current_.type == TokType::Newline) advance();
        result->commands.push_back(parse_command());
    }

    return result;
}

auto Parser::parse_command() -> Command {
    if (current_.type == TokType::LParen) {
        return parse_subshell();
    }

    if (current_.type == TokType::LBrace) {
        return parse_brace_group();
    }

    // Keywords for compound commands
    if (current_.type == TokType::Word && current_.keyword) {
        if (current_.value == "if") return parse_if();
        if (current_.value == "while") return parse_while();
        if (current_.value == "until") return parse_while(); // reuse, set is_until
        if (current_.value == "for") return parse_for();
    }

    return parse_simple_command();
}

auto Parser::parse_simple_command() -> SimpleCommand {
    SimpleCommand cmd;

    while (true) {
        // Try redirect
        auto redir = parse_redirect();
        if (redir) {
            cmd.redirs.push_back(std::move(*redir));
            continue;
        }

        if (current_.type == TokType::Word && !is_terminator(current_)) {
            // Check for assignment (VAR=value) before first command word
            auto& val = current_.value;
            if (cmd.words.empty() && !val.empty()) {
                auto eq = val.find('=');
                if (eq != std::string::npos && eq > 0) {
                    // Check LHS is a valid variable name
                    bool valid = true;
                    for (std::size_t i = 0; i < eq; ++i) {
                        if (!(std::isalnum(static_cast<unsigned char>(val[i])) || val[i] == '_') ||
                            (i == 0 && std::isdigit(static_cast<unsigned char>(val[i])))) {
                            valid = false;
                            break;
                        }
                    }
                    if (valid) {
                        cmd.assigns.emplace_back(val.substr(0, eq), val.substr(eq + 1));
                        advance();
                        continue;
                    }
                }
            }
            cmd.words.push_back(std::move(current_.value));
            advance();
            continue;
        }

        break;
    }

    return cmd;
}

auto Parser::parse_redirect() -> std::optional<Redir> {
    // Check for redirect: [n]<, [n]>, [n]>>, [n]<&, [n]>&
    int fd = -1;

    // Check if current token is a number followed by redirect operator
    // For simplicity, we peek at the lexer for patterns like "2>"
    bool is_redirect = false;
    TokType redir_type = TokType::Less;

    if (current_.type == TokType::Less || current_.type == TokType::Great ||
        current_.type == TokType::DGreate || current_.type == TokType::LessAnd ||
        current_.type == TokType::GreatAnd) {
        is_redirect = true;
        redir_type = current_.type;
        // Default fd: 0 for read, 1 for write
        fd = (redir_type == TokType::Less || redir_type == TokType::LessAnd) ? 0 : 1;
    } else if (current_.type == TokType::Word && !current_.value.empty()) {
        // Check for "2>", "2>>", "1>" etc.
        auto& val = current_.value;
        if (val.size() >= 2 && val[0] >= '0' && val[0] <= '9') {
            if (val[1] == '>' || val[1] == '<') {
                fd = val[0] - '0';
                if (val.size() >= 3 && val[1] == '>' && val[2] == '>') {
                    redir_type = TokType::DGreate;
                    current_.value = val.substr(3);
                    if (current_.value.empty()) advance();
                    is_redirect = true;
                } else if (val.size() >= 3 && val[1] == '>' && val[2] == '&') {
                    redir_type = TokType::GreatAnd;
                    current_.value = val.substr(3);
                    if (current_.value.empty()) advance();
                    is_redirect = true;
                } else if (val.size() >= 2 && val[1] == '<' && val.size() > 2 && val[2] == '&') {
                    redir_type = TokType::LessAnd;
                    current_.value = val.substr(3);
                    if (current_.value.empty()) advance();
                    is_redirect = true;
                } else {
                    redir_type = (val[1] == '>') ? TokType::Great : TokType::Less;
                    current_.value = val.substr(2);
                    if (current_.value.empty()) advance();
                    is_redirect = true;
                }
            }
        }
    }

    if (!is_redirect) return std::nullopt;

    advance(); // consume the redirect operator

    // Get target
    if (current_.type != TokType::Word) {
        std::fprintf(stderr, "cfbox sh: syntax error: missing redirect target\n");
        return std::nullopt;
    }

    Redir r;
    r.fd = fd;
    r.target = std::move(current_.value);
    advance();

    switch (redir_type) {
    case TokType::Less:    r.type = Redir::Read; break;
    case TokType::Great:   r.type = Redir::Write; break;
    case TokType::DGreate: r.type = Redir::Append; break;
    case TokType::LessAnd: r.type = Redir::DupIn; break;
    case TokType::GreatAnd: r.type = Redir::DupOut; break;
    default: r.type = Redir::Read; break;
    }

    return r;
}

auto Parser::parse_if() -> std::unique_ptr<IfClause> {
    auto result = std::make_unique<IfClause>();
    advance(); // consume 'if'

    // Parse condition (until 'then')
    auto cond = parse_compound_list();
    if (!expect_keyword("then")) {
        std::fprintf(stderr, "cfbox sh: syntax error: expected 'then'\n");
        return result;
    }

    // Parse then body
    auto body = parse_compound_list();
    result->conditions.push_back(std::move(cond));
    result->bodies.push_back(std::move(body));

    // Handle elif chains
    while (current_.type == TokType::Word && current_.value == "elif") {
        advance();
        auto elif_cond = parse_compound_list();
        if (!expect_keyword("then")) {
            std::fprintf(stderr, "cfbox sh: syntax error: expected 'then'\n");
            break;
        }
        auto elif_body = parse_compound_list();
        result->conditions.push_back(std::move(elif_cond));
        result->bodies.push_back(std::move(elif_body));
    }

    // Handle else
    if (current_.type == TokType::Word && current_.value == "else") {
        advance();
        result->else_body = parse_compound_list();
    }

    if (!expect_keyword("fi")) {
        std::fprintf(stderr, "cfbox sh: syntax error: expected 'fi'\n");
    }

    return result;
}

auto Parser::parse_while() -> std::unique_ptr<WhileClause> {
    auto result = std::make_unique<WhileClause>();
    result->is_until = (current_.value == "until");
    advance(); // consume 'while' or 'until'

    result->condition = parse_compound_list();
    if (!expect_keyword("do")) {
        std::fprintf(stderr, "cfbox sh: syntax error: expected 'do'\n");
        return result;
    }

    result->body = parse_compound_list();
    if (!expect_keyword("done")) {
        std::fprintf(stderr, "cfbox sh: syntax error: expected 'done'\n");
    }

    return result;
}

auto Parser::parse_for() -> std::unique_ptr<ForClause> {
    auto result = std::make_unique<ForClause>();
    advance(); // consume 'for'

    if (current_.type != TokType::Word) {
        std::fprintf(stderr, "cfbox sh: syntax error: expected variable name after 'for'\n");
        return result;
    }
    result->var_name = std::move(current_.value);
    advance();

    // Optional 'in' word_list
    if (current_.type == TokType::Word && current_.value == "in") {
        advance();
        while (current_.type == TokType::Word &&
               !(current_.keyword && (current_.value == "do" || current_.value == "done"))) {
            result->words.push_back(std::move(current_.value));
            advance();
        }
    }

    // Skip optional ';' or newlines
    while (current_.type == TokType::Semi || current_.type == TokType::Newline) advance();

    if (!expect_keyword("do")) {
        std::fprintf(stderr, "cfbox sh: syntax error: expected 'do'\n");
        return result;
    }

    result->body = parse_compound_list();
    if (!expect_keyword("done")) {
        std::fprintf(stderr, "cfbox sh: syntax error: expected 'done'\n");
    }

    return result;
}

auto Parser::parse_subshell() -> std::unique_ptr<Subshell> {
    advance(); // consume '('
    auto result = std::make_unique<Subshell>();
    result->body = parse_compound_list();
    if (!expect(TokType::RParen)) {
        std::fprintf(stderr, "cfbox sh: syntax error: expected ')'\n");
    }
    return result;
}

auto Parser::parse_brace_group() -> std::unique_ptr<BraceGroup> {
    advance(); // consume '{'
    auto result = std::make_unique<BraceGroup>();
    // Skip space after {
    if (current_.type == TokType::Newline) advance();
    result->body = parse_compound_list();
    if (current_.type != TokType::RBrace) {
        std::fprintf(stderr, "cfbox sh: syntax error: expected '}'\n");
    } else {
        advance();
    }
    return result;
}

} // namespace cfbox::sh
