# 2026-06-28 — cp -a 归档模式（Phase 2 批2）

## 背景
coreutils `cp -a`（archive）= `-rd --preserve=all`：递归 + 保 mode/owner/time + 复制 symlink 本身（不跟随）。顺带修复既有 `-p` 名实不符（实现只保 mode，help 却声称保 mode/owner/time）。

## 设计决策
- **lstat 驱动遍历，绝不跟随 symlink**：弃 `std::filesystem::copy`（默认跟随 symlink + 不保属性），自写 `copy_one_archive` 按 `S_ISLNK/S_ISDIR` 分发。这是 TOCTOU/symlink 高危面（PLAN GOTCHA #4）。
- **symlink 复制**：`read_symlink` + `create_symlink` 复制链接本身；owner 用 `lchown`，time 用 `utimensat(AT_SYMLINK_NOFOLLOW)`——`utime(2)` 不支持 symlink。
- **目录属性回填顺序**：先 `create_directory`（默认可写权限）→ 递归子项 → **最后** `preserve_attrs` 设 mode/owner/time。否则子项创建会刷新父目录 mtime，导致 -a 保时间戳失败（只在含子项目录暴露，单测易漏）。
- **属性失败 non-fatal**：owner 在非 root 下 EPERM 常见，coreutils 也是警告不致命；仅内容复制失败计 rc=1。
- **新增 fs 原语**：`fs::lstat`（link-aware）、`fs::set_times(path, timespec[2], no_follow)`。
- **潜伏 bug 顺手修**：`fs_util.hpp` 用 `std::vector` 却从未 include `<vector>`（靠 `<filesystem>` 传递包含侥幸编译），gcc-16 收紧传递 include 后暴露。显式补 `<vector>`。

## 验证
- GTest +7（ArchiveCopyPreservesMode/Timestamp/SymlinkAsLink/BrokenSymlink/Tree/DirectoryTimestamp + PreserveKeepsTimestamp），全量 **406/0** 绿。
- 集成 test_cp.sh +`cp -a` 场景（结构/mode/symlink/broken），54 脚本全绿。
- size-opt **422 KB**（基线 418 KB，+4 KB），≤550 KB 红线。

## 陷阱（留给后续批/维护者）
- `-r`（非 `-a`）仍走 `copy_recursive`（跟随 symlink）——coreutils `-r` 本就如此，向后兼容未改；安全归档一律用 `-a`。
- `st_atim/st_mtim` 是 POSIX 2008 字段，glibc/musl 可用；BSD 系 `st_atimespec` 需条件编译——cfbox Linux-only 暂无忧，交叉编译靠 CI cross 阶段兜底。
- commit: `a3b89ed`
