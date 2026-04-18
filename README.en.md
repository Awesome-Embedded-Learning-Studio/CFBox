# CFBox

[中文](README.md) | **[English](README.en.md)**

A minimalist BusyBox alternative written in modern C++23.

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![C++23](https://img.shields.io/badge/C++23-00599C?logo=cplusplus)](https://en.cppreference.com/w/cpp/23)
[![CMake](https://img.shields.io/badge/CMake-3.26+-064F8C?logo=cmake)](https://cmake.org/)
[![Tests](https://img.shields.io/badge/Tests-124_passing-brightgreen)](tests/)
[![Applets](https://img.shields.io/badge/Applets-16%2F16-brightgreen)](src/applets/)

## Overview

CFBox is a single-executable Unix utility collection distributed via symbolic links. Each subcommand can be invoked either through `argv[0]` detection or explicit subcommand syntax.

**Design philosophy:**
- **Simplicity first** — Get core functionality working before pursuing full POSIX compliance
- **Modern C++** — Leverage C++23 features with `std::expected` for error handling
- **Embedded-friendly** — Cross-compilation ready, minimal runtime dependencies

All 16 applets are implemented and tested.

## Quick Start

```bash
# Build
cmake -B build
cmake --build build

# Test
ctest --test-dir build --output-on-failure   # 108 GTest unit tests
bash tests/integration/run_all.sh            # 16 integration test scripts

# Run via subcommand
./build/cfbox echo "Hello, World!"

# Or install symbolic links
./scripts/gen_links.sh /usr/local/bin
echo "Hello, World!"   # now calls cfbox via symlink
```

## Supported Commands

| Applet | Supported Flags / Features |
|--------|----------------------------|
| `echo` | `-n` (no trailing newline), `-e` (interpret escape sequences) |
| `printf` | Format strings (`%s` `%d` `%f` `%c` `%%`), format reuse |
| `cat` | `-n` (number lines), `-b` (number non-blank), `-A` (show non-printing), stdin passthrough |
| `head` | `-n N` (first N lines), `-c N` (first N bytes), multi-file headers |
| `tail` | `-n N` (last N lines), `-c N` (last N bytes), multi-file headers |
| `wc` | `-l` (lines), `-w` (words), `-c` (bytes), `-m` (chars), multi-file totals |
| `sort` | `-r` (reverse), `-n` (numeric), `-u` (unique), `-k N` (key field), multi-file merge |
| `uniq` | `-c` (count), `-d` (duplicates only), `-u` (unique only), stdin support |
| `mkdir` | `-p` (create parents), `-m MODE` (permissions) |
| `rm` | `-r` (recursive), `-f` (force), `-i` (interactive), `/` safety check |
| `cp` | `-r` (recursive), `-p` (preserve permissions), multi-file to directory |
| `mv` | `-f` (force overwrite), cross-filesystem fallback (copy + remove) |
| `ls` | `-a` (show hidden), `-l` (long format), `-h` (human-readable sizes) |
| `grep` | `-E` (extended regex), `-i` (ignore case), `-v` (invert), `-n` (line numbers), `-r` (recursive), `-c` (count), `-l` (files with matches), `-q` (quiet) |
| `find` | `-name PATTERN` (glob), `-type [f\|d\|l]`, `-maxdepth N`, `-exec CMD {} ;` |
| `sed` | `-n` (suppress auto-print), `-e SCRIPT`; substitution `s/pat/repl/[g\|p\|d]`, line addresses, ranges, `$` |

## Requirements

- **Compiler:** GCC 13+ / Clang 17+ (C++23 support required)
- **CMake:** 3.26+
- **Platform:** Linux (x86_64 / aarch64)

## Architecture

**Dispatch mechanism:** `main.cpp` detects the invoked name via `argv[0]` (symlink detection) and falls back to subcommand syntax (`cfbox echo ...`). The `APPLET_REGISTRY` in [applets.hpp](include/cfbox/applets.hpp) is a `constexpr std::array` mapping names to entry functions.

**Core infrastructure:**

| Header | Purpose |
|--------|---------|
| [error.hpp](include/cfbox/error.hpp) | `std::expected<T, Error>` with `CFBOX_TRY` macro for error propagation |
| [applet.hpp](include/cfbox/applet.hpp) | `AppEntry` struct and `find_applet()` lookup |
| [args.hpp](include/cfbox/args.hpp) | CLI argument parser — short flags, flag values, `--` separator, positional args |
| [io.hpp](include/cfbox/io.hpp) | File I/O helpers — `read_all`, `read_lines`, `read_all_stdin`, `write_all`, `split_lines` |
| [fs_util.hpp](include/cfbox/fs_util.hpp) | Filesystem wrappers returning `Result<T>` — `exists`, `mkdir_recursive`, `copy_recursive`, `rename`, etc. |
| [escape.hpp](include/cfbox/escape.hpp) | Escape sequence processing for `echo` / `printf` |

**Testing:** GTest unit tests via CPM fetch (108 cases); shell-based integration tests (16 scripts) comparing against GNU coreutils behavior.

## Project Structure

```
cfbox/
├── CMakeLists.txt
├── cmake/
│   ├── compile/CompilerFlag.cmake
│   ├── third_party/CPM.cmake
│   └── toolchain/               # cross-compilation toolchains (Phase 4)
├── include/cfbox/
│   ├── applet.hpp               # AppEntry registry & lookup
│   ├── applets.hpp              # 16 applet declarations & APPLET_REGISTRY
│   ├── args.hpp                 # CLI argument parser
│   ├── error.hpp                # std::expected<T, Error> & CFBOX_TRY
│   ├── escape.hpp               # escape sequence processor
│   ├── fs_util.hpp              # filesystem utility wrappers
│   └── io.hpp                   # file I/O helpers
├── src/
│   ├── main.cpp                 # Dispatch entry (argv[0] + subcommand)
│   └── applets/                 # 16 command implementations
│       ├── echo.cpp   printf.cpp  cat.cpp    head.cpp
│       ├── tail.cpp   wc.cpp      sort.cpp   uniq.cpp
│       ├── mkdir.cpp  rm.cpp      cp.cpp     mv.cpp
│       ├── ls.cpp     grep.cpp    find.cpp   sed.cpp
├── tests/
│   ├── unit/                    # GTest unit tests (108 cases)
│   └── integration/             # Shell integration tests (16 scripts)
├── scripts/
│   └── gen_links.sh             # Symlink installer
├── .clang-format
├── CONTRIBUTING.md
├── LICENSE
├── README.md
└── ROADMAP.md
```

## Development Status

All core applets implemented (Phase 0-3 complete). See [ROADMAP.md](ROADMAP.md) for remaining Phase 4 (cross-compilation) work.

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for build instructions, coding conventions, and how to submit changes.

## License

MIT License — see [LICENSE](LICENSE) for details.
