#include "awk.hpp"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <regex.h>
#include <sstream>

namespace cfbox::awk {

class Executor {
    AwkState& st_;
public:
    explicit Executor(AwkState& st) : st_(st) {
        st_.vars["FS"] = st_.fs;
        st_.vars["OFS"] = st_.ofs;
        st_.vars["ORS"] = st_.ors;
        st_.vars["NR"] = "0";
        st_.vars["NF"] = "0";
        st_.vars["SUBSEP"] = st_.subsep;
    }

    auto run(NodePtr prog, const std::vector<std::string>& input_files) -> int {
        // First pass: register functions
        for (auto& rule : prog->children) {
            if (rule->type == NodeType::Function) {
                auto name = rule->str_val;
                std::vector<std::string> params;
                if (!rule->children.empty()) {
                    for (auto& p : rule->children[0]->children) params.push_back(p->value);
                }
                auto body = rule->children.size() > 1 ? rule->children[1] : make_node(NodeType::Block);
                st_.functions[name] = {params, body};
            }
        }

        // BEGIN
        for (auto& rule : prog->children) {
            if (rule->type != NodeType::Rule || rule->children.empty()) continue;
            if (rule->children[0]->type == NodeType::Ident && rule->children[0]->value == "BEGIN") {
                if (rule->children.size() > 1) exec_block(rule->children[1]);
                if (st_.should_exit) return st_.exit_code;
            }
        }

        // Process input
        st_.begin_done = true;
        if (!input_files.empty()) {
            for (const auto& file : input_files) {
                process_file(file, prog);
                if (st_.should_exit) break;
            }
        } else {
            process_stdin(prog);
        }

        // END
        st_.end_done = true;
        for (auto& rule : prog->children) {
            if (rule->type != NodeType::Rule || rule->children.empty()) continue;
            if (rule->children[0]->type == NodeType::Ident && rule->children[0]->value == "END") {
                if (rule->children.size() > 1) exec_block(rule->children[1]);
                if (st_.should_exit) break;
            }
        }
        return st_.exit_code;
    }

private:
    auto to_number(const std::string& s) -> double {
        if (s.empty()) return 0.0;
        char* end = nullptr;
        auto v = std::strtod(s.c_str(), &end);
        return (end == s.c_str()) ? 0.0 : v;
    }

    auto to_string(double v) -> std::string {
        if (v == static_cast<long long>(v) && std::abs(v) < 1e15) {
            return std::to_string(static_cast<long long>(v));
        }
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%.6g", v);
        return buf;
    }

    auto is_true(const std::string& s) -> bool {
        return !s.empty() && s != "0" && s != "0.0";
    }

    auto eval(NodePtr node) -> std::string {
        if (!node) return "";
        switch (node->type) {
            case NodeType::Number: return node->value;
            case NodeType::String: return node->value;
            case NodeType::Ident: return get_var(node->value);
            case NodeType::Field: {
                auto idx = static_cast<int>(to_number(eval(node->children[0])));
                if (idx == 0) return st_.record;
                if (idx > 0 && idx <= static_cast<int>(st_.fields.size())) return st_.fields[idx - 1];
                return "";
            }
            case NodeType::ArrayAccess: {
                auto key = eval(node->children[0]);
                return st_.arrays[node->value][key];
            }
            case NodeType::Assign: return eval_assign(node);
            case NodeType::BinaryOp: return eval_binary(node);
            case NodeType::UnaryOp: return eval_unary(node);
            case NodeType::Ternary: {
                if (is_true(eval(node->children[0]))) return eval(node->children[1]);
                return eval(node->children[2]);
            }
            case NodeType::Concat: return eval(node->children[0]) + eval(node->children[1]);
            case NodeType::RegexMatch: {
                auto str = eval(node->children[0]);
                auto pat = eval(node->children[1]);
                return regex_match(str, pat) ? "1" : "0";
            }
            case NodeType::NotMatch: {
                auto str = eval(node->children[0]);
                auto pat = eval(node->children[1]);
                return regex_match(str, pat) ? "0" : "1";
            }
            case NodeType::FuncCall: return eval_func_call(node);
            default: return "";
        }
    }

