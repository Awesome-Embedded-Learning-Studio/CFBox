#!/usr/bin/env bash
set -euo pipefail

# gen_size_table.sh — Generate the size-comparison markdown table for the README.
#
# Measures CFBox for real (size-opt native + armhf static when the toolchain is
# available); other projects are well-known reference values. Output goes to
# stdout — pipe into the README's "体积对比" / "Size comparison" section.
#
# Usage: scripts/gen_size_table.sh

cd "$(dirname "$0")/.."

# --- applet count from the registry (single source of truth) ---
applets=$(awk '/constexpr auto APPLET_REGISTRY/,/^}\)/' include/cfbox/applets.hpp | grep -c '{"')

# --- CFBox size-opt (native, dynamic) ---
cmake --no-warn-unused-cli -B build-size -DCMAKE_BUILD_TYPE=Release -DCFBOX_OPTIMIZE_FOR_SIZE=ON >/dev/null
cmake --build build-size -j"$(nproc)" >/dev/null
strip --strip-unneeded build-size/cfbox 2>/dev/null || true
size_opt_bytes=$(stat -c %s build-size/cfbox)
size_opt_kb=$(( size_opt_bytes / 1024 ))
per_applet=$(awk "BEGIN{ printf \"%.1f\", ${size_opt_kb} / ${applets} }")

# --- CFBox armhf static (optional — needs the arm toolchain on PATH) ---
armhf_row=""
if command -v arm-none-linux-gnueabihf-gcc >/dev/null 2>&1; then
    cmake --no-warn-unused-cli -B build-armhf-static \
        -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain/Toolchain-armhf.cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DCFBOX_OPTIMIZE_FOR_SIZE=ON \
        -DCFBOX_STATIC_LINK=ON >/dev/null
    cmake --build build-armhf-static -j"$(nproc)" >/dev/null
    arm-none-linux-gnueabihf-strip --strip-unneeded build-armhf-static/cfbox 2>/dev/null || true
    armhf_bytes=$(stat -c %s build-armhf-static/cfbox)
    armhf_mb=$(awk "BEGIN{ printf \"%.1f\", ${armhf_bytes} / 1048576 }")
    armhf_row="| CFBox (armhf static) | C++23 | ~${armhf_mb} MB | ${applets} | — |"
fi

# --- reference values for other projects (well-known, not measured here) ---
cat <<EOF
| 项目 | 语言 | 体积 | Applets | 体积/Applet |
|------|------|------|---------|-------------|
| **CFBox (size-opt)** | **C++23** | **${size_opt_kb} KB** | **${applets}** | **~${per_applet} KB** |
EOF
[ -n "$armhf_row" ] && echo "$armhf_row"
cat <<EOF
| Toybox | C | ~500 KB | 238 | ~2.1 KB |
| BusyBox (full) | C | ~1.7 MB | 274 | ~9 KB |
| uutils/coreutils | Rust | ~11 MB | ~100 | ~110 KB |
EOF
