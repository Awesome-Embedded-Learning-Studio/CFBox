#!/usr/bin/env bash
# Boot CFBox as PID 1 under qemu-system (armhf vexpress-a9) and drop into an
# interactive cfbox shell on the serial console. Loopback is brought up at boot
# so ping/traceroute/ifconfig-write work immediately.
#
# Interactive, NOT for CI — meant for hands-on verification of the privileged
# network applets (ifconfig write / ping / traceroute / netstat) that the CI
# native + qemu-user stages can't exercise (CAP_NET_ADMIN / CAP_NET_RAW).
#
# Usage: scripts/user_test/qemu_cfbox_shell.sh
#        CFBOX_BIN=... KERNEL=... DTB=... scripts/user_test/qemu_cfbox_shell.sh
#
# Overrides (env):
#   CFBOX_BIN  path to static armhf cfbox (default build-armhf/cfbox)
#   KERNEL     zImage path (default third_party/linux/arch/arm/boot/zImage)
#   DTB        vexpress dtb path (default .../dts/arm/vexpress-v2p-ca9.dtb)
#   MEM        qemu RAM (default 256M)
#
# Exit the guest:  poweroff -f        (clean)
# Kill qemu:       Ctrl-A then X
set -euo pipefail

script_dir="$(cd "$(dirname "$0")" && pwd)"
project_dir="$(cd "$script_dir/../.." && pwd)"
cd "$project_dir"

CFBOX_BIN="${CFBOX_BIN:-$project_dir/build-armhf/cfbox}"
KERNEL="${KERNEL:-$project_dir/third_party/linux/arch/arm/boot/zImage}"
DTB="${DTB:-$project_dir/third_party/linux/arch/arm/boot/dts/arm/vexpress-v2p-ca9.dtb}"
MEM="${MEM:-256M}"
INITRAMFS="${INITRAMFS:-$project_dir/build/armhf-shell-initramfs.cpio}"

# --- dependency checks -------------------------------------------------------
need() { command -v "$1" >/dev/null 2>&1 || { echo "ERROR: missing '$1'"; exit 1; }; }
need qemu-system-arm
need qemu-arm-static      # to enumerate applets when building symlinks
need cpio

if [[ ! -x "$CFBOX_BIN" ]]; then
    echo "ERROR: cfbox binary not found: $CFBOX_BIN"
    echo "Build it first:"
    echo "  cmake -B build-armhf \\"
    echo "    -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain/Toolchain-armhf.cmake \\"
    echo "    -DCMAKE_BUILD_TYPE=Release -DCFBOX_OPTIMIZE_FOR_SIZE=ON -DCFBOX_STATIC_LINK=ON"
    echo "  cmake --build build-armhf -j\$(nproc)"
    exit 1
fi
[[ -f "$KERNEL" ]] || { echo "ERROR: kernel missing: $KERNEL"; exit 1; }
[[ -f "$DTB" ]]    || { echo "ERROR: dtb missing: $DTB"; exit 1; }

# --- build initramfs (cfbox + applet symlinks + inittab) ---------------------
rootfs="$(mktemp -d /tmp/cfbox-shell-rootfs-XXXX)"
mkdir -p "$rootfs"/{bin,etc,proc,sys,dev,tmp,sbin,usr/bin,usr/sbin}
cp "$CFBOX_BIN" "$rootfs/bin/cfbox"

# applet symlinks via cfbox --list (cross-binary → qemu-arm-static)
qemu-arm-static "$rootfs/bin/cfbox" --list | awk '{print $1}' | while read -r n; do
    [[ -z "$n" ]] && continue
    ln -sf cfbox "$rootfs/bin/$n"
done
# seed common PATH dirs so lookups like /sbin/poweroff resolve regardless of PATH
for sub in sbin usr/bin usr/sbin; do
    for name in sh ifconfig ip route hostname netstat ping traceroute mount poweroff reboot uname echo; do
        ln -sf ../../bin/cfbox "$rootfs/$sub/$name" 2>/dev/null || ln -sf ../bin/cfbox "$rootfs/$sub/$name"
    done
done

# inittab: mount fs, bring up loopback (so ping 127.0.0.1 works immediately),
# then askfirst → /bin/sh on the console. askfirst waits for Enter before
# launching the shell so boot logs aren't drowned by a prompt.
cat > "$rootfs/etc/inittab" <<'EOF'
::sysinit:/bin/mount -t proc proc /proc
::sysinit:/bin/mount -t sysfs sysfs /sys
::sysinit:/bin/mount -t devtmpfs devtmpfs /dev
::sysinit:/bin/ifconfig lo up
::askfirst:/bin/sh
::ctrlaltdel:/sbin/reboot -f
EOF
ln -sf bin/cfbox "$rootfs/init"

mkdir -p "$(dirname "$INITRAMFS")"
(cd "$rootfs" && find . | cpio -o -H newc --quiet > "$INITRAMFS")
rm -rf "$rootfs"

# --- launch ------------------------------------------------------------------
cat <<EOF
=== cfbox qemu shell ready (armhf vexpress-a9) ===
Kernel:    $KERNEL
Initramfs: $INITRAMFS
Console:   ttyAMA0 (this terminal)

At the "press Enter" prompt, hit Enter to drop into the cfbox shell.
lo is already UP with 127.0.0.1 — try:
    ifconfig              # lo UP, eth0 present
    ip addr show
    route -n
    netstat -tln
    ping -c 3 127.0.0.1   # CAP_NET_RAW — works as root
    traceroute -n 127.0.0.1
    ifconfig lo mtu 1280  # CAP_NET_ADMIN write path, then restore to 65536

Exit:     poweroff -f          (clean shutdown)
Kill:     Ctrl-A then X
===================================================
EOF

# exec replaces this process with qemu — the console is wired straight to the
# caller's terminal, so typing in this shell drives the guest directly.
exec qemu-system-arm \
    -machine vexpress-a9 -cpu cortex-a9 -m "$MEM" \
    -nographic -no-reboot \
    -kernel "$KERNEL" -initrd "$INITRAMFS" -dtb "$DTB" \
    -append "console=ttyAMA0 init=/init"
