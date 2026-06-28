#!/usr/bin/env bash
# tests/check_test_floor.sh — 覆盖率的"测试地板"两道硬门（见 document/ai/COVERAGE.md §4）。
#
#   门 1 测试数量下限：GTest 数、集成脚本数不得低于 tests/expected_counts.txt 的基线。
#                       防的是"有人偷偷删测试文件 / 删 TEST() 用例，CI 仍然全绿"。
#   门 2 新 applet 必须带测试：APPLET_REGISTRY 里每个命令都要有 test_<name>.cpp 或 test_<name>.sh；
#                       没有的，必须在 tests/untested_applets.txt 里登记（老人老办法）。
#                       新加的、既没测试又没登记的 applet → 挂。
#
# 本地、CI 都能跑。CI 挂在 native job 末尾，非零退出即红。
# 只读测试与源码，不改任何东西。
set -u

script_dir="$(cd "$(dirname "$0")" && pwd)"
repo="$(cd "$script_dir/.." && pwd)"
build_dir="${BUILD_DIR:-$repo/build}"
cd "$repo"

# 共享主命令入口的别名：不单独要求测试（归并到主命令，如 [ / pkill / poweroff）
ALIASES='[ pkill poweroff'

have_test() { [[ -f "tests/unit/test_$1.cpp" || -f "tests/integration/test_$1.sh" ]]; }
is_alias() { local x; for x in $ALIASES; do [[ "$1" == "$x" ]] && return 0; done; return 1; }

fail=0

# ── 门 1：测试数量下限 ──────────────────────────────────────────
echo "=== 门 1：测试数量下限（不得低于基线 tests/expected_counts.txt）==="

expected_gtest=""; expected_int=""
if [[ -r tests/expected_counts.txt ]]; then
    while read -r k v; do
        [[ -z "$k" || "$k" == \#* ]] && continue
        case "$k" in gtest) expected_gtest=$v ;; integration) expected_int=$v ;; esac
    done < tests/expected_counts.txt
fi

cur_gtest=""
if ctest --test-dir "$build_dir" -N >/dev/null 2>&1; then
    cur_gtest=$(ctest --test-dir "$build_dir" -N 2>/dev/null | awk '/Total Tests:/ {print $3; exit}')
fi
cur_int=$(ls tests/integration/test_*.sh 2>/dev/null | wc -l | tr -d ' ')

if [[ -z "$cur_gtest" ]]; then
    echo "  WARN: $build_dir 没配置或没有测试二进制，跳过 GTest 计数门（先 cmake --build $build_dir）"
else
    echo "  GTest      : 当前=$cur_gtest  基线=${expected_gtest:-?}"
    if [[ -n "$expected_gtest" && "$cur_gtest" -lt "$expected_gtest" ]]; then
        echo "  FAIL: GTest 数 $cur_gtest < 基线 $expected_gtest（有人删测试了？）"; fail=1
    fi
fi
echo "  集成脚本   : 当前=$cur_int    基线=${expected_int:-?}"
if [[ -n "$expected_int" && "$cur_int" -lt "$expected_int" ]]; then
    echo "  FAIL: 集成脚本数 $cur_int < 基线 $expected_int（有人删脚本了？）"; fail=1
fi

# ── 门 2：新 applet 必须带测试 ──────────────────────────────────
echo "=== 门 2：新 applet 必须带测试（无测试的须登记在 tests/untested_applets.txt）==="

mapfile -t applets < <(grep -oE '\{"[a-z0-9[]+' include/cfbox/applets.hpp | cut -c3- | sort -u)
declare -A grandfathered=()
if [[ -r tests/untested_applets.txt ]]; then
    while read -r line; do
        [[ -z "$line" || "$line" == \#* ]] && continue
        grandfathered["$line"]=1
    done < tests/untested_applets.txt
fi

new_bare=0; stale=0
for a in "${applets[@]}"; do
    is_alias "$a" && continue
    if have_test "$a"; then
        if [[ -n "${grandfathered[$a]:-}" ]]; then
            echo "  过期: $a 现在已有测试，建议从 untested_applets.txt 删掉该行"; stale=$((stale+1))
        fi
    else
        if [[ -z "${grandfathered[$a]:-}" ]]; then
            echo "  FAIL: $a 既无 test_<name>.cpp/.sh，也没登记在 untested_applets.txt（新 applet 必须带测试）"
            new_bare=$((new_bare+1)); fail=1
        fi
    fi
done
echo "  applet 名总数(含别名)=${#applets[@]}; 已登记未测=${#grandfathered[@]}; 过期登记=$stale; 裸奔新增=$new_bare"

echo "==="
if (( fail )); then echo "测试地板门：FAIL"; exit 1; fi
echo "测试地板门：PASS"
exit 0
