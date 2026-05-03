#!/usr/bin/env bash
# Build initramfs with /etc/inittab for full init system testing.
# Usage: scripts/build_initramfs_inittab.sh --arch aarch64 --cfbox <path> [--inittab <path>]
set -euo pipefail

script_dir="$(cd "$(dirname "$0")" && pwd)"
project_dir="$(cd "$script_dir/.." && pwd)"

arch=""
cfbox_path=""
inittab_path="$project_dir/configs/qemu-inittab"
output_path=""

while [[ $# -gt 0 ]]; do
    case "$1" in
        --arch)     arch="$2";          shift 2 ;;
        --cfbox)    cfbox_path="$2";    shift 2 ;;
        --inittab)  inittab_path="$2";  shift 2 ;;
        --output)   output_path="$2";   shift 2 ;;
        *) echo "Unknown option: $1"; exit 1 ;;
    esac
done

[[ -z "$arch" ]]     && { echo "ERROR: --arch required"; exit 1; }
[[ -z "$cfbox_path" ]] && { echo "ERROR: --cfbox required"; exit 1; }

if [[ -z "$output_path" ]]; then
    output_path="$project_dir/build/${arch}-initramfs-inittab.cpio"
fi
mkdir -p "$(dirname "$output_path")"

echo "=== Building initramfs with inittab ($arch) ==="

initramfs="$(mktemp -d)"
trap 'rm -rf "$initramfs"' EXIT

# Directory structure
mkdir -p "$initramfs"/{bin,dev,proc,sys,tmp,etc,root}

# CFBox binary + symlinks
cp "$cfbox_path" "$initramfs/bin/cfbox"
chmod +x "$initramfs/bin/cfbox"

# Create applet symlinks
case "$arch" in
    aarch64) qemu_bin="qemu-aarch64-static" ;;
    armhf)   qemu_bin="qemu-arm-static" ;;
    *)       qemu_bin="qemu-$arch-static" ;;
esac

if "$initramfs/bin/cfbox" --list >/dev/null 2>&1; then
    list_cmd="$initramfs/bin/cfbox"
elif command -v "$qemu_bin" &>/dev/null; then
    list_cmd="$qemu_bin $initramfs/bin/cfbox"
else
    echo "ERROR: $qemu_bin needed"; exit 1
fi

while IFS= read -r line; do
    name=$(echo "$line" | awk '{print $1}')
    [[ -z "$name" ]] && continue
    ln -sf cfbox "$initramfs/bin/$name"
done < <($list_cmd --list)

# /etc/inittab
if [[ -f "$inittab_path" ]]; then
    cp "$inittab_path" "$initramfs/etc/inittab"
    echo "  /etc/inittab installed"
else
    echo "WARNING: inittab not found: $inittab_path"
fi

# /init symlink
ln -sf bin/cfbox "$initramfs/init"

# Build cpio
(cd "$initramfs" && find . | cpio -o -H newc --quiet > "$output_path")

echo ""
echo "Initramfs: $output_path ($(du -h "$output_path" | cut -f1))"
echo ""
echo "Boot with:"
echo "  qemu-system-aarch64 -machine virt -cpu cortex-a57 -m 256M -nographic \\"
echo "    -kernel <Image> -initrd $output_path \\"
echo "    -append 'console=ttyAMA0'"
