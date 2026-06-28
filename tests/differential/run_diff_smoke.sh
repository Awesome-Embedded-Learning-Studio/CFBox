#!/usr/bin/env bash
# 对照冒烟测试：把 cfbox 和系统 coreutils 在 5 个流式命令上比"标准输出 + 退出码"。
# 标签：MATCH(全一致) / ACCEPTABLE(已登记、可接受的差异) / DEFECT(已登记、待修的缺陷)
#       / NEW_DIFF(没见过的差异 → 脚本报错退出，提醒 triage)。
# 详见 document/ai/COVERAGE.md。本期只本地跑；接入 CI 留给后续批次。
#
# 判定口径：标准输出逐字节一致 AND 退出码相等 → MATCH。
# 错误输出(stderr)只展示、不参与判定——cfbox 报错是 "cfbox <cmd>:" 前缀，coreutils 是 "<cmd>:"，
# 报错措辞本就各写各的，逐字节比会是纯噪声。stderr 严格化是以后的事。
set -u

script_dir="$(cd "$(dirname "$0")" && pwd)"
repo="$(cd "$script_dir/../.." && pwd)"
CFBOX="${CFBOX:-$repo/build/cfbox}"

export LC_ALL=C   # 排除 locale 排序差异，双方都按字节序

if [[ -t 1 ]]; then
    C_RED=$'\033[31m'; C_YEL=$'\033[33m'; C_GRN=$'\033[32m'; C_RST=$'\033[0m'
else
    C_RED=''; C_YEL=''; C_GRN=''; C_RST=''
fi

if [[ ! -x "$CFBOX" ]]; then
    echo "ERROR: cfbox 不在 $CFBOX（设 CFBOX 环境变量，或先 cmake --build build）" >&2
    exit 2
fi

# 解析 oracle 绝对路径（type -P 绕开 alias/函数，例如本机 cat 被别名成 bat）
declare -A ORA
for app in cat head tail wc sort; do
    p="$(type -P "$app" 2>/dev/null || true)"
    if [[ -z "$p" ]]; then
        echo "ERROR: 系统 PATH 里找不到 $app" >&2; exit 2
    fi
    ORA[$app]="$p"
done

# 固定样本：每次现造在临时目录，干净、可复现、不提交二进制
tmp="$(mktemp -d)"; trap 'rm -rf "$tmp"' EXIT
:                                          > "$tmp/empty"
printf 'a\nbb\nccc\ndd\ne\n'               > "$tmp/small"          # 5 行
printf '2 b\n2 a\n1 x\n2 c\n10 z\n9 y\n'   > "$tmp/numericties"    # 含数值并列(2 出现三次)，专测 sort 稳定性
printf 'c\na\nb\na\nc\nb\n'                > "$tmp/dups"           # 含重复，测 sort -u
printf 'line1\nline2\nline3'               > "$tmp/notrailnl"      # 结尾无换行
printf 'ab\x00cd\nef\n'                    > "$tmp/binary"         # 含 NUL 字节

