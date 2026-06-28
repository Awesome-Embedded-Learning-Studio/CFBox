#pragma once

#include <csignal>
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
    Pipe, Semi, DSemi, And, Or,       // | ; ;; && ||
    LParen, RParen, LBrace, RBrace,
    Less, Great, DGreate,      // < > >>
    DLess, DLessDash,          // << <<-
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
    enum Type { Read, Write, Append, DupIn, DupOut, HereDoc };
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
struct CaseClause;
struct FuncDef;

using Command = std::variant<SimpleCommand,
                             std::unique_ptr<Pipeline>,
                             std::unique_ptr<IfClause>,
                             std::unique_ptr<WhileClause>,
                             std::unique_ptr<ForClause>,
                             std::unique_ptr<Subshell>,
                             std::unique_ptr<BraceGroup>,
                             std::unique_ptr<CaseClause>,
                             std::unique_ptr<FuncDef>>;

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

struct CaseBranch {
    std::vector<std::string> patterns;  // pat1 | pat2 (glob, pre-expansion)
    std::unique_ptr<AndOr> body;        // nullptr for an empty branch body
};

struct CaseClause {
    std::string word;                   // value to match (pre-expansion)
    std::vector<CaseBranch> branches;
};

struct FuncDef {
    std::string name;
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

    // Functions
    auto define_function(const std::string& name, std::unique_ptr<AndOr> body) -> void;
    [[nodiscard]] auto is_function(const std::string& name) const -> bool;
    auto get_function(const std::string& name) -> AndOr*;

    // Local variable scopes (function-local variables)
    auto push_scope() -> void;
    auto pop_scope() -> void;
    auto set_local(const std::string& name, const std::string& value) -> void;
    [[nodiscard]] auto in_function() const -> bool { return !local_scopes_.empty(); }

    // Signal / EXIT traps
    auto set_trap(int sig, const std::string& cmd) -> void { traps_[sig] = cmd; }
    [[nodiscard]] auto get_trap(int sig) const -> std::string {
        auto it = traps_.find(sig);
        return it != traps_.end() ? it->second : std::string{};
    }
    auto clear_trap(int sig) -> void { traps_.erase(sig); }
    [[nodiscard]] auto all_traps() const -> const std::unordered_map<int, std::string>& { return traps_; }

    // Control flow flags
    bool should_exit = false;
    int exit_status = 0;
    int break_depth = 0;     // break N: counts enclosing loops to exit
    bool continue_loop = false;
    bool return_pending = false;
    int return_status = 0;

private:
    std::unordered_map<std::string, std::string> vars_;
    std::unordered_set<std::string> exported_;
    std::vector<std::string> positional_;
    int last_status_ = 0;
    std::string script_name_;
    std::unordered_map<std::string, std::unique_ptr<AndOr>> functions_;
    std::vector<std::unordered_map<std::string, std::string>> local_scopes_;
    std::unordered_map<int, std::string> traps_;
};

// Set by the signal handler, consumed by the executor to run the trap command.
inline volatile std::sig_atomic_t trap_pending_signal{0};

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
    auto parse_case() -> std::unique_ptr<CaseClause>;
    auto parse_func() -> std::unique_ptr<FuncDef>;

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
// Expand param/arith/command/quote but skip field splitting and globbing —
// for case words/patterns where * ? [ ] are pattern syntax, not filename globs.
auto expand_noglob(const std::string& word, const ShellState& state) -> std::string;

} // namespace cfbox::sh
