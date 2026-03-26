# CFBox — Development RoadMap

> **目标**：用 CMake + C++23 实现最简洁的 busybox 替代品，单一可执行文件 + 符号链接分发，优先跑通核心功能，国际化/完整 POSIX 兼容性放后期。

---

## 项目基本信息

| 项目 | 值 |
|------|-----|
| 语言标准 | C++23 |
| 构建系统 | CMake 3.25+ |
| 目标平台 | Linux（aarch64 / x86_64）+ 嵌入式交叉编译 |
| 产物形态 | 单一可执行文件 `cfbox`，通过 `argv[0]` 或子命令 dispatch |
| 错误处理 | `std::expected<T, CfError>`（Rust-style Result） |
| Applet 注册 | `constexpr` 表驱动 |
| 测试 | GTest 单元测试 + Shell 集成测试（对比 GNU coreutils） |

---

## 一、目录结构（定稿）

```
cfbox/
├── CMakeLists.txt              # 顶层 CMake
├── cmake/
│   ├── Toolchain-aarch64.cmake # 交叉编译工具链示例
│   └── CompilerFlags.cmake     # 全局编译选项
├── include/
│   └── cfbox/
│       ├── applet.hpp          # AppletEntry 结构体 + constexpr 注册表
│       ├── error.hpp           # CfError / Result<T> 类型定义
│       ├── args.hpp            # 极简 argv 解析器
│       └── io.hpp              # 平台无关 I/O 包装（fd、读写、格式化）
├── src/
│   ├── main.cpp                # dispatch 入口（argv[0] + subcommand）
│   ├── applets/
│   │   ├── echo.cpp
│   │   ├── printf.cpp
│   │   ├── cat.cpp
│   │   ├── ls.cpp
│   │   ├── grep.cpp
│   │   ├── find.cpp
│   │   ├── sed.cpp
│   │   ├── cp.cpp
│   │   ├── mv.cpp
│   │   ├── rm.cpp
│   │   ├── mkdir.cpp
│   │   ├── head.cpp
│   │   ├── tail.cpp
│   │   ├── wc.cpp
│   │   ├── sort.cpp
│   │   └── uniq.cpp
│   └── install_links.cpp       # 可选：生成符号链接的 helper
├── tests/
│   ├── unit/                   # GTest 单元测试
│   │   ├── test_echo.cpp
│   │   ├── test_args.cpp
│   │   └── ...
│   └── integration/            # Shell 脚本集成测试
│       ├── run_all.sh
│       ├── helpers.sh          # diff_output() 等公共函数
│       ├── test_echo.sh
│       ├── test_cat.sh
│       └── ...
├── scripts/
│   └── gen_links.sh            # 自动生成符号链接脚本
└── docs/
    └── applet_status.md        # 每个 applet 的实现状态 checklist
```

---

## 开发阶段

### Phase 0 — 脚手架（基础能跑起来）
**目标：`cfbox echo hello` 能运行，测试框架能跑通**

- [ ] 初始化 git repo，`CMakeLists.txt` 骨架
- [ ] 实现 `error.hpp`（CfError + Result<T>）
- [ ] 实现 `applet.hpp`（AppletEntry + constexpr 表 + find_applet）
- [ ] 实现 `main.cpp` dispatch 逻辑
- [ ] 实现 `args.hpp` 基础版（仅短选项 + 位置参数）
- [ ] 实现第一个 applet：`echo`（最简单，无 syscall）
- [ ] 配置 GTest（FetchContent），写 `test_args.cpp` 第一个 case
- [ ] 写 `tests/integration/test_echo.sh`，CI 能跑通
- [ ] `scripts/gen_links.sh` 能生成符号链接并用链接调用成功

**完成标志**：`cmake -B build && cmake --build build && ./build/cfbox echo hello` 输出正确，GTest 绿灯。

---

### Phase 1 — 文本类 Applet（无复杂状态）
**目标：纯文本处理 applet 全部实现并通过集成测试**

优先顺序（由易到难）：

| Applet | 关键难点 | 预计复杂度 |
|--------|----------|------------|
| `echo` | `-n` `-e` 转义 | ⭐ |
| `printf` | 格式字符串解析 | ⭐⭐ |
| `cat` | `-n` `-A` 等 flag，stdin 透传 | ⭐ |
| `head` | `-n` 行数 / `-c` 字节数 | ⭐ |
| `tail` | 倒数读取（大文件需 ring buffer） | ⭐⭐ |
| `wc` | `-l` `-w` `-c` `-m` | ⭐⭐ |
| `sort` | `-r` `-n` `-u` `-k`，内存排序 | ⭐⭐⭐ |
| `uniq` | `-c` `-d` `-u`，依赖已排序输入 | ⭐⭐ |

**每个 applet 的完成标准**：
1. GTest 单元测试覆盖主要逻辑分支
2. Shell 集成测试 happy path + 至少 3 个 edge case 与 GNU 一致
3. 错误路径返回正确 exit code（1 或 2）

---

### Phase 2 — 文件系统类 Applet
**目标：fs 操作 applet 全部实现，处理好 errno 映射**

| Applet | 关键难点 | 预计复杂度 |
|--------|----------|------------|
| `mkdir` | `-p` 递归创建，mode 位 | ⭐⭐ |
| `rm` | `-r` 递归删除，`-f` 忽略错误 | ⭐⭐⭐ |
| `cp` | `-r` `-p` 保留权限，符号链接处理 | ⭐⭐⭐ |
| `mv` | 跨文件系统时 fallback 到 cp+rm | ⭐⭐⭐ |
| `ls` | `-l` `-a` `-h` 格式化，颜色可选 | ⭐⭐⭐ |

