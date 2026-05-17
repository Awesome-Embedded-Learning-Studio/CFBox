# Phase 0C: 体积、Profile 与裁剪预算

## 概述

**目标**：把体积从"偶尔测一下"升级为硬门禁。CFBox 面向 initramfs、容器和嵌入式，体积预算必须按 profile、目标三元组、链接模式和 strip 状态持续追踪。

**进入条件**：Phase 0A 完成 profile/applet 清单。

**退出条件**：
- `minimal`、`rescue`、`container`、`embedded`、`full` profile 均有 applet 集合和体积预算。
- `scripts/measure_size.sh` 或新脚本输出 raw size、section size、Top symbols、动态依赖、profile 信息。
- 每个新增 applet 或基础设施都有体积预算和实际增量。
- CI 中至少保存 size report artifact。
- 超预算项必须阻断或有书面豁免。

**硬门禁**：
- profile 预算超限不得进入下一阶段。
- 新增能力没有体积预算不得合入对应 Phase。
- release 候选必须附带 size diff。

---

## Part 1: 预算口径

### 1.1 统一命名格式

统一体积报告名称：

```text
<profile>/<target>/<link-mode>/<strip-state>
```

示例：

- `full/x86_64-linux-gnu/dynamic/stripped`
- `rescue/x86_64-linux-musl/static/stripped`
- `embedded/aarch64-linux-musl/static/unstripped`

### 1.2 v1.0 预算表

v1.0 预算沿用生产标准：

| Profile | static musl stripped 目标 |
|---------|---------------------------|
| `minimal` | <= 350 KB |
| `rescue` | <= 500 KB |
| `container` | <= 650 KB |
| `embedded` | <= 800 KB |
| `full` no-TLS | <= 900 KB |
| `full` TLS | <= 1.4 MB |

### 1.3 Phase 1 临时门槛

Phase 1 的临时门槛：full profile size-opt 保持在 550 KB 以内，除非 Phase 0C 重新校准预算并说明原因。

### 1.4 Profile 迁移计划

当前 CMake profile 定义（`cmake/Config.cmake`）中仅有四个 profile：`minimal`、`embedded`、`desktop`、`full`。其中 `desktop` 与 `full` 实际相同（均启用全部 applet）。v1.0 目标需要 `minimal`、`rescue`、`container`、`embedded`、`full` 五个 profile，因此 `desktop` 必须拆分为 `rescue` 和 `container`。

#### 1.4.1 拆分方案

`desktop` 拆分为两个独立 profile：

| 新 Profile | 定位 | 核心能力 |
|------------|------|----------|
| `rescue` | initramfs 救援环境 | shell + 权限管理（chmod/chown/chgrp）+ 挂载（mount/umount/chroot）+ dd + stty + 归档（tar）+ 压缩（gzip/gunzip）+ 网络诊断（grep、find、sed） |
| `container` | 最小容器替代工具集 | 在 rescue 基础上增加：文本处理（sort、awk）+ 进程管理（ps、kill）+ 校验（sha256sum、base64）+ 辅助（xargs、which） |

#### 1.4.2 迁移步骤

1. **在 `cmake/Config.cmake` 中添加 `rescue` 和 `container` profile 定义**：
   - 在 `CFBOX_PROFILE` 的 CACHE STRING 描述中更新可选项。
   - 添加 `elseif(CFBOX_PROFILE STREQUAL "rescue")` 分支，显式启用 rescue 所需 applet，显式关闭其余 applet。
   - 添加 `elseif(CFBOX_PROFILE STREQUAL "container")` 分支，在 rescue 基础上扩展 container 额外 applet。
   - 确保 `desktop` 分支暂时保留，打印 DEPRECATION 警告并映射到 `rescue` 或 `container`。

2. **定义各 profile 的 applet 集合**：
   - 在 `document/architecture.md` 或独立 profile 文档中列出每个 profile 的完整 applet 清单。
   - 确保 CMake profile 定义与文档一致（单一事实源）。

