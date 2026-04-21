#pragma once

#include <cstdio>
#include <map>
#include <memory>
#include <regex.h>
#include <string>
#include <vector>

namespace cfbox::awk {

enum class TokType {
    Number, String, Ident, Regex,
    Begin, End, Print, Printf, If, Else, While, For, In,
    Function, Return, Delete, Exit, NextStmt,
    // Operators
    Add, Sub, Mul, Div, Mod, Pow,
    Assign, AddAssign, SubAssign, MulAssign, DivAssign, ModAssign,
    Eq, Ne, Lt, Gt, Le, Ge,
    Match, NotMatch,
    And, Or, Not,
    Concat, Pipe,
    Inc, Dec,
    LParen, RParen, LBrace, RBrace, LBracket, RBracket,
    Semicolon, Newline, Comma, Dollar, Question, Colon,
    Eof
};

struct Token {
    TokType type;
    std::string text;
};

enum class NodeType {
    Program, Rule, Action, Block,
    Print, Printf,
    If, While, For, ForIn,
    Assign, BinaryOp, UnaryOp,
    Number, String, Ident, Field, ArrayAccess,
    FuncCall, Return, Delete, Exit, NextStmt,
    Function, Regex,
    RegexMatch, NotMatch,
    Ternary,
    Concat, Pipe,
    Getline
};

struct Node {
    NodeType type;
    std::string value;
    std::vector<std::shared_ptr<Node>> children;
    std::string str_val;
};

using NodePtr = std::shared_ptr<Node>;

inline auto make_node(NodeType t, std::string val = "") -> NodePtr {
    auto n = std::make_shared<Node>();
    n->type = t;
    n->value = std::move(val);
    return n;
}

struct AwkState {
    std::vector<std::string> fields;
    int nr = 0;
    std::string filename;
    std::string record;
    std::map<std::string, std::string> vars;
    std::map<std::string, std::map<std::string, std::string>> arrays;
    std::map<std::string, std::pair<std::vector<std::string>, NodePtr>> functions;
    int exit_code = 0;
    bool should_exit = false;
    bool should_next = false;
    bool begin_done = false;
    bool end_done = false;
    std::string ofs = " ";
    std::string ors = "\n";
    std::string fs = " ";
    std::string rs = "\n";
    std::string subsep = "\x1f";
    std::string output_field = "";
};

// Forward declarations
auto awk_tokenize(const std::string& src) -> std::vector<Token>;
auto awk_parse(const std::string& src) -> NodePtr;
auto awk_execute(NodePtr ast, AwkState& state, const std::vector<std::string>& files) -> int;

} // namespace cfbox::awk
