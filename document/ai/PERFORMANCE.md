# CFBox 性能 标尺

> 这份文档讲性能优化的规矩——什么该优化、怎么测、怎么保证改完没改坏输出。大概一个季度才动一次。放在 `document/ai/`，从 [CLAUDE.md](../../CLAUDE.md) 和 [DIRECTIVES.md](DIRECTIVES.md) 指过来，和 [COVERAGE.md](COVERAGE.md)、[STRUCTURE-TASTE.md](STRUCTURE-TASTE.md) 平级。
>
> 一句话：性能优化的对象是"跑得快"（宿主 wall-clock），不是"算出不同的结果"——**cfbox 的输出字节 + 退出码在优化前后必须逐字节不变**，靠对照测试兜底（见 [COVERAGE.md](COVERAGE.md)）。

## 一、核心立场（三条）

1. **只动 wall-clock，绝不动输出**。优化 sed/grep/sort/md5sum 这些命令的吞吐时，输出必须和优化前（以及和 BusyBox/GNU oracle）逐字节一致。任何"优化"若改变了输出，就是 bug。
2. **wall-clock 和 cycle-accuracy 是两根轴，不串**。CFBox 是宿主命令工具，没有 cycle-accurate 模拟器，只量真实 wall-clock；不要把"指令数"伪装成"用户体验"。
3. **依赖原则**：cfbox **二进制实现**零外部依赖（手写 deflate/POSIX regex，保持小体积）；但**基建/测试用权威标准工具优先**——测试用 GTest、性能用 google-benchmark，都经 CPM 拉入（CPM 已经在用）。

## 二、每次性能改动走 4 步闭环

1. **构造真热路径场景**：拿真实大输入（大文件 grep/cat/sort、大目录 ls/find、大 tar、大 md5sum）来测，不写"空转微循环"那种白赢基准。
2. **对抗验保真**（两靶子）：
   - **场景保真**：基准真在跑那条热路径吗？没退化成"编译器一把梭"的常量循环吗？
   - **测量可信**：方差/噪声多大？归因到对的代码了吗？编译器有没有把工作优化掉（看汇编/反汇编证伪）？
3. **确认 bench 提升 AND 正确性不变**：吞吐有可测提升，**且**对照 harness + GTest + 集成全绿。
4. **纳入防退化**：把这条热路径的基准固化进 bench 套件，以后回归自动盯。

## 三、测量基建（Phase 0，优先于一切优化）

现在**完全没有**性能基建，先建：

- **bench 框架 = google-benchmark（经 CPMAddPackage，参照现有 GTest 用法）**。做**进程内微基准**：cfbox 的 `*_main(int, char**)` 入口可在进程内直接调（单测已经在这么干），bench 复用这个接缝，量热路径算法本身（排除进程启动开销）。
- **构建档 = Release `-O2`**（[CompilerFlag.cmake](../../cmake/compile/CompilerFlag.cmake) 已有，绝不 bench Debug/无优化构建）。
- **端到端 wall-clock**：另配一个 shell 脚本，对比 cfbox vs `/usr/bin/$cmd`（coreutils）在大样本上的耗时，贴近真实用户体验；复用对照 harness 的 fixture。
- **编译期 perf 计数器宏**：**暂缓**（在没有内测驱动前是死插桩；perf/callgrind 这种现成 profiler 对单二进制已能给源码级热点归因）。
- **防退化门 = advisory**（容差带 + 中位数，CI 只报告不挡）：bench 噪声和 CI 机器方差大，hard 门会误报；稳定后再考虑。

## 四、热路径清单（hunting 已确认，按收益排，附录）

- **sed 每行重编译正则**（[sed.cpp:229-262](../../src/applets/sed.cpp#L229-L262)）— CPU-bound，high。修法：parse 阶段编译一次存进 SedCommand。
- **md5sum 整文件 2× 内存 + 非流式**（[checksum.hpp:73-81](../../include/cfbox/checksum.hpp#L73-L81)）— 大文件 high。修法：md5 加 update/finalize 增量 API。
- **tar -c 整归档缓存在一个 string**（[tar.cpp:119-147](../../src/applets/tar.cpp#L119-L147)）— high（条件）。修法：流式逐成员写。
- **cmp 双文件全量载入，首差异不早退**（[cmp.cpp:33-50](../../src/applets/cmp.cpp#L33-L50)）— medium。修法：双缓冲块读 + memcmp。
- **io::for_each_line 逐字符 fgetc**（[io.hpp:137](../../include/cfbox/io.hpp#L137)）— 实测约 10× 慢于块读，medium（公共骨架）。修法：块 fread + memchr 找换行。
- **head/tail/sort/uniq 先 read_all 再 split** — medium。修法：head 流式早退、tail 反向 seek。

## 五、执行批次（标尺确认后，每批 propose-then-execute，全 behavior-preserving）

- **批 0（基建）**：CPM 拉 google-benchmark + 加 `tests/benchmark/` 目标（Release -O2，进程内微基准）+ 端到端 wall-clock 脚本 + CI advisory 门。建立基线数字。
- **批 1（演示闭环）**：sed 每行重编译 → 修，bench 证提升 + 对照 oracle 证输出不变 + 进防退化。
- **批 2+**：md5sum 流式、tar 流式、cmp 早退、for_each_line 块读……每条都走 4 步闭环。