    auto get_var(const std::string& name) -> std::string& {
        return st_.vars[name];
    }

    auto eval_assign(NodePtr node) -> std::string {
        auto op = node->value;
        auto& target_node = node->children[0];
        auto rhs = eval(node->children[1]);

        if (target_node->type == NodeType::Ident) {
            auto& var = get_var(target_node->value);
            if (op == "=") { var = rhs; }
            else if (op == "+=") { var = to_string(to_number(var) + to_number(rhs)); }
            else if (op == "-=") { var = to_string(to_number(var) - to_number(rhs)); }
            else if (op == "*=") { var = to_string(to_number(var) * to_number(rhs)); }
            else if (op == "/=") { var = to_string(to_number(var) / to_number(rhs)); }
            else if (op == "%=") { var = to_string(std::fmod(to_number(var), to_number(rhs))); }
            return var;
        }
        if (target_node->type == NodeType::Field) {
            auto idx = static_cast<int>(to_number(eval(target_node->children[0])));
            if (idx > 0 && idx <= static_cast<int>(st_.fields.size())) {
                st_.fields[idx - 1] = rhs;
            }
            return rhs;
        }
        if (target_node->type == NodeType::ArrayAccess) {
            auto key = eval(target_node->children[0]);
            st_.arrays[target_node->value][key] = rhs;
            return rhs;
        }
        return rhs;
    }

    auto eval_binary(NodePtr node) -> std::string {
        auto op = node->value;
        auto lv = eval(node->children[0]);
        auto rv = eval(node->children[1]);
        auto ln = to_number(lv), rn = to_number(rv);

        if (op == "+") return to_string(ln + rn);
        if (op == "-") return to_string(ln - rn);
        if (op == "*") return to_string(ln * rn);
        if (op == "/") return (rn != 0) ? to_string(ln / rn) : "0";
        if (op == "%") return (rn != 0) ? to_string(std::fmod(ln, rn)) : "0";
        if (op == "**") return to_string(std::pow(ln, rn));
        if (op == "<") return (ln < rn) ? "1" : "0";
        if (op == "<=") return (ln <= rn) ? "1" : "0";
        if (op == ">") return (ln > rn) ? "1" : "0";
        if (op == ">=") return (ln >= rn) ? "1" : "0";
        if (op == "==") return (lv == rv) ? "1" : "0";
        if (op == "!=") return (lv != rv) ? "1" : "0";
        if (op == "&&") return (is_true(lv) && is_true(rv)) ? "1" : "0";
        if (op == "||") return (is_true(lv) || is_true(rv)) ? "1" : "0";
        return "0";
    }

    auto eval_unary(NodePtr node) -> std::string {
        auto v = eval(node->children[0]);
        if (node->value == "!") return is_true(v) ? "0" : "1";
        if (node->value == "-") return to_string(-to_number(v));
        if (node->value == "++") {
            auto r = to_string(to_number(v) + 1);
            if (node->children[0]->type == NodeType::Ident) get_var(node->children[0]->value) = r;
            return r;
        }
        if (node->value == "--") {
            auto r = to_string(to_number(v) - 1);
            if (node->children[0]->type == NodeType::Ident) get_var(node->children[0]->value) = r;
            return r;
        }
        return v;
    }

    auto regex_match(const std::string& str, const std::string& pat) -> bool {
        regex_t regex;
        if (regcomp(&regex, pat.c_str(), REG_EXTENDED | REG_NOSUB) != 0) return false;
        auto ret = regexec(&regex, str.c_str(), 0, nullptr, 0);
        regfree(&regex);
        return ret == 0;
    }

