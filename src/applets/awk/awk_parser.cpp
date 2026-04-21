#include "awk.hpp"
#include <cstdlib>

namespace cfbox::awk {

class Parser {
    std::vector<Token> toks_;
    std::size_t pos_ = 0;
public:
    explicit Parser(std::vector<Token> toks) : toks_(std::move(toks)) {}

    auto parse() -> NodePtr {
        auto prog = make_node(NodeType::Program);
        while (peek().type != TokType::Eof) {
            skip_newlines();
            if (peek().type == TokType::Eof) break;
            if (peek().type == TokType::Function) {
                prog->children.push_back(parse_function());
            } else {
                prog->children.push_back(parse_rule());
            }
            skip_newlines();
        }
        return prog;
    }

private:
    auto peek() const -> const Token& { return toks_[pos_]; }
    auto advance() -> Token { return toks_[pos_++]; }
    auto expect(TokType t) -> Token {
        auto tok = advance();
        if (tok.type != t) {
            std::fprintf(stderr, "cfbox awk: expected token type %d, got '%s'\n",
                         static_cast<int>(t), tok.text.c_str());
        }
        return tok;
    }
    auto match(TokType t) -> bool { if (peek().type == t) { ++pos_; return true; } return false; }
    void skip_newlines() { while (peek().type == TokType::Newline) ++pos_; }
    void skip_terminators() { while (peek().type == TokType::Newline || peek().type == TokType::Semicolon) ++pos_; }

    auto parse_function() -> NodePtr {
        advance(); // 'function'
        auto name = expect(TokType::Ident).text;
        expect(TokType::LParen);
        std::vector<std::string> params;
        if (peek().type == TokType::Ident) {
            params.push_back(advance().text);
            while (match(TokType::Comma)) {
                params.push_back(expect(TokType::Ident).text);
            }
        }
        expect(TokType::RParen);
        skip_newlines();
        auto body = parse_block();
        auto node = make_node(NodeType::Function, name);
        node->str_val = name; // store function name
        // Store params in first child, body in second
        auto params_node = make_node(NodeType::Block);
        for (const auto& p : params) params_node->children.push_back(make_node(NodeType::Ident, p));
        node->children.push_back(params_node);
        node->children.push_back(body);
        return node;
    }

    auto parse_rule() -> NodePtr {
        auto rule = make_node(NodeType::Rule);
        if (peek().type == TokType::Begin) {
            advance();
            rule->children.push_back(make_node(NodeType::Ident, "BEGIN"));
        } else if (peek().type == TokType::End) {
            advance();
            rule->children.push_back(make_node(NodeType::Ident, "END"));
        } else if (peek().type != TokType::LBrace) {
            rule->children.push_back(parse_expr());
        }
        skip_newlines();
        if (peek().type == TokType::LBrace) {
            rule->children.push_back(parse_block());
        }
        return rule;
    }

    auto parse_block() -> NodePtr {
        expect(TokType::LBrace);
        skip_newlines();
        auto block = make_node(NodeType::Block);
        while (peek().type != TokType::RBrace && peek().type != TokType::Eof) {
            block->children.push_back(parse_stmt());
            skip_terminators();
        }
        expect(TokType::RBrace);
        return block;
    }

    auto parse_stmt() -> NodePtr {
        skip_newlines();
        switch (peek().type) {
            case TokType::Print: return parse_print();
            case TokType::Printf: return parse_printf_stmt();
            case TokType::If: return parse_if();
            case TokType::While: return parse_while();
            case TokType::For: return parse_for();
            case TokType::Return: return parse_return();
            case TokType::Delete: return parse_delete();
            case TokType::Exit: advance(); return make_node(NodeType::Exit);
            case TokType::NextStmt: advance(); return make_node(NodeType::NextStmt);
            case TokType::LBrace: return parse_block();
            default: return parse_expr_stmt();
        }
    }

    auto parse_print() -> NodePtr {
        advance(); // 'print'
        auto node = make_node(NodeType::Print);
        if (peek().type != TokType::Newline && peek().type != TokType::Semicolon &&
            peek().type != TokType::RBrace && peek().type != TokType::Eof &&
            peek().type != TokType::Pipe && peek().type != TokType::Gt) {
            node->children.push_back(parse_expr());
            while (match(TokType::Comma)) {
                node->children.push_back(parse_expr());
            }
        }
        if (match(TokType::Gt) || match(TokType::Pipe)) {
            node->children.push_back(parse_primary());
        }
        return node;
    }

