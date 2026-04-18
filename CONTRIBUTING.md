# Contributing to CFBox

Thank you for your interest in CFBox. This document covers building, testing, and contributing.

## Prerequisites

- **Compiler:** GCC 13+ or Clang 17+ (C++23 support required)
- **CMake:** 3.26+
- **Git**

## Building

```bash
cmake -B build
cmake --build build
```

Debug builds (default) include AddressSanitizer and UBSan. For a release build:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## Running Tests

```bash
# Unit tests (108 GTest cases)
ctest --test-dir build --output-on-failure

# Integration tests (16 shell scripts)
bash tests/integration/run_all.sh
```

## Code Style

- Format code with the project `.clang-format` (LLVM-based, 4-space indent, 100-column limit).
- All code must compile cleanly with the flags in `cmake/compile/CompilerFlag.cmake` (`-Wall -Wextra -Wpedantic` plus additional warnings).
- Follow existing patterns:
  - `std::expected<T, Error>` with `CFBOX_TRY` for error handling
  - `cfbox::args::parse()` for CLI argument parsing
  - `cfbox::io` for file I/O, `cfbox::fs` for filesystem operations
- No `using namespace std`.

## Adding a New Applet

1. Create `src/applets/<name>.cpp` with signature `auto <name>_main(int argc, char* argv[]) -> int`.
   - Add a comment header listing supported flags and known differences from GNU.
2. Declare the function in `include/cfbox/applets.hpp`.
3. Add one entry to `APPLET_REGISTRY` in `include/cfbox/applets.hpp`.
4. Add GTest unit tests in `tests/unit/test_<name>.cpp` (see `test_capture.hpp` for stdout capture and `TempDir` utilities).
5. Add shell integration tests in `tests/integration/test_<name>.sh` following the pattern in existing scripts.

## Submitting Changes

1. Fork the repository.
2. Create a feature branch.
3. Ensure all tests pass (`ctest` + `run_all.sh`).
4. Open a pull request against `main`.

## Reporting Issues

Use [GitHub Issues](https://github.com/Awesome-Embedded-Learning-Studio/CFBox/issues).
