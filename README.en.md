# CFBox

[中文](README.md) | **[English](README.en.md)**

A minimalist BusyBox alternative written in modern C++23.

[![CI](https://github.com/Awesome-Embedded-Learning-Studio/CFBox/actions/workflows/ci.yml/badge.svg)](https://github.com/Awesome-Embedded-Learning-Studio/CFBox/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![C++23](https://img.shields.io/badge/C++23-00599C?logo=cplusplus)](https://en.cppreference.com/w/cpp/23)
[![CMake](https://img.shields.io/badge/CMake-3.26+-064F8C?logo=cmake)](https://cmake.org/)
[![Tests](https://img.shields.io/badge/Tests-331_passing-brightgreen)](tests/)
[![Applets](https://img.shields.io/badge/Applets-109-brightgreen)](src/applets/)

## Overview

CFBox is a single-executable Unix utility collection distributed via symbolic links. 109 applets implemented and tested, with a CI pipeline covering native builds, cross-compilation, and QEMU user/system-mode testing. Features configurable CMake builds (per-applet toggles), GNU-style long options, and colored help output.

**Design philosophy:** Simplicity first — Modern C++ (`std::expected`) — Embedded-friendly (cross-compilation, static linking)

## Size Comparison

| Project | Language | Size | Applets | Size/Applet |
|---------|----------|------|---------|-------------|
| **CFBox (size-opt)** | **C++23** | **446 KB** | **109** | **~4.1 KB** |
| Toybox | C | ~500 KB | 238 | ~2.1 KB |
| BusyBox (full) | C | ~1.7 MB | 274 | ~9 KB |
| uutils/coreutils | Rust | ~11 MB | ~100 | ~110 KB |

> CFBox is **3-4x smaller** than BusyBox while providing a complete AWK interpreter, archive suite (tar/cpio/ar/unzip/gzip), diff/patch (Myers O(ND) algorithm), process tools (ps/top/pstree/pgrep/pmap), and a built-in TUI framework.

## Performance

| Operation | Data Size | Time |
|-----------|-----------|------|
| grep -c | 10 MB | 54 ms |
| cat | 10 MB | 63 ms |
| wc | 10 MB | 17 ms |
| sort | 100K lines | 32 ms |
| diff | 100K lines (similar) | 79 ms |

- grep/cat/wc use streaming I/O — reading `/dev/urandom` won't exhaust memory
- diff uses Myers O(ND) algorithm; sort precomputes keys to avoid repeated allocation
- Zero external dependencies: hand-written lightweight deflate/inflate replaces zlib

## Quick Start

```bash
# Build
cmake -B build
cmake --build build

# Test
ctest --test-dir build --output-on-failure   # 331 GTest unit tests
bash tests/integration/run_all.sh            # 54 integration test scripts

# Run via subcommand
./build/cfbox echo "Hello, World!"

# Or install symbolic links
./scripts/gen_links.sh /usr/local/bin
echo "Hello, World!"   # now calls cfbox via symlink
```

## Supported Commands (109)

### Text Processing (31)

`echo`, `printf`, `cat`, `head`, `tail`, `wc`, `sort`, `uniq`, `grep`, `sed`, `fold`, `expand`, `cut`, `paste`, `nl`, `comm`, `tr`, `tac`, `rev`, `shuf`, `factor`, `od`, `split`, `seq`, `tsort`, `expr`, `awk`, `diff`, `patch`, `cmp`, `ed`

### File Operations (19)

`mkdir`, `rm`, `cp`, `mv`, `ls`, `find`, `ln`, `touch`, `stat`, `install`, `mktemp`, `truncate`, `du`, `df`, `readlink`, `realpath`, `rmdir`, `link`, `unlink`

### Archive & Compression (6)

`tar` (ustar format), `cpio` (newc format), `ar` (static library), `unzip`, `gzip`, `gunzip`

### Shell & Scripting (2)

`sh` (POSIX shell: pipes, redirections, variable expansion, command substitution, if/while/for, 15 builtins), `xargs`

### System Info (20)

`pwd`, `basename`, `dirname`, `uname`, `hostname`, `whoami`, `id`, `tty`, `date`, `nproc`, `logname`, `hostid`, `printenv`, `env`, `uptime`, `free`, `cal`, `dmesg`, `who`, `test`

### Process Management (15)

`ps`, `top`, `kill`, `pgrep`/`pkill`, `pidof`, `pstree`, `pmap`, `fuser`, `pwdx`, `sysctl`, `iostat`, `watch`, `nice`, `renice`, `timeout`

### Other (16)

`true`, `false`, `yes`, `sleep`, `usleep`, `sync`, `nohup`, `cksum`, `md5sum`, `sum`, `hexdump`, `more`, `tee`, `init` (PID 1 initramfs init system), `mkfifo`, `mknod`

> All applets support `--help` / `--version`

## Requirements

- **Compiler:** GCC 13+ / Clang 17+ (C++23 support required)
- **CMake:** 3.26+
- **Platform:** Linux (x86_64 / aarch64)

## Documentation

| Document | Description |
|----------|-------------|
| [Architecture & Design](document/architecture.md) | Dispatch mechanism, core infrastructure, error handling, testing |
| [Production Roadmap](document/todo/README.md) | Production roadmap docs for Phase 4.5 to v1.0; currently maintained in Chinese |
| [Cross-Compilation & Embedded](document/cross-compilation.md) | Toolchains, CMake options, build examples, binary sizes |
| [QEMU Testing](document/qemu-testing.md) | User-mode / system-mode testing, init applet, kernel config |
| [Continuous Integration](document/ci.md) | CI pipeline overview |
| [Contributing Guide](CONTRIBUTING.md) | Build, test, code style, submission |

## Project Structure

```
cfbox/
├── CMakeLists.txt
├── cmake/
│   ├── Config.cmake                  # Per-applet configuration (CFBOX_ENABLE_xxx options)
│   ├── compile/CompilerFlag.cmake    # Compiler warnings & optimization flags
│   ├── third_party/CPM.cmake        # CPM dependency manager (GTest only)
│   └── toolchain/                   # Cross-compilation toolchains
├── include/cfbox/
│   ├── applet.hpp / applets.hpp     # Registry & dispatch
│   ├── args.hpp                     # Short + long option argument parser
│   ├── error.hpp                    # std::expected error handling + CFBOX_TRY
│   ├── io.hpp                       # Streaming I/O (for_each_line, read_all, write_all)
│   ├── stream.hpp                   # Line-by-line pipeline, LineProcessor
│   ├── deflate.hpp / inflate.hpp    # Hand-written lightweight DEFLATE (zero deps)
│   ├── compress.hpp                 # gzip wrapper
│   ├── utf8.hpp                     # Unicode width/count (constexpr + static_assert)
│   ├── term.hpp                     # ANSI colored output (NO_COLOR support)
│   ├── terminal.hpp                 # Terminal control (RawMode RAII, cursor, double buffer)
│   ├── tui.hpp                      # TUI framework (ScreenBuffer, Key, TuiApp)
│   ├── proc.hpp                     # /proc parser (processes, memory, CPU, disks)
│   ├── regex.hpp                    # POSIX regex RAII (scoped_regex)
│   └── ...                          # help.hpp, fs_util.hpp, escape.hpp, checksum.hpp
├── src/
│   ├── main.cpp                     # Dispatch entry
│   └── applets/                     # 109 command implementations
├── tests/
│   ├── unit/                        # GTest unit tests (331 cases)
│   └── integration/                 # Shell integration tests (54 scripts)
└── scripts/                         # Build, test, install scripts
```

## Next Steps

Current release: v0.1.0. Upcoming work, in priority order:

### Phase 0: Production Pre-gates (In Progress)

Before adding new applets, the following quality foundations must be completed:

| Phase | Scope | Status |
|-------|-------|--------|
| **0A** Baseline Inventory | 109-applet catalog, maturity labels, profile assignments, doc drift fixes | Pending |
| **0B** Perf Baseline | Core benchmarks (cat/grep/sed/sort/find/tar/gzip/cp/tail), RSS regression thresholds | Pending |
| **0C** Size Budget | Per-profile size caps, new rescue/container profiles, per-applet delta tracking | Pending |
| **0D** IO Policy | Streaming audit (head/tail/sed/tr/md5sum etc.), large file/pipe/broken pipe tests | Pending |
| **0E** Safety Hardening | Unified numeric parsers, parser fuzz smoke, privileged command isolation tests | Pending |
| **0F** CI & Release | Tiered CI (sanitizer/benchmark/differential/cross/QEMU), reproducible builds, error format spec | Pending |

### Phase 1: Core System (After Phase 0)

New system-level applets: `chmod`, `chown`, `chgrp`, `mount`, `umount`, `chroot`, `dd`, `stty`, plus deepening existing core commands.

> See [document/todo/README.md](document/todo/README.md) for the full roadmap.

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md).

## License

MIT License — see [LICENSE](LICENSE) for details.