    auto parse_printf_stmt() -> NodePtr {
        advance(); // 'printf'
        auto node = make_node(NodeType::Printf);
        node->children.push_back(parse_expr());
        while (match(TokType::Comma)) {
            node->children.push_back(parse_expr());
        }
        return node;
    }

    auto parse_if() -> NodePtr {
        advance(); // 'if'
        expect(TokType::LParen);
        auto node = make_node(NodeType::If);
        node->children.push_back(parse_expr());
        expect(TokType::RParen);
        skip_newlines();
        node->children.push_back(parse_stmt());
        skip_newlines();
        if (peek().type == TokType::Else) {
            advance();
            skip_newlines();
            node->children.push_back(parse_stmt());
        }
        return node;
    }

    auto parse_while() -> NodePtr {
        advance();
        expect(TokType::LParen);
        auto node = make_node(NodeType::While);
        node->children.push_back(parse_expr());
        expect(TokType::RParen);
        skip_newlines();
        node->children.push_back(parse_stmt());
        return node;
    }

    auto parse_for() -> NodePtr {
        advance();
        expect(TokType::LParen);
        if (peek().type == TokType::Ident) {
            // Check for for-in: for (var in array)
            auto saved = pos_;
            auto name = advance().text;
            if (peek().type == TokType::In) {
                advance();
                auto arr = expect(TokType::Ident).text;
                expect(TokType::RParen);
                skip_newlines();
                auto body = parse_stmt();
                auto node = make_node(NodeType::ForIn);
                node->children.push_back(make_node(NodeType::Ident, name));
                node->children.push_back(make_node(NodeType::Ident, arr));
                node->children.push_back(body);
                return node;
            }
            pos_ = saved;
        }
        auto node = make_node(NodeType::For);
        if (peek().type != TokType::Semicolon) node->children.push_back(parse_expr());
        expect(TokType::Semicolon);
        node->children.push_back(parse_expr());
        expect(TokType::Semicolon);
        if (peek().type != TokType::RParen) node->children.push_back(parse_expr());
        expect(TokType::RParen);
        skip_newlines();
        node->children.push_back(parse_stmt());
        return node;
    }

    auto parse_return() -> NodePtr {
        advance();
        auto node = make_node(NodeType::Return);
        if (peek().type != TokType::Newline && peek().type != TokType::Semicolon && peek().type != TokType::RBrace) {
            node->children.push_back(parse_expr());
        }
        return node;
    }

    auto parse_delete() -> NodePtr {
        advance();
        auto node = make_node(NodeType::Delete);
        node->children.push_back(parse_primary());
        return node;
    }

    auto parse_expr_stmt() -> NodePtr { return parse_expr(); }

    auto parse_expr() -> NodePtr { return parse_assign(); }

    auto parse_assign() -> NodePtr {
        auto left = parse_ternary();
        if (peek().type == TokType::Assign || peek().type == TokType::AddAssign ||
            peek().type == TokType::SubAssign || peek().type == TokType::MulAssign ||
            peek().type == TokType::DivAssign || peek().type == TokType::ModAssign) {
            auto op = advance();
            auto right = parse_assign();
            auto node = make_node(NodeType::Assign, op.text);
            node->children.push_back(left);
            node->children.push_back(right);
            return node;
        }
        return left;
    }

    auto parse_ternary() -> NodePtr {
        auto cond = parse_or();
        if (match(TokType::Question)) {
            auto then_val = parse_assign();
            expect(TokType::Colon);
            auto else_val = parse_assign();
            auto node = make_node(NodeType::Ternary);
            node->children.push_back(cond);
            node->children.push_back(then_val);
            node->children.push_back(else_val);
            return node;
        }
        return cond;
    }

    auto parse_or() -> NodePtr {
        auto left = parse_and();
        while (peek().type == TokType::Or) {
            advance();
            auto right = parse_and();
            auto node = make_node(NodeType::BinaryOp, "||");
            node->children.push_back(left);
            node->children.push_back(right);
            left = node;
        }
        return left;
    }

    auto parse_and() -> NodePtr {
        auto left = parse_match();
        while (peek().type == TokType::And) {
            advance();
            auto right = parse_match();
            auto node = make_node(NodeType::BinaryOp, "&&");
            node->children.push_back(left);
            node->children.push_back(right);
            left = node;
        }
        return left;
    }

    auto parse_match() -> NodePtr {
        auto left = parse_compare();
        while (peek().type == TokType::Match || peek().type == TokType::NotMatch) {
            auto op = advance();
            auto right = parse_primary();
            auto node = make_node(op.type == TokType::Match ? NodeType::RegexMatch : NodeType::NotMatch);
            node->children.push_back(left);
            node->children.push_back(right);
            left = node;
        }
        return left;
    }

