#!/usr/bin/env bash
# 端到端 wall-clock:cfbox vs 系统 coreutils 在大输入上的耗时对比。
# 补充进程内微基准(cfbox_bench)——这个看真实用户体验的 wall-clock。
# advisory 信号(bench 噪声真实,CI 机器方差大);当信号看,不当硬门。见 document/ai/PERFORMANCE.md。
set -u

script_dir="$(cd "$(dirname "$0")" && pwd)"
repo="$(cd "$script_dir/../.." && pwd)"
# 优先用 Release 构建的 cfbox(公平计时);没有就退回 Debug。
CFBOX="${CFBOX:-$repo/build-bench/cfbox}"
[[ -x "$CFBOX" ]] || CFBOX="$repo/build/cfbox"
if [[ ! -x "$CFBOX" ]]; then echo "ERROR: 找不到 cfbox 二进制(设 CFBOX 或先构建)" >&2; exit 2; fi

export LC_ALL=C
tmp="$(mktemp -d)"; trap 'rm -rf "$tmp"' EXIT
seq 1 200000 | awk '{print "line "$1" foo bar baz"}' > "$tmp/big"   # 20 万行
head -c 50000000 /dev/urandom > "$tmp/big50"                          # 50 MB 二进制
cp "$tmp/big50" "$tmp/big50b"

# 跑一次命令,打印耗时(毫秒)
ms() { local s e; s=$(date +%s%N); "$@" >/dev/null 2>&1; e=$(date +%s%N); echo $(( (e - s) / 1000000 )); }
# 取 3 次的中位数
med3() { local a b c; a=$(ms "$@"); b=$(ms "$@"); c=$(ms "$@"); printf '%s\n' "$a" "$b" "$c" | sort -n | sed -n 2p; }
# run <applet> <args...>:比 cfbox <applet> <args> vs /usr/bin/<applet> <args>(type -P 绕开别名)
run() {
    local applet=$1; shift
    local ora; ora=$(type -P "$applet")
    local cf or
    cf=$(med3 "$CFBOX" "$applet" "$@")
    or=$(med3 "$ora" "$@")
    printf '  %-8s cfbox=%6sms  core=%6sms  ratio=%5.2fx\n' "$applet" "$cf" "$or" "$(awk -v c="$cf" -v o="$or" 'BEGIN{print (o>0)? c/o : 0}')"
}

echo "cfbox : $CFBOX"
echo "input : $(wc -l < "$tmp/big") 行文本 + 50MB 二进制"
echo "=== 文本处理(grep/wc/sort/cut)==="
run grep foo "$tmp/big"
run grep -c foo "$tmp/big"
run wc "$tmp/big"
run sort "$tmp/big"
run cut -c1-10 "$tmp/big"
echo "=== 大文件二进制(md5sum/cmp)==="
run md5sum "$tmp/big50"
run cmp "$tmp/big50" "$tmp/big50b"
echo "(ratio < 1.0 = cfbox 比 coreutils 快;> 1.0 = 慢)"
