#pragma once

#include <cstdio>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

namespace cfbox::sh {

// ── Token ────────────────────────────────────────────────────────
enum class TokType {
    Word, Newline, Eof,
    Pipe, Semi, And, Or,       // | ; && ||
    LParen, RParen, LBrace, RBrace,
    Less, Great, DGreate,      // < > >>
    LessAnd, GreatAnd,         // <& >&
    // Keywords stored as Word with keyword flag
};

struct Token {
    TokType type = TokType::Word;
    std::string value;
    bool keyword = false;  // true for if/then/else/fi/while/until/do/done/for/in/case/esac
};

// ── Redirection ──────────────────────────────────────────────────
struct Redir {
    enum Type { Read, Write, Append, DupIn, DupOut };
    int fd = -1;      // target fd (default inferred: 0 for Read, 1 for Write/Append)
    Type type = Read;
    std::string target; // filename or fd number for dup
};

// ── AST Nodes ────────────────────────────────────────────────────
struct SimpleCommand {
    std::vector<std::string> words;          // command + args (pre-expansion)
    std::vector<Redir> redirs;
    std::vector<std::pair<std::string, std::string>> assigns; // VAR=val before cmd
};

struct Pipeline;
struct IfClause;
struct WhileClause;
struct ForClause;
struct Subshell;
struct BraceGroup;

using Command = std::variant<SimpleCommand,
                             std::unique_ptr<Pipeline>,
                             std::unique_ptr<IfClause>,
                             std::unique_ptr<WhileClause>,
                             std::unique_ptr<ForClause>,
                             std::unique_ptr<Subshell>,
                             std::unique_ptr<BraceGroup>>;

struct Pipeline {
    std::vector<Command> commands;
    bool negated = false;
};

struct AndOr {
    enum class Op { Semi, And, Or };
    // entries[0].first is unused (Semi); entries[0].second is the first pipeline
    std::vector<std::pair<Op, std::unique_ptr<Pipeline>>> entries;
};

struct IfClause {
    std::vector<std::unique_ptr<AndOr>> conditions; // if cond; elif cond; ...
    std::vector<std::unique_ptr<AndOr>> bodies;     // then body; elif body; ...
    std::unique_ptr<AndOr> else_body;               // optional else
};

struct WhileClause {
    bool is_until = false;
    std::unique_ptr<AndOr> condition;
    std::unique_ptr<AndOr> body;
};

struct ForClause {
    std::string var_name;
    std::vector<std::string> words; // empty means "$@"
    std::unique_ptr<AndOr> body;
};

struct Subshell {
    std::unique_ptr<AndOr> body;
};

struct BraceGroup {
    std::unique_ptr<AndOr> body;
};

// ── Shell State ──────────────────────────────────────────────────
class ShellState {
public:
    // Variables
    auto get_var(std::string_view name) const -> std::string;
    auto set_var(const std::string& name, const std::string& value) -> void;
    auto unset_var(const std::string& name) -> void;
    auto export_var(const std::string& name) -> void;
    auto is_exported(const std::string& name) const -> bool;
    auto all_vars() const -> const std::unordered_map<std::string, std::string>& { return vars_; }

    // Positional parameters
    auto positional_params() const -> const std::vector<std::string>& { return positional_; }
    auto set_positional(std::vector<std::string> args) -> void;
    auto shift(int n) -> void;

    // Special parameters
    auto last_status() const -> int { return last_status_; }
    auto set_last_status(int s) -> void { last_status_ = s; }
    auto shell_pid() const -> int;

    // Script name ($0)
    auto script_name() const -> const std::string& { return script_name_; }
    auto set_script_name(std::string name) -> void { script_name_ = std::move(name); }

    // Control flow flags
    bool should_exit = false;
    int exit_status = 0;
    bool break_loop = false;
    int break_count = 0;
    bool continue_loop = false;

private:
    std::unordered_map<std::string, std::string> vars_;
    std::unordered_set<std::string> exported_;
    std::vector<std::string> positional_;
    int last_status_ = 0;
    std::string script_name_;
};

// ── Lexer ────────────────────────────────────────────────────────
class Lexer {
public:
    explicit Lexer(std::string_view input);

    auto next_token() -> Token;
    auto peek_token() -> const Token&;

private:
    auto advance() -> char;
    auto peek() const -> char;
    auto at_end() const -> bool;

    auto skip_spaces_and_comments() -> void;
    auto read_word() -> Token;
    auto read_operator() -> std::optional<Token>;
    auto read_single_quoted() -> std::string;
    auto read_double_quoted() -> std::string;

    auto is_keyword(std::string_view word) const -> bool;

    std::string_view input_;
    std::size_t pos_ = 0;
    Token cached_;
    bool has_cached_ = false;
};

// ── Parser ───────────────────────────────────────────────────────
class Parser {
public:
    explicit Parser(Lexer& lexer);

    auto parse_program() -> std::unique_ptr<AndOr>;

private:
    auto advance() -> const Token&;
    auto peek() const -> const Token&;
    auto expect(TokType type) -> bool;
    auto expect_keyword(std::string_view kw) -> bool;

    auto parse_compound_list() -> std::unique_ptr<AndOr>;
    auto parse_and_or() -> std::unique_ptr<AndOr>;
    auto parse_pipeline() -> std::unique_ptr<Pipeline>;
    auto parse_command() -> Command;
    auto parse_simple_command() -> SimpleCommand;
    auto parse_redirect() -> std::optional<Redir>;
    auto parse_if() -> std::unique_ptr<IfClause>;
    auto parse_while() -> std::unique_ptr<WhileClause>;
    auto parse_for() -> std::unique_ptr<ForClause>;
    auto parse_subshell() -> std::unique_ptr<Subshell>;
    auto parse_brace_group() -> std::unique_ptr<BraceGroup>;

    Lexer& lexer_;
    Token current_;
};

// ── Executor ─────────────────────────────────────────────────────
auto execute(AndOr& node, ShellState& state) -> int;
auto execute_command(Command& cmd, ShellState& state) -> int;

// ── Builtins ─────────────────────────────────────────────────────
using BuiltinFunc = int (*)(std::vector<std::string>& args, ShellState& state);

auto get_builtins() -> const std::unordered_map<std::string, BuiltinFunc>&;
auto is_builtin(const std::string& name) -> bool;
auto run_builtin(const std::string& name, std::vector<std::string>& args, ShellState& state) -> int;

// ── Word Expansion ───────────────────────────────────────────────
auto expand_word(const std::string& word, const ShellState& state) -> std::vector<std::string>;
auto expand_words(const std::vector<std::string>& words, const ShellState& state) -> std::vector<std::string>;

} // namespace cfbox::sh