    auto parse_compare() -> NodePtr {
        auto left = parse_concat();
        while (peek().type == TokType::Eq || peek().type == TokType::Ne ||
               peek().type == TokType::Lt || peek().type == TokType::Gt ||
               peek().type == TokType::Le || peek().type == TokType::Ge) {
            auto op = advance();
            auto right = parse_concat();
            auto node = make_node(NodeType::BinaryOp, op.text);
            node->children.push_back(left);
            node->children.push_back(right);
            left = node;
        }
        return left;
    }

    auto parse_concat() -> NodePtr {
        auto left = parse_add();
        // Implicit concatenation: two adjacent strings/idents/fields
        while ((peek().type == TokType::String || peek().type == TokType::Ident ||
                peek().type == TokType::Dollar || peek().type == TokType::LParen ||
                peek().type == TokType::Number) && peek().type != TokType::Eof) {
            auto right = parse_add();
            auto node = make_node(NodeType::Concat);
            node->children.push_back(left);
            node->children.push_back(right);
            left = node;
        }
        return left;
    }

    auto parse_add() -> NodePtr {
        auto left = parse_mul();
        while (peek().type == TokType::Add || peek().type == TokType::Sub) {
            auto op = advance();
            auto right = parse_mul();
            auto node = make_node(NodeType::BinaryOp, op.text);
            node->children.push_back(left);
            node->children.push_back(right);
            left = node;
        }
        return left;
    }

    auto parse_mul() -> NodePtr {
        auto left = parse_unary();
        while (peek().type == TokType::Mul || peek().type == TokType::Div ||
               peek().type == TokType::Mod || peek().type == TokType::Pow) {
            auto op = advance();
            auto right = parse_unary();
            auto node = make_node(NodeType::BinaryOp, op.text);
            node->children.push_back(left);
            node->children.push_back(right);
            left = node;
        }
        return left;
    }

    auto parse_unary() -> NodePtr {
        if (peek().type == TokType::Not) {
            advance();
            auto node = make_node(NodeType::UnaryOp, "!");
            node->children.push_back(parse_unary());
            return node;
        }
        if (peek().type == TokType::Sub) {
            advance();
            auto node = make_node(NodeType::UnaryOp, "-");
            node->children.push_back(parse_unary());
            return node;
        }
        if (peek().type == TokType::Inc || peek().type == TokType::Dec) {
            auto op = advance();
            auto node = make_node(NodeType::UnaryOp, op.text);
            node->children.push_back(parse_primary());
            return node;
        }
        return parse_postfix();
    }

    auto parse_postfix() -> NodePtr {
        auto node = parse_primary();
        if (peek().type == TokType::Inc || peek().type == TokType::Dec) {
            auto op = advance();
            auto post = make_node(NodeType::UnaryOp, op.text);
            post->children.push_back(node);
            return post;
        }
        return node;
    }

    auto parse_primary() -> NodePtr {
        switch (peek().type) {
            case TokType::Number: return make_node(NodeType::Number, advance().text);
            case TokType::String: return make_node(NodeType::String, advance().text);
            case TokType::Regex: return make_node(NodeType::Regex, advance().text);
            case TokType::Dollar: {
                advance();
                auto node = make_node(NodeType::Field);
                node->children.push_back(parse_primary());
                return node;
            }
            case TokType::LParen: {
                advance();
                auto expr = parse_expr();
                expect(TokType::RParen);
                return expr;
            }
            case TokType::Ident: {
                auto name = peek().text;
                advance();
                if (peek().type == TokType::LParen) {
                    // Function call
                    advance();
                    auto node = make_node(NodeType::FuncCall, name);
                    if (peek().type != TokType::RParen) {
                        node->children.push_back(parse_expr());
                        while (match(TokType::Comma)) {
                            node->children.push_back(parse_expr());
                        }
                    }
                    expect(TokType::RParen);
                    return node;
                }
                if (peek().type == TokType::LBracket) {
                    advance();
                    auto idx = parse_expr();
                    expect(TokType::RBracket);
                    auto node = make_node(NodeType::ArrayAccess, name);
                    node->children.push_back(idx);
                    return node;
                }
                return make_node(NodeType::Ident, name);
            }
            default:
                return make_node(NodeType::Number, "0");
        }
    }
};

auto awk_parse(const std::string& src) -> NodePtr {
    auto tokens = awk_tokenize(src);
    Parser parser(std::move(tokens));
    return parser.parse();
}

} // namespace cfbox::awk
