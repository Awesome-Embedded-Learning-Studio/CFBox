# Contributing to CFBox

Thank you for your interest in CFBox. This document covers building, testing, and contributing.

## Prerequisites

- **Compiler:** GCC 13+ or Clang 17+ (C++23 support required)
- **CMake:** 3.26+
- **Git**

## Building

### Debug Build (Default)

```bash
cmake -B build
cmake --build build
```

Debug builds automatically enable AddressSanitizer (ASan) and UndefinedBehaviorSanitizer (UBSan) with `-g3 -O0`. Compiler warnings are treated as errors (`-Werror`) with extensive warning flags (see [CompilerFlag.cmake](cmake/compile/CompilerFlag.cmake)).

### Release Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Release builds use `-O2` by default and enable LTO. For size-optimized builds, add `-DCFBOX_OPTIMIZE_FOR_SIZE=ON` to use `-Os` instead.

## Running Tests

```bash
# Unit tests (108 GTest cases)
ctest --test-dir build --output-on-failure

# Integration tests (16 shell scripts comparing against GNU coreutils)
bash tests/integration/run_all.sh
```

## Cross-Compilation

### AArch64

```bash
# Dynamic linking
cmake -B build-aarch64 \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain/Toolchain-aarch64.cmake \
  -DCMAKE_BUILD_TYPE=Release \
  -DCFBOX_OPTIMIZE_FOR_SIZE=ON
cmake --build build-aarch64

# Static linking
cmake -B build-aarch64-static \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain/Toolchain-aarch64.cmake \
  -DCMAKE_BUILD_TYPE=Release \
  -DCFBOX_OPTIMIZE_FOR_SIZE=ON \
  -DCFBOX_STATIC_LINK=ON
cmake --build build-aarch64-static
```

Requires: `aarch64-linux-gnu-g++` (install via `gcc-aarch64-linux-gnu g++-aarch64-linux-gnu`).

### ARMv7-A

```bash
# Static linking (requires Arm GNU Toolchain)
cmake -B build-armhf-static \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain/Toolchain-armhf.cmake \
  -DCMAKE_BUILD_TYPE=Release \
  -DCFBOX_OPTIMIZE_FOR_SIZE=ON \
  -DCFBOX_STATIC_LINK=ON
cmake --build build-armhf-static
```

Requires: [Arm GNU Toolchain](https://developer.arm.com/tools-and-software/open-source-software/developer-tools/gnu-toolchain) (`arm-none-linux-gnueabihf-g++`).

### Verifying Cross-Compiled Binaries

```bash
file build-aarch64/cfbox          # Confirm target architecture
size build-aarch64/cfbox          # Check section sizes
scripts/measure_size.sh build-aarch64/cfbox  # Detailed size analysis
```

## QEMU Testing

### User-Mode Testing

Run cross-compiled binaries under QEMU user-mode emulation with full integration tests:

```bash
# Requires: qemu-user-static
./scripts/qemu_user_test.sh --target aarch64 --link static
./scripts/qemu_user_test.sh --target armhf --link static
```

### System-Mode Testing

Boot a minimal Linux kernel with CFBox as PID 1. Requires the Linux kernel source (as a git submodule):

```bash
# Build initramfs
./scripts/build_initramfs.sh --arch aarch64 --cfbox build-aarch64-static/cfbox

# Boot and test
./scripts/qemu_system_test.sh \
  --arch aarch64 \
  --kernel path/to/Image \
  --initramfs build/aarch64-initramfs.cpio
```

The `init` applet auto-mounts proc/sysfs/devtmpfs, runs smoke tests, then powers off. Minimal kernel config: [configs/qemu-virt-aarch64.config](configs/qemu-virt-aarch64.config).

## CI Pipeline

The CI pipeline ([ci.yml](.github/workflows/ci.yml)) runs on every push/PR to `main` with 5 stages:

| Stage | Description |
|-------|-------------|
| **native** | x86-64 Debug build with ASan/UBSan + all tests |
| **release-size** | Release build (`-Os` + LTO) + binary size report |
| **cross-compile** | aarch64/armhf cross-compilation (dynamic + static) |
| **qemu-user-test** | Integration tests under QEMU user-mode emulation |
| **qemu-system-test** | Full boot test with CFBox as PID 1 (main/release branches only) |

## Code Style

- Format code with the project [.clang-format](.clang-format) (LLVM-based, 4-space indent, 100-column limit).
- All code must compile cleanly with the flags in [CompilerFlag.cmake](cmake/compile/CompilerFlag.cmake) (`-Wall -Wextra -Wpedantic` plus additional warnings, all treated as errors).
- Follow existing patterns:
  - `std::expected<T, Error>` with `CFBOX_TRY` for error handling
  - `cfbox::args::parse()` for CLI argument parsing
  - `cfbox::io` for file I/O, `cfbox::fs` for filesystem operations
- No `using namespace std`.

## Adding a New Applet

1. Create `src/applets/<name>.cpp` with signature `auto <name>_main(int argc, char* argv[]) -> int`.
   - Add a comment header listing supported flags and known differences from GNU.
2. Declare the function in [applets.hpp](include/cfbox/applets.hpp).
3. Add one entry to `APPLET_REGISTRY` in [applets.hpp](include/cfbox/applets.hpp).
4. Add GTest unit tests in `tests/unit/test_<name>.cpp` (see [test_capture.hpp](tests/unit/test_capture.hpp) for stdout capture and `TempDir` utilities).
5. Add shell integration tests in `tests/integration/test_<name>.sh` following the pattern in existing scripts.

> **Note:** The `init` applet is special — it runs as PID 1 in QEMU system-mode tests. Regular applets should not need special PID 1 handling.

## Submitting Changes

1. Fork the repository.
2. Create a feature branch.
3. Ensure all tests pass (`ctest` + `run_all.sh`).
4. Open a pull request against `main`.

## Reporting Issues

Use [GitHub Issues](https://github.com/Awesome-Embedded-Learning-Studio/CFBox/issues).