**注意事项**：
- `cp` / `mv` 需要处理 `EXDEV`（跨设备 rename 失败）
- `rm -r` 需要用 `openat` + `unlinkat` 防止 TOCTOU
- `ls -l` 的时间格式需与 GNU 对齐（`strftime`）

---

### Phase 3 — 搜索与流编辑
**目标：grep / find / sed 基础版可用**

| Applet | 范围限定（MVP） | 预计复杂度 |
|--------|----------------|------------|
| `grep` | `-E` BRE/ERE，`-i` `-v` `-n` `-r`，用 `<regex>` | ⭐⭐⭐ |
| `find` | `-name` `-type` `-maxdepth` `-exec` | ⭐⭐⭐⭐ |
| `sed` | `s/old/new/[g\|p\|d]`，行地址，不做完整 sed 语言 | ⭐⭐⭐⭐ |

> **降低风险的决策**：`grep` 直接用 `std::regex`（C++11 起标准库），不手写 NFA/DFA；`sed` 只实现 `s` 替换和 `d` 删除，不实现分支跳转。这两点可以在后续 Phase 5+ 增强。

---

### Phase 4 — 构建优化 & 嵌入式就绪
**目标：交叉编译通过，二进制尽量小**

- [ ] 验证 `Toolchain-aarch64.cmake` 能编译并在 qemu-aarch64 运行
- [ ] 开启 `-Os` / LTO，测量 stripped binary 大小
- [ ] 验证静态链接二进制（`-static`）无运行时依赖
- [ ] 用 `size` 命令检查各 section 大小，识别膨胀来源
- [ ] CI 中增加交叉编译 job

---

### Phase 5 — 完善度提升（持续迭代）
以下为后续可选增强，不影响 Phase 0-4 的 MVP 交付：

- `args.hpp` 增强：支持 `--long-flag=value`，自动生成 `--help`
- `grep` 替换为 手写 NFA 或引入 PCRE2（可选 CMake 选项）
- `sed` 扩展：`y` 音译，`a\` / `i\` 插入行，分支标签
- `ls` 颜色：`LS_COLORS` 环境变量解析
- `find` 扩展：`-mtime` `-newer` `-perm` `-delete`
- `printf` 完整 IEEE 754 格式化
- 国际化（i18n）：`gettext` 框架接入

---

## 六、代码规范约定

1. **所有 syscall 封装返回 `Result<T>`**，不允许直接 `if (open(...) < 0) exit(1)`
2. **`TRY()` 宏用于早返回**，避免深层嵌套
3. **Applet 主函数签名统一** `int applet_xxx(int argc, char** argv)`，内部自行 dispatch，exit code 直接 return
4. **禁止 `using namespace std`**，使用完整限定名或局部 using
5. **头文件只放声明 + inline constexpr**，不放实现（除模板）
6. **每个 `.cpp` 文件顶部必须有注释**：applet 名、支持的 flag 列表、已知与 GNU 的差异

---

## 七、进度追踪 Checklist

> 建议复制到 `document/applet_status.md` 随项目维护。

### 基础设施

- [ ] `error.hpp` 实现完成
- [ ] `applet.hpp` constexpr 表 + dispatch 实现完成
- [ ] `args.hpp` 短选项版本完成
- [ ] `main.cpp` argv[0] + subcommand dispatch 完成
- [ ] GTest 框架接入，至少一个测试绿灯
- [ ] Shell 集成测试 helpers.sh 完成
- [ ] `gen_links.sh` 符号链接生成脚本完成
- [ ] 交叉编译工具链文件完成并验证

### Phase 1 Applets

| Applet | 实现 | 单元测试 | 集成测试 | 备注 |
|--------|------|----------|----------|------|
| echo   | [ ]  | [ ]      | [ ]      |      |
| printf | [ ]  | [ ]      | [ ]      |      |
| cat    | [ ]  | [ ]      | [ ]      |      |
| head   | [ ]  | [ ]      | [ ]      |      |
| tail   | [ ]  | [ ]      | [ ]      |      |
| wc     | [ ]  | [ ]      | [ ]      |      |
| sort   | [ ]  | [ ]      | [ ]      |      |
| uniq   | [ ]  | [ ]      | [ ]      |      |

### Phase 2 Applets

| Applet | 实现 | 单元测试 | 集成测试 | 备注 |
|--------|------|----------|----------|------|
| mkdir  | [ ]  | [ ]      | [ ]      |      |
| rm     | [ ]  | [ ]      | [ ]      |      |
| cp     | [ ]  | [ ]      | [ ]      |      |
| mv     | [ ]  | [ ]      | [ ]      |      |
| ls     | [ ]  | [ ]      | [ ]      |      |

### Phase 3 Applets

| Applet | 实现 | 单元测试 | 集成测试 | 备注 |
|--------|------|----------|----------|------|
| grep   | [ ]  | [ ]      | [ ]      |      |
| find   | [ ]  | [ ]      | [ ]      |      |
| sed    | [ ]  | [ ]      | [ ]      |      |

---

## 八、关键风险与决策备忘

| 风险 | 决策 |
|------|------|
| `std::regex` 在某些 musl-libc 静态链接环境下有 bug | Phase 3 前验证，必要时换 PCRE2 或手写 |
| `sort` 大文件内存不足 | MVP 只做内存排序，后期加 external sort |
| `tail -f` follow 模式 | MVP 不实现，后期加 `inotify` |
| C++23 在老 GCC（<13）不完整 | 要求 GCC 13+ / Clang 17+，在 README 中声明 |
| 静态链接 + `std::regex` 体积膨胀 | 先度量，必要时换轻量 regex 库 |

---

*生成时间：2026-03-26 | 版本：v0.1*