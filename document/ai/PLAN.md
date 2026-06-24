# CFBox — 当前焦点（批级进度）

> Tier 3（批级，易变）。单一事实源（批级）。全树见 [ROADMAP.md](ROADMAP.md)，铁律见 [DIRECTIVES.md](DIRECTIVES.md)。
> **Phase 1.5 代码质量审查 ✅ 完成**（体积 -14%、消 iostream/stoi、统一错误宏、fs 封装扩展，379 测试全绿）。
> **v0.3.0 已发布**：L2 rootfs 启动骨架（init/mount/mdev/umount/swapoff/reboot/poweroff，117→123 applet）+ tail -f —— cfbox 在 i.MX6ULL 上作为 PID 1 替代 BusyBox。基线 399 测试 / 418 KB / 123 applet。
> 焦点 → Phase 2 批2 `cp -a`（归档模式：保权限/属主/时间戳/symlink/递归）。
> 状态：✅ DONE / 🔄 NEXT / ⏳ PENDING / ⛔ BLOCKED。每批≈一 commit，完成门 `cmake --build build -j$(nproc) && ctest --test-dir build --output-on-failure` 全绿 + `bash tests/integration/run_all.sh`。

## ✅ Phase 1.5（代码质量审查）已完成 — 2026-05-26

| 批 | 范围 | 状态 | Commit | 测试 |
|----|------|------|--------|------|
| A-G 扫描 | 七维度质量审查（架构/体积/安全/鲁棒性/工具链/测试/注释） | ✅ | (见 changelogs/v0.2.0.md) | 379/0 |
| 收尾 | 体积 -14%、消 iostream/stoi、统一错误宏、fs 封装扩展 | ✅ | a96ec85(merge) | 379/0 |

## 🔄 Phase 2（核心命令深化）— 进行中

> 目标：让高频核心命令的功能深度到位（场景闭环优先于 applet 数量）。每批≈一 commit + 完成门。批级 propose/脚手架用 `/next`，验证收尾用 `/done`。

| 批 | 范围 | 状态 | Commit | 测试 |
|----|------|------|--------|------|
| 批1 | `tail -f/-F`（fd-based follow：fstat 轮询 + 64KiB quantum + -F drain-switch + SIGINT 退出 0） | ✅ | bff34e9 | 381/0 |
| 批2 | `cp -a`（归档模式：保权限/属主/时间戳/symlink/递归） | 🔄 NEXT | — | — |
| 批3 | `test` POSIX 子集（文件测试/字符串/整数/复合表达式，退出码语义） | ⏳ | — | — |
| 批4 | `ls -R` 递归 + `--color`（LS_COLORS 感知、递归缩进） | ⏳ | — | — |
| 批5+ | grep -A/-B/-C、find 布尔表达式、sh 深化（按运维频率排） | ⏳ | — | — |

> 各批细节（触及文件、Result 签名草案、完成门、gotcha）由 `/next <批>` 现场产出脚手架，确认后写入本表 commit/测试列。

## ✅ L2 rootfs 启动骨架（v0.3.0，插队完成）— 已合并 PR #13（2026-06）

> 场景驱动插队（非 Phase 2 原计划）：让 cfbox 在 i.MX6ULL 上替代 BusyBox 当 PID 1，补齐启动闭环全部 applet 缺口。端到端验证：cfbox 当 PID 1 跑 imx-forge rootfs，启动到 console。

| 批 | 范围 | 状态 | Commit |
|----|------|------|--------|
| init askfirst | `init` 支持 `askfirst`（console getty 等回车激活） | ✅ | 4b95168 |
| mount | `mount -a/-t/-o`，读 fstab | ✅ | 65e1f17 |
| mdev | `mdev -s` 冷启动扫 /sys 建 /dev 节点 | ✅ | 6ee0770 |
| 关机集 | `umount -a -r` / `swapoff -a` / `reboot` / `poweroff` | ✅ | de78018 |
| 串口修复 | sh cooked tty + `VERASE`、`ls -l` owner/group、default `PATH` | ✅ | 8045c11 / a803b99 |
| CI 兼容 | gcc-13（unused-result + 显式 `<algorithm>`） | ✅ | d307998 / dc25a09 |
| 展示 | README i.MX6ULL 端到端 + size-table 生成器 | ✅ | 8c45c77 |

> 新增 6 applet（mount/mdev/umount/swapoff/reboot/poweroff），117→123。完整故事见 [changelogs/v0.3.0.md](../../changelogs/v0.3.0.md)。

## OPEN GOTCHAS（跨批陷阱，改前必看）

1. **APPLET_REGISTRY + CMake 开关双改**：新增/裁剪 applet 必须**同时**改 [include/cfbox/applets.hpp](../../include/cfbox/applets.hpp) 注册表与 CMake 的 `CFBOX_ENABLE_*`（生成于 [applet_config.hpp.in](../../include/cfbox/applet_config.hpp.in)）；漏一处 → 编译期静默跳过或链接错。
2. **交叉编译盲区**：本地 `build/`（x86-64 + ASan）抓不到 aarch64/armhf；改公共头/分发逻辑/ABI 后，push 前留意 CI 的 cross-compile + qemu-user/system 阶段是否绿（[document/ci.md](../ci.md)）。
3. **体积回归**：新增 `<filesystem>`/iostream/include 膨胀会撑大 size-opt 二进制（预算 ≤ 550 KB，当前 418 KB）；批量改动后跑 `cmake -B build-size -DCMAKE_BUILD_TYPE=Release -DCFBOX_OPTIMIZE_FOR_SIZE=ON && cmake --build build-size -j$(nproc)` + strip 核查。
4. **TOCTOU / symlink**：cp/rm/mv/chmod 的 check-then-act 与 `-R` 跟随 symlink 是安全高危（质量扫描 C 维度）；改这些 applet 先看既有防御。
5. **流式 vs 全量**：大文件优先 `cfbox::io::for_each_line()`；滥用 `read_all()` 会内存爆炸（grep/cat/wc 已流式化，参考）。
6. **multi-call binary 全局状态**：装 signal handler / 改进程全局状态的 applet 必须 RAII 恢复（如 tail -f 的 sigaction guard），否则污染同进程后续 applet 调用。
7. **底层 fd 操作不走 fs_util**：follow/字节流消费等需 `fstat`/`lseek`/`read` 的场景用 `<sys/stat.h>` raw POSIX（fs_util::status 拉 `std::filesystem` 撑体积）；公共 fs 封装是高层路径操作。

## 回到仓库
`/resume`（读本文件 + `git log --oneline -15`）。Codex 等价粘贴 prompt 见 [prompts.md](prompts.md)。