3. **通过体积测量验证**：
   - 对 `rescue` 和 `container` 分别执行 `scripts/measure_size.sh`，记录实际体积。
   - 与 v1.0 预算表对比，确认 `rescue` <= 500 KB、`container` <= 650 KB。
   - 如超预算，调整 applet 集合或优化实现。

4. **移除或重命名 `desktop` profile**：
   - 确认无外部用户依赖 `desktop` profile 后，将其从 CMake 中移除。
   - 在 `CHANGELOG.md` 中记录 breaking change。
   - 如需过渡期，可将 `desktop` 保留为 `container` 的别名并打印 deprecation 警告。

#### 1.4.3 时间线

Profile 迁移必须在 Phase 0C 完成之前完成，因为 Phase 1 将基于新 profile 结构添加 applet。具体时间线：

| 步骤 | 完成时间 |
|------|----------|
| 添加 `rescue`/`container` CMake 定义 | Phase 0C 第一周 |
| 文档化各 profile applet 集 | Phase 0C 第一周 |
| 体积测量与预算校准 | Phase 0C 第二周 |
| 移除 `desktop` profile | Phase 0C 结束前 |

---

## Part 2: Profile 落地要求

### 2.1 总体要求

当前 CMake profile 需要补齐到路线图口径：

| Profile | 用途 | Phase 0C 要求 |
|---------|------|---------------|
| `minimal` | 极小基础工具集 | 保留最小文件/文本操作，列出不承诺能力 |
| `rescue` | initramfs 救援 | shell、权限、挂载、dd、归档、压缩、网络诊断 |
| `container` | 最小容器替代 | shell、文件、文本处理、下载、脚本兼容 |
| `embedded` | PID 1/runtime | init、挂载、日志、getty/login、网络、设备基础 |
| `full` | 开发者完整体验 | 默认稳定 applet，实验 applet 标注 |

Phase 0C 不要求一次实现所有 profile 的全部 applet，但必须明确：

- CMake profile 名称。
- 当前启用 applet 集。
- v1.0 目标 applet 集。
- 体积预算和缺口。

### 2.2 各 Profile 当前状态与 v1.0 目标对比

以下逐 profile 对比当前 CMake 实际启用的 applet 集合与 v1.0 目标集合，明确差距和迁移路径。

#### 2.2.1 `minimal` Profile

**当前 CMake applet 集**（`cmake/Config.cmake` 中 `minimal` 分支）：

```text
echo, cat, mkdir, rm, cp, mv, ls, grep
```

共 8 个 applet，其余全部显式关闭。

**v1.0 目标 applet 集**：

```text
echo, cat, mkdir, rm, cp, mv, ls, grep
```

与当前相同。`minimal` profile 的 applet 集合在 v1.0 不做扩展，保持极小体积。

**不承诺能力（文档需明确列出）**：

以下能力在 `minimal` profile 中**不提供**，用户如需这些能力应升级到 `rescue` 或更高 profile：

| 不承诺能力 | 替代 profile |
|------------|-------------|
| shell（`sh`） | `rescue` 及以上 |
| 文件查找（`find`） | `rescue` 及以上 |
| 流编辑（`sed`） | `rescue` 及以上 |
| 文本排序（`sort`） | `container` 及以上 |
| 归档（`tar`、`cpio`） | `rescue` 及以上 |
| 压缩（`gzip`） | `rescue` 及以上 |
| 进程管理（`ps`、`kill`） | `container` 及以上 |
| init（PID 1） | `embedded` 及以上 |
| 网络工具 | `embedded` 及以上 |

#### 2.2.2 `embedded` Profile

**当前 CMake applet 集**（`cmake/Config.cmake` 中 `embedded` 分支）：

```text
全部 applet，仅排除：sort, uniq, sed
```

即当前 `embedded` 启用了除 `sort`、`uniq`、`sed` 之外的所有已实现 applet。

**v1.0 目标 applet 集**：

在当前基础上新增以下 initramfs/嵌入式运行时必需 applet：

