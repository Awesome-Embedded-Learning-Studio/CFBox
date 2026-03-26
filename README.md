# CFBox

A minimalist BusyBox alternative written in modern C++23.

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![C++23](https://img.shields.io/badge/C++23-00599C?logo=cplusplus)](https://en.cppreference.com/w/cpp/23)
[![CMake](https://img.shields.io/badge/CMake-3.25+-064F8C?logo=cmake)](https://cmake.org/)

## Overview

CFBox is a single-executable Unix utility collection distributed via symbolic links. Each subcommand can be invoked either through `argv[0]` detection or explicit subcommand syntax.

**Design Philosophy:**
- **Simplicity first** — Get core functionality working before pursuing full POSIX compliance
- **Modern C++** — Leverage C++23 features with `std::expected` for error handling
- **Embedded-friendly** — Cross-compilation ready, minimal runtime dependencies

## Quick Start

```bash
# Build
cmake -B build
cmake --build build

# Run via subcommand
./build/cfbox echo "Hello, World!"

# Install symbolic links
./scripts/gen_links.sh /usr/local/bin
```

## Supported Commands

| Category | Commands |
|----------|----------|
| **Text** | `echo`, `printf`, `cat`, `head`, `tail`, `wc`, `sort`, `uniq` |
| **Filesystem** | `mkdir`, `rm`, `cp`, `mv`, `ls` |
| **Search** | `grep`, `find`, `sed` |

*See [docs/applet_status.md](docs/applet_status.md) for implementation status*

## Requirements

- **Compiler:** GCC 13+ / Clang 17+
- **CMake:** 3.25+
- **Platforms:** Linux (x86_64 / aarch64)

## Project Structure

```
cfbox/
├── include/cfbox/      # Core headers (error, applet, args, io)
├── src/
│   ├── main.cpp        # Dispatch entry point
│   ├── applets/        # Individual command implementations
│   └── install_links.cpp
├── tests/
│   ├── unit/           # GTest unit tests
│   └── integration/    # Shell integration tests
└── scripts/
    └── gen_links.sh    # Symlink generation
```

## Development Status

| Phase | Status | Description |
|-------|--------|-------------|
| Phase 0 | ![Pending](https://img.shields.io/badge/Status-Todo-gray) | Scaffolding & build system |
| Phase 1 | ![Pending](https://img.shields.io/badge/Status-Todo-gray) | Text processing utilities |
| Phase 2 | ![Pending](https://img.shields.io/badge/Status-Todo-gray) | Filesystem operations |
| Phase 3 | ![Pending](https://img.shields.io/badge/Status-Todo-gray) | Search & stream editing |

See [ROADMAP.md](ROADMAP.md) for detailed planning.

## License

MIT License — see [LICENSE](LICENSE) for details.