# known_diffs：每行 "STATUS applet case_id"，STATUS ∈ ACCEPTABLE|DEFECT（# 开头是注释）
declare -A KNOWN_STATUS
load_known() {
    local f="$script_dir/known_diffs" st ap cid
    [[ -r "$f" ]] || return 0
    while read -r st ap cid; do
        [[ -z "$st" || "$st" == \#* ]] && continue
        KNOWN_STATUS["$ap|$cid"]="$st"
    done < "$f"
}
load_known

n_match=0; n_acceptable=0; n_defect=0; n_new=0
new_cases=()

# compare <applet> <case_id> <fixture> <arg...>
compare() {
    local applet="$1" cid="$2" fixture="$3"; shift 3
    local -a args=("$@")

    local cf_out="$tmp/cf.out" cf_err="$tmp/cf.err" or_out="$tmp/or.out" or_err="$tmp/or.err"
    local cf_rc=0 or_rc=0
    "$CFBOX" "$applet" "${args[@]}" < "$fixture" > "$cf_out" 2> "$cf_err" || cf_rc=$?
    "${ORA[$applet]}" "${args[@]}" < "$fixture" > "$or_out" 2> "$or_err" || or_rc=$?

    local stdout_match=1 rc_match=1
    cmp -s "$cf_out" "$or_out" || stdout_match=0
    [[ "$cf_rc" == "$or_rc" ]] || rc_match=0

    local status
    if (( stdout_match && rc_match )); then
        status=MATCH
    else
        local key="$applet|$cid"
        if [[ -n "${KNOWN_STATUS[$key]:-}" ]]; then
            status="${KNOWN_STATUS[$key]}"
        else
            status=NEW_DIFF
        fi
    fi

    local tag
    case "$status" in
        MATCH)      n_match=$((n_match+1));        tag="${C_GRN}MATCH${C_RST}" ;;
        ACCEPTABLE) n_acceptable=$((n_acceptable+1)); tag="${C_YEL}ACCEPTABLE${C_RST}" ;;
        DEFECT)     n_defect=$((n_defect+1));      tag="${C_RED}DEFECT${C_RST}" ;;
        NEW_DIFF)   n_new=$((n_new+1));            new_cases+=("$applet|$cid"); tag="${C_RED}NEW_DIFF${C_RST}" ;;
    esac

    printf '%s  %-6s %-16s args=[%s]\n' "$tag" "$applet" "$cid" "${args[*]}"
    if [[ "$status" != MATCH ]]; then
        local sout="same" srct="same"
        (( stdout_match )) || sout="DIFF"
        (( rc_match ))     || srct="DIFF"
        printf '        rc: cfbox=%s oracle=%s [%s] | stdout [%s]\n' "$cf_rc" "$or_rc" "$srct" "$sout"
        if (( ! stdout_match )); then
            printf '        --- cfbox stdout ---\n'
            sed 's/^/        | /' "$cf_out" 2>/dev/null | head -8
            printf '        --- oracle stdout ---\n'
            sed 's/^/        | /' "$or_out" 2>/dev/null | head -8
        fi
        printf '\n'
    fi
}

echo "cfbox : $CFBOX"
echo "oracle: cat=$(command -v cat 2>/dev/null)  (脚本内用 type -P 绕开别名)"
echo

echo "=== cat ==="
compare cat basic     "$tmp/empty"
compare cat small     "$tmp/small"
compare cat n_small   "$tmp/small"      -n
compare cat b_small   "$tmp/small"      -b
compare cat A_small   "$tmp/small"      -A
compare cat binary    "$tmp/binary"
compare cat notrail   "$tmp/notrailnl"

echo "=== head ==="
compare head n2_small   "$tmp/small"        -n 2
compare head n100_small "$tmp/small"        -n 100
compare head c3_small   "$tmp/small"        -c 3
compare head notrail    "$tmp/notrailnl"
compare head n2_num     "$tmp/numericties"  -n 2

echo "=== tail ==="
compare tail n2_small   "$tmp/small"        -n 2
compare tail n2_notrail "$tmp/notrailnl"    -n 2
compare tail c3_small   "$tmp/small"        -c 3
compare tail n2_num     "$tmp/numericties"  -n 2

echo "=== wc ==="
compare wc small       "$tmp/small"
compare wc l_small     "$tmp/small"   -l
compare wc w_small     "$tmp/small"   -w
compare wc c_small     "$tmp/small"   -c
compare wc m_small     "$tmp/small"   -m
compare wc empty       "$tmp/empty"
compare wc c_binary    "$tmp/binary"  -c

echo "=== sort ==="
compare sort small    "$tmp/small"
compare sort n_num    "$tmp/numericties" -n
compare sort r_small  "$tmp/small"       -r
compare sort rn_num   "$tmp/numericties" -rn
compare sort u_dups   "$tmp/dups"        -u

echo
echo "================================================================"
printf '汇总: MATCH=%s  ACCEPTABLE=%s  DEFECT=%s  NEW_DIFF=%s\n' \
    "$n_match" "$n_acceptable" "$n_defect" "$n_new"
if (( n_new > 0 )); then
    echo "${C_RED}出现未登记的新差异（见上 NEW_DIFF）：${C_RST}"
    printf '  - %s\n' "${new_cases[@]}"
    echo "请 triage 后写进 known_diffs（ACCEPTABLE 或 DEFECT）。"
    exit 1
fi
echo "${C_GRN}无新差异（全部一致，或差异已登记进 known_diffs）。${C_RST}"
exit 0