| 新增 applet | 用途 | 说明 |
|-------------|------|------|
| `init` | PID 1 进程管理 | 系统启动入口，信号处理，子进程回收 |
| `mount` | 文件系统挂载 | 挂载 proc/sys/devtmpfs 等 |
| `umount` | 文件系统卸载 | 优雅关机/重启 |
| `dd` | 块设备操作 | 镜像写入、低级拷贝 |
| `stty` | 终端设置 | 串口配置，getty 前置依赖 |
| `syslog` | 系统日志 | klogd/syslogd 功能 |
| `getty` | 终端登录管理 | 打开 tty、提示登录 |
| `login` | 用户认证 | PAM/passwd 认证 |

**注意**：以上 applet（如 `init`、`mount`、`umount`、`dd`、`stty`、`syslog`、`getty`、`login`）部分尚未在 `CFBOX_APPLETS` 列表中实现。Phase 1 需优先实现这些 applet，并在实现时更新 `embedded` profile 的 CMake 定义。

#### 2.2.3 `rescue` Profile（新增）

**当前 CMake applet 集**：不存在（`rescue` profile 尚未在 CMake 中定义）。

**v1.0 目标 applet 集**：

```text
sh, chmod, chown, chgrp, mount, umount, chroot, dd, stty,
tar, gzip, gunzip, grep, find, sed, echo, cat, ls, cp, mv,
rm, mkdir, rmdir, ln, readlink, realpath, pwd, basename,
dirname, date, sleep, true, false, test, id, whoami, hostname,
dmesg, sync
```

核心定位：initramfs 救援环境，足以完成系统修复、数据恢复、网络诊断等基本操作。

**按功能分类**：

| 功能域 | applet |
|--------|--------|
| shell | `sh` |
| 权限管理 | `chmod`, `chown`, `chgrp` |
| 挂载/切换根 | `mount`, `umount`, `chroot` |
| 块设备 | `dd` |
| 终端 | `stty` |
| 归档/压缩 | `tar`, `gzip`, `gunzip` |
| 搜索/过滤 | `grep`, `find`, `sed` |
| 基础文件操作 | `echo`, `cat`, `ls`, `cp`, `mv`, `rm`, `mkdir`, `rmdir`, `ln` |
| 路径操作 | `readlink`, `realpath`, `pwd`, `basename`, `dirname` |
| 系统/状态 | `date`, `sleep`, `true`, `false`, `test`, `id`, `whoami`, `hostname`, `dmesg`, `sync` |

#### 2.2.4 `container` Profile（新增）

**当前 CMake applet 集**：不存在（`container` profile 尚未在 CMake 中定义）。

**v1.0 目标 applet 集**：

在 `rescue` 基础上增加以下 applet：

| 额外 applet | 用途 |
|-------------|------|
| `sort` | 文本排序，脚本常用 |
| `awk` | 文本处理语言，脚本兼容 |
| `xargs` | 参数组合执行 |
| `ps` | 进程查看 |
| `kill` | 进程信号发送 |
| `sha256sum` | 文件校验 |
| `base64` | 编解码 |
| `which` | 命令查找 |

即 `container` = `rescue` 全部 applet + 以上 8 个 applet。

核心定位：最小容器替代工具集，足以运行大多数 shell 脚本，替代容器中的 coreutils/busybox。

#### 2.2.5 `full` Profile

**当前 CMake applet 集**（`cmake/Config.cmake` 中 `desktop`/`full` 分支）：

```text
全部已注册 applet（CFBOX_APPLETS 列表中约 109 个），均默认 ON
```

具体包括（来自 `cmake/Config.cmake` 中 `CFBOX_APPLETS` 列表）：

```text
echo printf cat head tail wc sort uniq
mkdir rm cp mv ls grep find sed init
true false yes pwd
basename dirname uname nproc link
hostname logname whoami tty
sleep id test
sh
printenv hostid sync usleep rmdir unlink who env
readlink realpath touch truncate stat install mktemp
ln mkfifo mknod du
seq tee tac fold expand
cut paste nl comm tr
cksum md5sum sum
date od split shuf factor
timeout nice nohup df
expr tsort
xargs
gzip gunzip diff cmp patch ed tar cpio ar unzip
awk
free uptime kill pidof ps pgrep sysctl
pwdx pstree pmap fuser iostat
watch top
dmesg hexdump more rev cal renice
```

