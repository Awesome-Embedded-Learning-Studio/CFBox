#!/usr/bin/env bash
# Build a minimal initramfs cpio archive containing ONLY CFBox.
# CFBox's built-in 'init' applet acts as PID 1 — no external shell or busybox needed.
# Usage: scripts/build_initramfs.sh --arch <aarch64|armhf> --cfbox <path> [--output <path>]
set -euo pipefail

script_dir="$(cd "$(dirname "$0")" && pwd)"
project_dir="$(cd "$script_dir/.." && pwd)"

# Defaults
arch=""
cfbox_path=""
output_path=""

while [[ $# -gt 0 ]]; do
    case "$1" in
        --arch)     arch="$2";          shift 2 ;;
        --cfbox)    cfbox_path="$2";    shift 2 ;;
        --output)   output_path="$2";   shift 2 ;;
        -h|--help)
            echo "Usage: $0 --arch <aarch64|armhf> --cfbox <path> [--output <path>]"
            echo ""
            echo "  --arch      Target architecture (aarch64 or armhf)"
            echo "  --cfbox     Path to CFBox static binary (must include init applet)"
            echo "  --output    Output cpio path (default: build/<arch>-initramfs.cpio)"
            exit 0 ;;
        *) echo "Unknown option: $1"; exit 1 ;;
    esac
done

# Validate arguments
if [[ -z "$arch" ]]; then
    echo "ERROR: --arch is required"; exit 1
fi
if [[ -z "$cfbox_path" ]]; then
    echo "ERROR: --cfbox is required"; exit 1
fi
if [[ ! -x "$cfbox_path" ]]; then
    echo "ERROR: CFBox binary not found or not executable: $cfbox_path"; exit 1
fi

if [[ -z "$output_path" ]]; then
    output_path="$project_dir/build/${arch}-initramfs.cpio"
fi
mkdir -p "$(dirname "$output_path")"
output_path="$(cd "$(dirname "$output_path")" && pwd)/$(basename "$output_path")"

echo "=== Building initramfs for $arch (CFBox only) ==="
echo "CFBox:  $cfbox_path"
echo "Output: $output_path"
echo ""

# Create initramfs directory structure
initramfs="$(mktemp -d)"
trap 'rm -rf "$initramfs"' EXIT

mkdir -p "$initramfs"/{bin,dev,proc,sys,tmp}

# Copy CFBox binary
cp "$cfbox_path" "$initramfs/bin/cfbox"
chmod +x "$initramfs/bin/cfbox"

# Create applet symlinks (includes init -> cfbox)
# gen_links.sh runs `cfbox --list` to discover applets, so for cross-compiled
# binaries we need a QEMU wrapper to execute it. But symlinks must point to
# the actual cfbox binary, not the wrapper.
case "$arch" in
    aarch64) qemu_bin="qemu-aarch64-static" ;;
    armhf)   qemu_bin="qemu-arm-static" ;;
    *)       qemu_bin="qemu-$arch-static" ;;
esac

# Determine how to run the cross-compiled cfbox --list
# Symlinks must use relative paths (cfbox, not absolute) for the initramfs.
if "$initramfs/bin/cfbox" --list >/dev/null 2>&1; then
    list_cmd="$initramfs/bin/cfbox"
elif command -v "$qemu_bin" &>/dev/null; then
    list_cmd="$qemu_bin $initramfs/bin/cfbox"
else
    echo "ERROR: $qemu_bin needed to run cross-compiled cfbox --list"
    exit 1
fi

while IFS= read -r line; do
    name=$(echo "$line" | awk '{print $1}')
    [[ -z "$name" ]] && continue
    ln -sf cfbox "$initramfs/bin/$name"
    echo "  linked $name"
done < <($list_cmd --list)

# Create /init symlink — the kernel executes this first.
# CFBox's init applet will detect PID 1 and mount filesystems automatically.
ln -sf bin/cfbox "$initramfs/init"

# Build cpio archive
(cd "$initramfs" && find . | cpio -o -H newc --quiet > "$output_path")

echo ""
echo "Initramfs created: $output_path"
echo "Size: $(du -h "$output_path" | cut -f1)"
echo ""
echo "Contents:"
ls -la "$initramfs/bin/"
echo ""
echo "Boot with:"
if [[ "$arch" == "aarch64" ]]; then
    echo "  qemu-system-aarch64 -machine virt -cpu cortex-a57 -m 256M -nographic \\"
    echo "    -kernel <Image> -initrd $output_path \\"
    echo "    -append 'console=ttyAMA0'"
elif [[ "$arch" == "armhf" ]]; then
    echo "  qemu-system-arm -machine vexpress-a9 -cpu cortex-a9 -m 256M -nographic \\"
    echo "    -kernel <zImage> -initrd $output_path \\"
    echo "    -append 'console=ttyAMA0'"
fi
