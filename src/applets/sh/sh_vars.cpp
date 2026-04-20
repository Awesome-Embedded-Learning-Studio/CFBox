#include "sh.hpp"

#include <cstdlib>
#include <unistd.h>

namespace cfbox::sh {

auto ShellState::get_var(std::string_view name) const -> std::string {
    // Special parameters
    if (name == "?") return std::to_string(last_status_);
    if (name == "$") return std::to_string(shell_pid());
    if (name == "!") return "0"; // TODO: track last background PID
    if (name == "#") return std::to_string(positional_.size());
    if (name == "@") {
        std::string result;
        for (std::size_t i = 0; i < positional_.size(); ++i) {
            if (i > 0) result += ' ';
            result += positional_[i];
        }
        return result;
    }
    if (name == "*") {
        std::string result;
        for (std::size_t i = 0; i < positional_.size(); ++i) {
            if (i > 0) result += ' ';
            result += positional_[i];
        }
        return result;
    }
    if (name.size() == 1 && name[0] >= '0' && name[0] <= '9') {
        int n = name[0] - '0';
        if (n == 0) return script_name_;
        if (static_cast<std::size_t>(n - 1) < positional_.size()) return positional_[n - 1];
        return "";
    }

    auto it = vars_.find(std::string{name});
    if (it != vars_.end()) return it->second;

    // Fall back to environment
    const char* env = std::getenv(std::string{name}.c_str());
    return env ? std::string{env} : "";
}

auto ShellState::set_var(const std::string& name, const std::string& value) -> void {
    vars_[name] = value;
    if (exported_.count(name)) {
        ::setenv(name.c_str(), value.c_str(), 1);
    }
}

auto ShellState::unset_var(const std::string& name) -> void {
    vars_.erase(name);
    exported_.erase(name);
    ::unsetenv(name.c_str());
}

auto ShellState::export_var(const std::string& name) -> void {
    exported_.insert(name);
    auto it = vars_.find(name);
    if (it != vars_.end()) {
        ::setenv(name.c_str(), it->second.c_str(), 1);
    }
}

auto ShellState::is_exported(const std::string& name) const -> bool {
    return exported_.count(name) > 0;
}

auto ShellState::set_positional(std::vector<std::string> args) -> void {
    positional_ = std::move(args);
}

auto ShellState::shift(int n) -> void {
    if (n < 0) n = 0;
    if (n >= static_cast<int>(positional_.size())) {
        positional_.clear();
    } else {
        positional_.erase(positional_.begin(), positional_.begin() + n);
    }
}

auto ShellState::shell_pid() const -> int {
    return static_cast<int>(::getpid());
}

} // namespace cfbox::sh
