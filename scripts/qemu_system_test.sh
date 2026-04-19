#!/usr/bin/env bash
# Run CFBox under QEMU system-mode emulation with a custom initramfs.
# Usage: scripts/qemu_system_test.sh --arch <aarch64|armhf> --kernel <path> --initramfs <path>
set -euo pipefail

script_dir="$(cd "$(dirname "$0")" && pwd)"

# Defaults
arch=""
kernel=""
initramfs=""
timeout_secs=120

while [[ $# -gt 0 ]]; do
    case "$1" in
        --arch)       arch="$2";       shift 2 ;;
        --kernel)     kernel="$2";     shift 2 ;;
        --initramfs)  initramfs="$2";  shift 2 ;;
        --timeout)    timeout_secs="$2"; shift 2 ;;
        -h|--help)
            echo "Usage: $0 --arch <aarch64|armhf> --kernel <path> --initramfs <path> [--timeout <secs>]"
            echo ""
            echo "  --arch       Target architecture"
            echo "  --kernel     Path to kernel image (Image for aarch64, zImage for armhf)"
            echo "  --initramfs  Path to initramfs cpio archive"
            echo "  --timeout    Timeout in seconds (default: 120)"
            exit 0 ;;
        *) echo "Unknown option: $1"; exit 1 ;;
    esac
done

# Validate
if [[ -z "$arch" || -z "$kernel" || -z "$initramfs" ]]; then
    echo "ERROR: --arch, --kernel, and --initramfs are all required"
    exit 1
fi
if [[ ! -f "$kernel" ]]; then
    echo "ERROR: Kernel not found: $kernel"; exit 1
fi
if [[ ! -f "$initramfs" ]]; then
    echo "ERROR: Initramfs not found: $initramfs"; exit 1
fi

# Construct QEMU command based on architecture
case "$arch" in
    aarch64)
        qemu_cmd="qemu-system-aarch64"
        machine="virt"
        cpu="cortex-a57"
        dtb=""
        ;;
    armhf)
        qemu_cmd="qemu-system-arm"
        machine="vexpress-a9"
        cpu="cortex-a9"
        dtb="-dtb $(dirname "$kernel")/dts/arm/vexpress-v2p-ca9.dtb"
        ;;
    *)
        echo "ERROR: Unsupported arch: $arch (use aarch64 or armhf)"
        exit 1
        ;;
esac

if ! command -v "$qemu_cmd" &>/dev/null; then
    echo "ERROR: $qemu_cmd not found. Install: sudo pacman -S qemu-system-aarch64 qemu-system-arm"
    exit 1
fi

echo "=== QEMU System-Mode Test ($arch) ==="
echo "Kernel:    $kernel"
echo "Initramfs: $initramfs"
echo "Machine:   $machine, CPU: $cpu"
echo "Timeout:   ${timeout_secs}s"
echo ""

# Run QEMU and capture output
log_file="$(mktemp /tmp/cfbox-qemu-XXXXXX.log)"
trap 'rm -f "$log_file"' EXIT

echo "Booting..."
set +e
# Build QEMU command (dtb is optional, only set for armhf)
qemu_args=(
    -machine "$machine" -cpu "$cpu" -m 256M
    -nographic -no-reboot
    -kernel "$kernel" -initrd "$initramfs"
    -append "console=ttyAMA0 init=/init"
)
if [[ -n "$dtb" ]]; then
    qemu_args+=($dtb)
fi
timeout "$timeout_secs" "$qemu_cmd" "${qemu_args[@]}" \
    > "$log_file" 2>&1 &
qemu_pid=$!

# Stream output in real-time
tail -f "$log_file" &
tail_pid=$!

# Wait for QEMU to finish
wait "$qemu_pid" 2>/dev/null
qemu_exit=$?
kill "$tail_pid" 2>/dev/null
wait "$tail_pid" 2>/dev/null
set -e

echo ""

# Parse results
if grep -q "ALL TESTS COMPLETE" "$log_file"; then
    echo "=== Test Execution Finished ==="
    echo ""

    # Count passes and fails
    pass_count=$(grep -c "PASS:" "$log_file" || true)
    fail_count=$(grep -c "FAIL:" "$log_file" || true)

    echo "Results: $pass_count passed, $fail_count failed"
    echo ""

    # Show any failures in detail
    if [[ $fail_count -gt 0 ]]; then
        echo "Failed tests:"
        grep "FAIL:" "$log_file" || true
        echo ""
    fi

    if [[ $fail_count -eq 0 ]]; then
        echo "All system-level tests PASSED!"
        exit 0
    else
        echo "Some tests FAILED."
        exit 1
    fi
else
    echo "ERROR: Test completion marker not found in output."
    echo ""
    if [[ $qemu_exit -eq 124 ]]; then
        echo "QEMU timed out after ${timeout_secs}s."
    else
        echo "QEMU exited with code: $qemu_exit"
    fi
    echo ""
    echo "Last 30 lines of output:"
    tail -30 "$log_file"
    exit 1
fi
