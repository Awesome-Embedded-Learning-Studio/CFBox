# CFBox

[中文](README.md) | **[English](README.en.md)**

A minimalist BusyBox alternative written in modern C++23.

[![CI](https://github.com/Awesome-Embedded-Learning-Studio/CFBox/actions/workflows/ci.yml/badge.svg)](https://github.com/Awesome-Embedded-Learning-Studio/CFBox/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![C++23](https://img.shields.io/badge/C++23-00599C?logo=cplusplus)](https://en.cppreference.com/w/cpp/23)
[![CMake](https://img.shields.io/badge/CMake-3.26+-064F8C?logo=cmake)](https://cmake.org/)
[![Tests](https://img.shields.io/badge/Tests-149_passing-brightgreen)](tests/)
[![Applets](https://img.shields.io/badge/Applets-17-brightgreen)](src/applets/)

## Overview

CFBox is a single-executable Unix utility collection distributed via symbolic links. 17 applets implemented and tested, with a CI pipeline covering native builds, cross-compilation, and QEMU user/system-mode testing across 5 stages. Features configurable CMake builds (per-applet toggles), GNU-style long options, and colored help output.

**Design philosophy:** Simplicity first — Modern C++ (`std::expected`) — Embedded-friendly (cross-compilation, static linking)

## Quick Start

```bash
# Build
cmake -B build
cmake --build build

# Test
ctest --test-dir build --output-on-failure   # 149 GTest unit tests
bash tests/integration/run_all.sh            # 17 integration test scripts

# Run via subcommand
./build/cfbox echo "Hello, World!"

# Or install symbolic links
./scripts/gen_links.sh /usr/local/bin
echo "Hello, World!"   # now calls cfbox via symlink
```

## Supported Commands

### Text Processing

| Applet | Supported Flags / Features |
|--------|----------------------------|
| `echo` | `-n` (no trailing newline), `-e` (interpret escape sequences), all applets support `--help` / `--version` |
| `printf` | Format strings (`%s` `%d` `%f` `%c` `%%`), format reuse |
| `cat` | `-n` (number lines), `-b` (number non-blank), `-A` (show non-printing), stdin passthrough |
| `head` | `-n N` (first N lines), `-c N` (first N bytes), multi-file headers |
| `tail` | `-n N` (last N lines), `-c N` (last N bytes), multi-file footers |
| `wc` | `-l` (lines), `-w` (words), `-c` (bytes), `-m` (chars), multi-file totals |
| `sort` | `-r` (reverse), `-n` (numeric), `-u` (unique), `-k N` (key field), multi-file merge |
| `uniq` | `-c` (count), `-d` (duplicates only), `-u` (unique only), stdin support |
| `grep` | `-E` (extended regex), `-i` (ignore case), `-v` (invert), `-n` (line numbers), `-r` (recursive), `-c` (count), `-l` (files with matches), `-q` (quiet) |
| `sed` | `-n` (suppress auto-print), `-e SCRIPT`; substitution `s/pat/repl/[g\|p\|d]`, line addresses, ranges, `$` |

### File Operations

| Applet | Supported Flags / Features |
|--------|----------------------------|
| `mkdir` | `-p`/`--parents` (create parents), `-m`/`--mode MODE` (permissions) |
| `rm` | `-r`/`--recursive` (recursive), `-f`/`--force` (force), `-i` (interactive), `/` safety check |
| `cp` | `-r`/`--recursive` (recursive), `-p`/`--preserve` (preserve permissions), multi-file to directory |
| `mv` | `-f` (force overwrite), cross-filesystem fallback (copy + remove) |

### Directory & Search

| Applet | Supported Flags / Features |
|--------|----------------------------|
| `ls` | `-a`/`--all` (show hidden), `-l`/`--long` (long format), `-h`/`--human-readable` (human-readable sizes) |
| `find` | `-name PATTERN` (glob), `-type [f\|d\|l]`, `-maxdepth N`, `-exec CMD {} ;` |

### System

| Applet | Description |
|--------|-------------|
| `init` | System initialization — auto-mounts proc/sysfs/devtmpfs when PID 1, runs smoke tests, then powers off |

## Requirements

- **Compiler:** GCC 13+ / Clang 17+ (C++23 support required)
- **CMake:** 3.26+
- **Platform:** Linux (x86_64 / aarch64)

## Documentation

| Document | Description |
|----------|-------------|
| [Architecture & Design](document/architecture.md) | Dispatch mechanism, core infrastructure, error handling, testing |
| [Cross-Compilation & Embedded](document/cross-compilation.md) | Toolchains, CMake options, build examples, binary sizes |
| [QEMU Testing](document/qemu-testing.md) | User-mode / system-mode testing, init applet, kernel config |
| [Continuous Integration](document/ci.md) | CI pipeline 5-stage overview |
| [Contributing Guide](CONTRIBUTING.md) | Build, test, code style, submission |

## Project Structure

```
cfbox/
├── CMakeLists.txt
├── cmake/
│   ├── Config.cmake                  # Per-applet configuration (CFBOX_ENABLE_xxx options)
│   ├── compile/CompilerFlag.cmake    # Compiler warnings & optimization flags
│   ├── third_party/CPM.cmake        # CPM dependency manager
│   └── toolchain/                   # Cross-compilation toolchains
├── configs/
│   └── qemu-virt-aarch64.config     # Minimal QEMU aarch64 kernel config
├── document/                         # Detailed documentation
├── include/cfbox/
│   ├── applet_config.hpp.in         # CMake-generated config (version + enable flags)
│   ├── applet.hpp / applets.hpp     # Registry & dispatch
│   ├── args.hpp                     # Short + long option argument parser
│   ├── help.hpp                     # --help / --version help system
│   ├── term.hpp                     # ANSI colored output (NO_COLOR support)
│   ├── utf8.hpp                     # Unicode-aware width/count utilities
│   └── ...                          # error.hpp, io.hpp, fs_util.hpp, escape.hpp
├── src/
│   ├── main.cpp                     # Dispatch entry
│   └── applets/                     # 17 command implementations
├── tests/
│   ├── unit/                        # GTest unit tests (149 cases)
│   └── integration/                 # Shell integration tests (17 scripts)
├── scripts/                         # Build, test, install scripts
├── .github/workflows/ci.yml         # CI pipeline
└── CONTRIBUTING.md                   # Contributing guide
```

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md).

## License

MIT License — see [LICENSE](LICENSE) for details.
