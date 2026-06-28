# CFBox 结构与工艺 标尺

> 这份文档讲代码结构与工艺原则——怎么让代码"好读、好改、不容易在改动时引入 bug"。大概一个季度才动一次。放在 `document/ai/` 下，从 [CLAUDE.md](../../CLAUDE.md) 和 [DIRECTIVES.md](DIRECTIVES.md) 指过来，和 [CODING-TASTE.md](CODING-TASTE.md)（管命名/格式那种微观风格）、[COVERAGE.md](COVERAGE.md) 平级。
>
> 一句话：微观风格交给 clang-format 和 CODING-TASTE；这份管"函数多大、逻辑有没有重复、抽象边界有没有串"这种结构性的事。

## 一、七条结构与工艺原则（每条配 CFBox 真实例子）

1. **一个函数干一件事、停在一个抽象层次**。规模超限往往是职责堆叠的症状。
   - 反例：tail.cpp（436 行）单文件混了"静态 tail + -f 跟随 + 信号处理"三件事；io::for_each_line 把"逐字符读"和"尾行边界处理"混在一层。
2. **DRY——逻辑唯一归属，第二份出现就抽**。
   - 反例：用户名→名字解析（getpwuid 失败回退）在 ls/stat/id/whoami 写了 4 份，且回退策略不一致（stat 回退空串、其余回退数字）；diff.cpp 的 hunk 行号统计整段复制两遍；sed.cpp 的分隔段抽取循环重复；sh_main 手写 fread 循环重复了 io::read_all。
3. **抽象边界不串——通用层不长出应用语义**。
   - 反例：stream 层的 run_processor 自己拼 "cfbox:" 错误信息（应用语义泄漏进通用流层）；fs::for_each_entry 静默吞掉迭代错误。
4. **显式优先 / 数据驱动 dispatch——优先级是语义时做成有序表，别藏进 if-else 物理顺序**。
   - ✅ 正面：APPLET_REGISTRY 是 constexpr 数据表（分发靠查表，不是 if-else 链）；find/test 用递归下降解析器，优先级做进语法结构。
5. **可测试性是设计约束——留可独立测试的接缝**。
   - ✅ 正面：test_capture.hpp 在进程内直接调 `_main`，等于一个轻量 harness；反面：LineProcessor 这个虚基类被引入却零引用（死抽象）。
6. **最简表达——别写"算恒等"的冗余代码**。
   - 反例：io::for_each_line 尾部两个 constexpr 分支体逐字相同；ErrorView 类型只在单测里用、生产零引用（YAGNI 死代码）。
7. **控制流纪律——嵌套≤3 层；一行 if 也用 {}**。
   - 反例：个别函数嵌套 ≥3 层（diff 的 build_hunks）。整体偏干净。

## 二、机械护栏（脚本可查、版本无关的进 CI 当硬门；版本敏感的做软门）

**硬门（CI exit 非零，精确匹配函数调用、不匹配注释/标识符）：**

- **禁不安全 C 函数**：`sprintf(`/`strcpy(`/`strcat(`/`gets(` → 用 `snprintf` / `std::string` / `std::string_view` 替代。现状：0 处真违规（历史 grep 命中全是注释/awk 函数名误报）。
- **禁裸 `std::fopen`（在 applets 里）**：文本/二进制读写走 `cfbox::io::open_file`（带 Result + RAII）。现状：15 处，本维度清掉。
- **禁 `std::stoi`/`std::stol`**：项目 -fno-exceptions，这俩在非法输入上抛异常→std::terminate（崩溃）。改 `std::strtol`+errno 或 `std::from_chars` + CFBOX_ERR 干净报错。现状：19 处（全是命令行数值参数解析），本维度清掉。
- **layering 门**：applets 不自造递归目录遍历，用 `cfbox::fs::for_each_entry` 或加注释豁免。现状：5 处，grandfather 进基线清单，新加的裸奔才挡。

**软门（advisory，本地 + IDE；不上 CI Werror，直到 CI pin 死工具链版本）：**

- **clang-format dry-run**：CI 加一步检查（先只报告；CODING-TASTE 说"CI 也跑"，现状没跑——补上）。
- **clang-tidy**（含 `readability-function-size` 行数门、`readability-braces` 等）：版本敏感，advisory。

## 三、每次改结构，走这三步

1. 先看牵连：grep 被改抽象的引用方（`cfbox::fs::`、`cfbox::io::`、被改头文件的 include 方）。
2. 行为不变：结构改动不得改变可观测输出——靠 GTest + 集成 + 对照测试兜底（见 [COVERAGE.md](COVERAGE.md)）。
3. 收尾同步：改完跑 clang-format；结构性决策记进 [document/notes/](../notes/)。

## 四、附录：CFBox 现状（2026-06-28 勘查）

- **死抽象**：`stream::LineProcessor`/`run_processor`（[stream.hpp:57-78](../../include/cfbox/stream.hpp#L57-L78)，0 引用）、`base::ErrorView`（[error.hpp:14-17](../../include/cfbox/error.hpp#L14-L17)，生产零引用）。
- **DRY 债**：owner 名字解析 4 份回退不一致；diff hunk 统计复制两遍；sed 段抽取重复；sh_main 重复 read_all。
- **banned-pattern**：裸 fopen 15 处、stoi/stol 19 处、不安全 C 函数 0 处真违规。
- **layering**：5 个 applet 自造递归遍历（grep/find/tar/du/sysctl）。
- **微观风格门缺失**：CI 没 clang-format/clang-tidy 步骤。
- **大文件**（职责堆叠候选，单文件）：tail.cpp 436、sed.cpp 368、ls.cpp 336、find.cpp 313。（sh/awk 是多文件按职责拆分，大但合理。）

## 五、执行批次（标尺确认后，每批 propose-then-execute，全行为保持）

- **批1（零风险）**：删死抽象 LineProcessor/run_processor + ErrorView（+ 其单测）。0 引用，体积小降。
- **批2**：清 19 处 stoi/stol → strtol/from_chars + CFBOX_ERR（修潜在崩溃）。
- **批3**：清 15 处裸 fopen → io::open_file / read_all / for_each_line。
- **批4**：DRY 收敛——owner 名字解析（统一回退）、diff hunk、sed 段抽取。
- **批5**：上机械护栏脚本（banned-pattern 精确匹配 + layering grandfather）+ clang-format dry-run，接进 CI。
