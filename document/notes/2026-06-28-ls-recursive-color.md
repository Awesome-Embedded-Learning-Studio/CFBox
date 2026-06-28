# 2026-06-28 — ls -R 递归 + --color（Phase 2 批4）

## 背景
ls 仅支持 -a/-l/-h，无递归、无着色（全仓零 ANSI）。顺带修一个潜伏格式 bug。

## 设计决策
- **-R 递归 DFS**（`list_recursive`）：GNU 风格分块——每个目录 `path:` 头 + 内容，块间空行。**不跟随 symlink 目录**（symlink_status 判定，规避环形 fs 无限递归）。
- **`--color[=always|auto|never]`**：args.hpp **不注册** --color（避免 has_value 贪婪吃下一个 positional 当值），手动用 `has_long`/`get_long` 解析；bare `--color`=always。auto 由 `isatty(STDOUT)` 控制。
- **固定 type→色映射**（dir=01;34 蓝 / exec=01;32 绿 / symlink=01;36 青 / fifo / socket / 设备=黄系）。LS_COLORS glob 解析**延后**（体积红线 + 收益低）。
- **提取 `print_entry`/`collect_visible`**：消除原 list_directory 与 list_path 单文件的长格式重复（DRY）。
- **修潜伏 bug**：`format_permissions` 返回 10 字符（含前导占位 `-`），调用方又 insert type_char → `ls -l` 输出 `d-rwxr-xr-x`（多一 `-`）。改为返回 9 字符、type_char 由调用方前缀 → `drwxr-xr-x`（coreutils 兼容）。

## 验证
- GTest +7（RecursiveListsNestedDirs/DoesNotFollowSymlinkDir/PlusLong、ColorAlways/AutoOff/Never/InvalidValue），全量 **424/0**。
- 集成 test_ls.sh +3（-R 递归、--color=always 出 ANSI、--color=auto 管道无 ANSI），10 passed。
- size-opt **422 KB**（持平）。

## 陷阱
- `capture_stdout`（test_capture.hpp）用 dup2 重定向到文件 → isatty=0 → auto 自动关色，既有 6 GTest 零回归；着色用例一律 `--color=always`。
- --color 不注册到 args specs 是刻意为之；若将来 args.hpp 支持长选项可选值，可改回注册。
- LS_COLORS 解析未做，--color 仅 type-based 固定色；用户自定义 `*.ext` 着色暂不支持（后置）。
- commit: `6dfe329`