**v1.0 目标 applet 集**：

```text
全部稳定 applet（当前 CFBOX_APPLETS 列表）+ Phase 1 新增的稳定 applet
```

`full` profile 的目标是开发者完整体验，默认启用所有稳定 applet。实验性 applet 在 `full` profile 中通过独立 CMake option 控制，默认关闭，需显式启用。

---

## Part 3: Size Report

### 3.1 脚本用法

扩展或新增脚本输出：

```bash
scripts/measure_size.sh build-release/cfbox --profile full --format json
```

### 3.2 必须包含的字段

必须包含：

1. **raw file size** — 二进制文件原始大小（字节）。
2. **`size` section breakdown** — text、data、bss 等 section 的大小。
3. **stripped/unstripped 对比** — 同时报告 strip 前后体积。
4. **`ldd` 或静态链接判断** — 标明动态/静态链接及动态库依赖列表。
5. **Top 20 symbols** — 体积最大的 20 个符号及其大小。
6. **与上一基线的增量** — raw size 和 text section 的 delta。
7. **新增/删除 applet 列表** — 与上一基线对比的 applet 变更。
8. **profile 信息** — 当前构建使用的 profile 名称、target triplet、link mode、strip state。

### 3.3 报告目录结构

建议报告位置：

```text
build/reports/size/
├── full-x86_64-linux-gnu-dynamic.json
├── rescue-x86_64-linux-musl-static.json
└── diff-summary.md
```

### 3.4 完整 JSON 输出示例

`scripts/measure_size.sh --format json` 的输出应遵循以下结构。所有数值字段以字节为单位（整数），便于程序化比较和 CI 判断。

```json
{
  "binary": "cfbox",
  "profile": "full",
  "target": "x86_64-linux-gnu",
  "link_mode": "dynamic",
  "strip_state": "stripped",
  "raw_size": 456789,
  "text_size": 380000,
  "data_size": 12000,
  "bss_size": 800,
  "top_symbols": [
    {"name": "cfbox::applet::echo_main", "size": 1234},
    {"name": "cfbox::applet::ls_main", "size": 1180},
    {"name": "cfbox::applet::grep_main", "size": 1056},
    {"name": "cfbox::applet::find_main", "size": 980},
    {"name": "cfbox::applet::awk_main", "size": 950},
    {"name": "cfbox::applet::sed_main", "size": 920},
    {"name": "cfbox::applet::tar_main", "size": 890},
    {"name": "cfbox::applet::sh_main", "size": 850},
    {"name": "cfbox::applet::ps_main", "size": 810},
    {"name": "cfbox::applet::cat_main", "size": 780},
    {"name": "cfbox::applet::cp_main", "size": 750},
    {"name": "cfbox::applet::sort_main", "size": 720},
    {"name": "cfbox::applet::dd_main", "size": 690},
    {"name": "cfbox::applet::init_main", "size": 660},
    {"name": "cfbox::applet::gzip_main", "size": 640},
    {"name": "cfbox::applet::diff_main", "size": 610},
    {"name": "cfbox::applet::patch_main", "size": 580},
    {"name": "cfbox::applet::top_main", "size": 550},
    {"name": "cfbox::applet::mount_main", "size": 520},
    {"name": "cfbox::applet::xargs_main", "size": 490}
  ],
  "dynamic_deps": [
    "libc.so.6",
    "libm.so.6"
  ],
  "baseline_delta": {
    "raw_delta": 0,
    "text_delta": 0,
    "baseline_file": "build/reports/size/full-x86_64-linux-gnu-dynamic.json.baseline"
  },
  "applet_changes": {
    "added": [],
    "removed": []
  }
}
```

**字段说明**：