    auto eval_func_call(NodePtr node) -> std::string {
        auto name = node->value;
        std::vector<std::string> args;
        for (auto& c : node->children) args.push_back(eval(c));

        // Built-in functions
        if (name == "length") {
            if (args.empty()) return to_string(static_cast<double>(st_.record.size()));
            return to_string(static_cast<double>(args[0].size()));
        }
        if (name == "substr") {
            if (args.size() < 2) return "";
            auto pos = static_cast<std::size_t>(to_number(args[1]));
            if (pos < 1) pos = 1;
            auto len = args.size() > 2 ? static_cast<std::size_t>(to_number(args[2])) : std::string::npos;
            return args[0].substr(pos - 1, len);
        }
        if (name == "index") {
            if (args.size() < 2) return "0";
            auto pos = args[0].find(args[1]);
            return pos != std::string::npos ? to_string(static_cast<double>(pos + 1)) : "0";
        }
        if (name == "split") {
            if (args.size() < 2) return "0";
            auto sep = args.size() > 2 ? args[2] : st_.fs;
            auto& arr = st_.arrays[args[1]];
            arr.clear();
            std::vector<std::string> parts;
            if (sep.size() == 1) {
                std::string tok;
                for (char c : args[0]) {
                    if (c == sep[0]) { if (!tok.empty()) { parts.push_back(tok); tok.clear(); } }
                    else tok += c;
                }
                if (!tok.empty()) parts.push_back(tok);
            } else {
                // regex split
                regex_t regex;
                if (regcomp(&regex, sep.c_str(), REG_EXTENDED) == 0) {
                    auto* p = args[0].c_str();
                    while (*p) {
                        regmatch_t m;
                        if (regexec(&regex, p, 1, &m, 0) == 0 && m.rm_so >= 0) {
                            parts.emplace_back(p, static_cast<std::size_t>(m.rm_so));
                            p += m.rm_eo;
                        } else {
                            parts.push_back(p);
                            break;
                        }
                    }
                    regfree(&regex);
                }
            }
            for (std::size_t i = 0; i < parts.size(); ++i) {
                arr[to_string(static_cast<double>(i + 1))] = parts[i];
            }
            return to_string(static_cast<double>(parts.size()));
        }
        if (name == "tolower") return args.empty() ? "" : [&]{ auto s = args[0]; for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c))); return s; }();
        if (name == "toupper") return args.empty() ? "" : [&]{ auto s = args[0]; for (auto& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c))); return s; }();
        if (name == "gsub" || name == "sub") {
            if (args.size() < 2) return "0";
            auto pat = args[0], repl = args[1];
            auto& str = st_.fields.empty() ? st_.record : st_.fields[0];
            int count = 0;
            regex_t regex;
            if (regcomp(&regex, pat.c_str(), REG_EXTENDED) == 0) {
                regmatch_t m;
                auto* p = str.c_str();
                std::string result;
                while (*p) {
                    if (regexec(&regex, p, 1, &m, 0) == 0 && m.rm_so >= 0) {
                        result.append(p, static_cast<std::size_t>(m.rm_so));
                        result.append(repl);
                        p += m.rm_eo;
                        ++count;
                        if (name == "sub") { result.append(p); break; }
                    } else {
                        result.append(p);
                        break;
                    }
                }
                regfree(&regex);
                str = result;
            }
            return to_string(static_cast<double>(count));
        }
        if (name == "match") {
            if (args.size() < 2) return "0";
            regex_t regex;
            if (regcomp(&regex, args[1].c_str(), REG_EXTENDED) != 0) return "0";
            regmatch_t m;
            if (regexec(&regex, args[0].c_str(), 1, &m, 0) != 0) { regfree(&regex); return "0"; }
            st_.vars["RSTART"] = to_string(static_cast<double>(m.rm_so + 1));
            st_.vars["RLENGTH"] = to_string(static_cast<double>(m.rm_eo - m.rm_so));
            regfree(&regex);
            return st_.vars["RSTART"];
        }
        if (name == "sprintf") {
            if (args.empty()) return "";
            auto fmt = args[0];
            // Simple sprintf: only %s, %d, %f
            std::string result;
            std::size_t ai = 1;
            for (std::size_t i = 0; i < fmt.size(); ++i) {
                if (fmt[i] == '%' && i + 1 < fmt.size()) {
                    ++i;
                    if (fmt[i] == 's') { result += (ai < args.size()) ? args[ai++] : ""; }
                    else if (fmt[i] == 'd') { char b[32]; std::snprintf(b, sizeof(b), "%lld", static_cast<long long>(ai < args.size() ? to_number(args[ai++]) : 0)); result += b; }
                    else if (fmt[i] == 'f') { char b[64]; std::snprintf(b, sizeof(b), "%f", ai < args.size() ? to_number(args[ai++]) : 0.0); result += b; }
                    else if (fmt[i] == '%') { result += '%'; }
                    else { result += '%'; result += fmt[i]; }
                } else {
                    result += fmt[i];
                }
            }
            return result;
        }
        if (name == "int") return to_string(std::floor(to_number(args.empty() ? "0" : args[0])));
        if (name == "sin") return to_string(std::sin(to_number(args.empty() ? "0" : args[0])));
        if (name == "cos") return to_string(std::cos(to_number(args.empty() ? "0" : args[0])));
        if (name == "sqrt") return to_string(std::sqrt(to_number(args.empty() ? "0" : args[0])));
        if (name == "rand") return to_string(static_cast<double>(std::rand()) / RAND_MAX);
        if (name == "srand") { if (!args.empty()) std::srand(static_cast<unsigned>(to_number(args[0]))); return "0"; }
        if (name == "atan2") return to_string(std::atan2(to_number(args.empty() ? "0" : args[0]), to_number(args.size() > 1 ? args[1] : "0")));
        if (name == "log") return to_string(std::log(to_number(args.empty() ? "0" : args[0])));
        if (name == "exp") return to_string(std::exp(to_number(args.empty() ? "0" : args[0])));

        // User-defined function
        auto it = st_.functions.find(name);
        if (it != st_.functions.end()) {
            auto& [params, body] = it->second;
            // Save current vars
            auto saved_vars = st_.vars;
            auto saved_arrays = st_.arrays;
            for (std::size_t i = 0; i < params.size(); ++i) {
                st_.vars[params[i]] = (i < args.size()) ? args[i] : "";
            }
            exec_block(body);
            auto result = st_.vars[name]; // convention: function name holds return value
            st_.vars = std::move(saved_vars);
            st_.arrays = std::move(saved_arrays);
            return result;
        }
        return "";
    }

    auto exec_block(NodePtr block) -> void {
        if (!block) return;
        for (auto& stmt : block->children) {
            if (st_.should_exit || st_.should_next) return;
            exec_stmt(stmt);
        }
    }

    auto exec_stmt(NodePtr node) -> void {
        if (!node || st_.should_exit || st_.should_next) return;
        switch (node->type) {
            case NodeType::Print: exec_print(node); break;
            case NodeType::Printf: exec_printf(node); break;
            case NodeType::If: exec_if(node); break;
            case NodeType::While: exec_while(node); break;
            case NodeType::For: exec_for(node); break;
            case NodeType::ForIn: exec_for_in(node); break;
            case NodeType::Block: exec_block(node); break;
            case NodeType::Return: st_.should_exit = true; break;
            case NodeType::Exit: st_.should_exit = true;
                st_.exit_code = node->children.empty() ? 0 : static_cast<int>(to_number(eval(node->children[0])));
                break;
            case NodeType::NextStmt: st_.should_next = true; break;
            case NodeType::Delete:
                if (!node->children.empty() && node->children[0]->type == NodeType::ArrayAccess) {
                    auto& arr = st_.arrays[node->children[0]->value];
                    arr.erase(eval(node->children[0]->children[0]));
                }
                break;
            default: eval(node); break;
        }
    }

    auto exec_print(NodePtr node) -> void {
        std::string out;
        if (node->children.empty()) {
            out = st_.record;
        } else {
            auto ofs = st_.vars.count("OFS") ? st_.vars["OFS"] : st_.ofs;
            auto last_data_idx = node->children.size();
            // Check if last child is redirect target
            for (std::size_t i = 0; i < last_data_idx; ++i) {
                if (i > 0) out += ofs;
                out += eval(node->children[i]);
            }
        }
        auto ors = st_.vars.count("ORS") ? st_.vars["ORS"] : st_.ors;
        std::fputs(out.c_str(), stdout);
        std::fputs(ors.c_str(), stdout);
    }

    auto exec_printf(NodePtr node) -> void {
        // Build a sprintf call node using printf's children
        auto sprintf_node = make_node(NodeType::FuncCall, "sprintf");
        sprintf_node->children = node->children;
        auto result = eval_func_call(sprintf_node);
        std::fputs(result.c_str(), stdout);
    }

    auto exec_if(NodePtr node) -> void {
        if (is_true(eval(node->children[0]))) {
            exec_stmt(node->children[1]);
        } else if (node->children.size() > 2) {
            exec_stmt(node->children[2]);
        }
    }

    auto exec_while(NodePtr node) -> void {
        while (is_true(eval(node->children[0])) && !st_.should_exit && !st_.should_next) {
            exec_stmt(node->children[1]);
        }
    }

    auto exec_for(NodePtr node) -> void {
        // children: [init, cond, update, body]
        if (node->children.size() >= 4) {
            eval(node->children[0]); // init
            while (is_true(eval(node->children[1])) && !st_.should_exit && !st_.should_next) {
                exec_stmt(node->children[3]); // body
                if (st_.should_exit || st_.should_next) break;
                eval(node->children[2]); // update
            }
        }
    }

    auto exec_for_in(NodePtr node) -> void {
        auto var_name = node->children[0]->value;
        auto arr_name = node->children[1]->value;
        auto it = st_.arrays.find(arr_name);
        if (it != st_.arrays.end()) {
            for (auto& [key, _] : it->second) {
                st_.vars[var_name] = key;
                exec_stmt(node->children[2]);
                if (st_.should_exit || st_.should_next) break;
            }
        }
    }

    auto process_file(const std::string& path, NodePtr prog) -> void {
        FILE* f = std::fopen(path.c_str(), "r");
        if (!f) { std::fprintf(stderr, "cfbox awk: cannot open '%s'\n", path.c_str()); return; }
        st_.filename = path;
        char line[65536];
        while (std::fgets(line, sizeof(line), f)) {
            std::string rec(line);
            if (!rec.empty() && rec.back() == '\n') rec.pop_back();
            process_record(rec, prog);
            if (st_.should_exit) break;
        }
        std::fclose(f);
    }

    auto process_stdin(NodePtr prog) -> void {
        st_.filename = "";
        char line[65536];
        while (std::fgets(line, sizeof(line), stdin)) {
            std::string rec(line);
            if (!rec.empty() && rec.back() == '\n') rec.pop_back();
            process_record(rec, prog);
            if (st_.should_exit) break;
        }
    }

    auto process_record(const std::string& record, NodePtr prog) -> void {
        st_.record = record;
        st_.nr++;
        st_.vars["NR"] = std::to_string(st_.nr);
        st_.vars["FNR"] = std::to_string(st_.nr);

        // Split into fields
        auto fs = st_.vars.count("FS") ? st_.vars["FS"] : st_.fs;
        st_.fields.clear();
        if (fs.size() == 1) {
            std::string field;
            for (char c : record) {
                if (c == fs[0]) { st_.fields.push_back(std::move(field)); field.clear(); }
                else field += c;
            }
            st_.fields.push_back(std::move(field));
        } else {
            st_.fields.push_back(record);
        }
        st_.vars["NF"] = std::to_string(st_.fields.size());

        // Apply rules
        for (auto& rule : prog->children) {
            if (st_.should_exit) break;
            if (rule->type != NodeType::Rule) continue;
            if (rule->children.empty()) continue;
            st_.should_next = false;

            bool match = true;
            if (rule->children[0]->type == NodeType::Ident) {
                auto& name = rule->children[0]->value;
                if (name == "BEGIN" || name == "END") continue;
            } else if (rule->children[0]->type != NodeType::Block) {
                // Has an explicit pattern expression
                match = is_true(eval(rule->children[0]));
            }
            // If children[0] is a Block (no pattern), always match

            if (match && rule->children.size() > 1) {
                exec_block(rule->children[1]);
            } else if (match && rule->children.size() == 1 && rule->children[0]->type == NodeType::Block) {
                // Pattern-only rule with no pattern, just action block
                exec_block(rule->children[0]);
            } else if (match && rule->children.size() == 1) {
                // Pattern only, default action: print
                std::puts(record.c_str());
            }
        }
    }
};

auto awk_execute(NodePtr ast, AwkState& state, const std::vector<std::string>& files) -> int {
    Executor exec(state);
    return exec.run(ast, files);
}

} // namespace cfbox::awk
