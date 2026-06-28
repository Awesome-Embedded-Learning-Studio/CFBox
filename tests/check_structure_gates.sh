#!/usr/bin/env bash
# tests/check_structure_gates.sh — 结构机械护栏（见 document/ai/STRUCTURE-TASTE.md §2）。
# 硬门（CI exit 非零）：
#   门 1 banned-pattern —— 精确匹配"调用"（带括号），不误伤注释/标识符（如 awk 的 sprintf 函数名）。
#     a) 不安全 C 函数 sprintf(/strcpy(/strcat(/gets( —— 用 snprintf/std::string/string_view。
#     b) std::stoi(/std::stol( —— 项目 -fno-exceptions，非法输入会抛→terminate；用 cfbox::args::parse_int。
#     c) applets 里的裸 std::fopen —— 用 cfbox::io::open_file（io.hpp 本身是允许的封装层，不在 src/applets 下）。
#   门 2 layering —— applets 不自造递归目录遍历，用 cfbox::fs::for_each_entry 或加注释豁免。
#     现有 5 个 grandfather：需求（depth/filter/per-entry-error/相对路径）超出 for_each_entry 能力。
set -u

script_dir="$(cd "$(dirname "$0")" && pwd)"
repo="$(cd "$script_dir/.." && pwd)"
cd "$repo"

fail=0
report() { sed 's/^/    /'; }

echo "=== 门 1：banned-pattern ==="

n=$(grep -rnE '\b(sprintf|strcpy|strcat|gets)\(' src/ include/ 2>/dev/null | wc -l | tr -d ' ')
echo "  不安全 C 函数调用(sprintf/strcpy/strcat/gets): $n"
if (( n > 0 )); then grep -rnE '\b(sprintf|strcpy|strcat|gets)\(' src/ include/ 2>/dev/null | report; fail=1; fi

n=$(grep -rnE 'std::sto[il]\(' src/ 2>/dev/null | wc -l | tr -d ' ')
echo "  std::stoi/stol 调用: $n"
if (( n > 0 )); then grep -rnE 'std::sto[il]\(' src/ 2>/dev/null | report; fail=1; fi

n=$(grep -rn 'std::fopen' src/applets/ 2>/dev/null | wc -l | tr -d ' ')
echo "  applets 裸 std::fopen: $n"
if (( n > 0 )); then grep -rn 'std::fopen' src/applets/ 2>/dev/null | report; fail=1; fi

echo "=== 门 2：layering（applets 不自造递归遍历）==="

# grandfather：for_each_entry 覆盖不了的合理用例
allow='src/applets/du.cpp src/applets/find.cpp src/applets/tar.cpp src/applets/sysctl.cpp src/applets/grep.cpp'
new_layer=0
while IFS= read -r f; do
    [[ -z "$f" ]] && continue
    ok=0
    for g in $allow; do [[ "$f" == "$g" ]] && ok=1 && break; done
    if (( ! ok )); then
        echo "  FAIL: $f 直接用 recursive_directory_iterator —— 用 cfbox::fs::for_each_entry 或加注释豁免"
        new_layer=$((new_layer + 1))
        fail=1
    fi
done < <(grep -rln 'recursive_directory_iterator' src/applets/ 2>/dev/null)
echo "  grandfather(允许): $(echo $allow | wc -w | tr -d ' ') 个；新增裸奔: $new_layer"

echo "==="
if (( fail )); then echo "结构机械护栏：FAIL"; exit 1; fi
echo "结构机械护栏：PASS"
exit 0