| JSON 字段 | 类型 | 说明 |
|-----------|------|------|
| `binary` | string | 二进制文件名 |
| `profile` | string | 构建使用的 profile 名称 |
| `target` | string | 目标三元组（如 `x86_64-linux-gnu`） |
| `link_mode` | string | `dynamic` 或 `static` |
| `strip_state` | string | `stripped` 或 `unstripped` |
| `raw_size` | integer | 文件原始大小（字节） |
| `text_size` | integer | text section 大小（字节） |
| `data_size` | integer | data section 大小（字节） |
| `bss_size` | integer | bss section 大小（字节） |
| `top_symbols` | array | 体积最大的 20 个符号，每项含 `name` 和 `size` |
| `dynamic_deps` | array | 动态库依赖列表（静态链接时为空数组） |
| `baseline_delta` | object | 与基线的增量比较，含 `raw_delta`、`text_delta`、`baseline_file` |
| `applet_changes` | object | 与基线相比的 applet 变更，含 `added` 和 `removed` 列表 |

---

## Part 4: 每 Applet 增量

### 4.1 测量命令

新增 applet 需提供体积测量：

```bash
cmake -B build-size-base -DCMAKE_BUILD_TYPE=Release -DCFBOX_OPTIMIZE_FOR_SIZE=ON -DCFBOX_ENABLE_<APPLET>=OFF
cmake -B build-size-new  -DCMAKE_BUILD_TYPE=Release -DCFBOX_OPTIMIZE_FOR_SIZE=ON -DCFBOX_ENABLE_<APPLET>=ON
```

完整流程：

```bash
# 1. 构建 baseline（不含目标 applet）
cmake -B build-size-base -DCMAKE_BUILD_TYPE=Release \
    -DCFBOX_OPTIMIZE_FOR_SIZE=ON \
    -DCFBOX_ENABLE_<APPLET>=OFF
cmake --build build-size-base

# 2. 构建 with-applet
cmake -B build-size-new -DCMAKE_BUILD_TYPE=Release \
    -DCFBOX_OPTIMIZE_FOR_SIZE=ON \
    -DCFBOX_ENABLE_<APPLET>=ON
cmake --build build-size-new

# 3. 采集增量
scripts/measure_size.sh build-size-base/cfbox --format json > /tmp/base.json
scripts/measure_size.sh build-size-new/cfbox  --format json > /tmp/new.json

# 4. 计算 delta
python3 scripts/compute_delta.py /tmp/base.json /tmp/new.json --applet <APPLET>
```

### 4.2 报告字段

报告字段：

| 字段 | 要求 |
|------|------|
| applet | 新增或修改命令 |
| raw_delta | raw binary 增量 |
| text_delta | text section 增量 |
| data_delta | data section 增量 |
| shared_infra | 是否引入共享基础设施 |
| amortization | 被哪些 applet 复用 |
| budget_result | pass、warn、fail |

**budget_result 判定规则**：

| 结果 | 条件 |
|------|------|
| `pass` | raw_delta 在该 applet 预算以内 |
| `warn` | raw_delta 超预算 10% 以内 |
| `fail` | raw_delta 超预算 10% 以上，阻断合入 |

---

## 豁免规则

体积豁免必须说明：

- 超预算字节数。
- 用户可见能力。
- 是否可通过 profile 裁剪关闭。
- 是否影响 `minimal/rescue/container/embedded/full`。
- 关闭计划或长期接受理由。

### 豁免申请模板

```markdown
### 体积豁免申请

- **Applet/基础设施**: <名称>
- **超预算字节数**: <N> bytes (预算 <budget> bytes, 实际 <actual> bytes)
- **超预算百分比**: <P>%
- **用户可见能力**: <描述新增能力>
- **Profile 裁剪**: <是/否> — 通过 `-DCFBOX_ENABLE_<APPLET>=OFF` 关闭
- **影响 Profile**: <列出受影响 profile>
- **关闭计划**: <描述> 或 "长期接受"
- **理由**: <为什么必须接受此超预算>
```

### 豁免审批

- `warn` 级别：维护者可在 PR 中直接批准，需在 commit message 中注明 `size-exemption: warn`。
- `fail` 级别：需在 issue 中讨论，至少一名维护者批准后方可合入，commit message 注明 `size-exemption: fail (refs #N)`。
