#!/usr/bin/env bash
# Run CFBox integration tests under QEMU user-mode emulation.
# Usage: scripts/qemu_user_test.sh [--target aarch64|armhf|all] [--link static|dynamic]
set -euo pipefail

script_dir="$(cd "$(dirname "$0")" && pwd)"
project_dir="$(cd "$script_dir/.." && pwd)"

# Defaults
target="all"
link="static"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --target) target="$2"; shift 2 ;;
        --link)   link="$2";   shift 2 ;;
        -h|--help)
            echo "Usage: $0 [--target aarch64|armhf|all] [--link static|dynamic]"
            exit 0 ;;
        *) echo "Unknown option: $1"; exit 1 ;;
    esac
done

# Determine which targets to test
if [[ "$target" == "all" ]]; then
    targets=(aarch64 armhf)
else
    targets=("$target")
fi

# Map target architecture to QEMU binary name
qemu_bin_for_arch() {
    case "$1" in
        aarch64) echo "qemu-aarch64-static" ;;
        armhf)   echo "qemu-arm-static" ;;
        *)       echo "qemu-$1-static" ;;
    esac
}

# Verify QEMU is installed
check_qemu() {
    local qemu_bin
    qemu_bin="$(qemu_bin_for_arch "$1")"
    if ! command -v "$qemu_bin" &>/dev/null; then
        echo "ERROR: $qemu_bin not found. Install: sudo pacman -S qemu-user-static"
        return 1
    fi
}

# Run tests for a single target
run_tests() {
    local arch="$1"
    local qemu_bin
    qemu_bin="$(qemu_bin_for_arch "$arch")"
    local build_dir="$project_dir/build-${arch}"

    if [[ "$link" == "static" ]]; then
        build_dir="${build_dir}-static"
    fi

    local cfbox_binary="$build_dir/cfbox"
    if [[ ! -x "$cfbox_binary" ]]; then
        echo "ERROR: $cfbox_binary not found. Build it first."
        echo "  cmake -B $build_dir \\"
        echo "    -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain/Toolchain-${arch}.cmake \\"
        echo "    -DCMAKE_BUILD_TYPE=Release -DCFBOX_OPTIMIZE_FOR_SIZE=ON \\"
        $([[ "$link" == "static" ]] && echo "    -DCFBOX_STATIC_LINK=ON \\")
        echo "  cmake --build $build_dir"
        return 1
    fi

    echo "========================================"
    echo "Testing $arch ($link) via $qemu_bin"
    echo "Binary: $cfbox_binary"
    echo "========================================"

    # Smoke test
    echo "--- Smoke test ---"
    "$qemu_bin" "$cfbox_binary" --list
    echo ""
    "$qemu_bin" "$cfbox_binary" echo "Hello from $arch via QEMU user-mode"
    echo ""

    # Full integration test suite
    # Create a wrapper script so existing test scripts work without modification.
    # The wrapper invokes: qemu-<arch>-static <cfbox_binary> "$@"
    local wrapper
    wrapper="$(mktemp /tmp/cfbox-qemu-wrapper-XXXXXX.sh)"
    cat > "$wrapper" << WRAPPER
#!/bin/sh
exec $qemu_bin $cfbox_binary "\$@"
WRAPPER
    chmod +x "$wrapper"

    echo "--- Integration tests ---"
    CFBOX="$wrapper" bash "$project_dir/tests/integration/run_all.sh"
    local test_result=$?

    rm -f "$wrapper"
    return $test_result
}

# Main
total_fail=0
for t in "${targets[@]}"; do
    if ! check_qemu "$t"; then
        echo "SKIP: $t (QEMU not installed)"
        ((++total_fail))
        continue
    fi

    if ! run_tests "$t"; then
        ((++total_fail))
    fi
    echo ""
done

if [[ $total_fail -eq 0 ]]; then
    echo "All QEMU user-mode tests passed!"
    exit 0
else
    echo "$total_fail target(s) failed."
    exit 1
fi
