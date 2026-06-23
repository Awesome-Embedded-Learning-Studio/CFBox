# projects/ — CFBox 集成 demo

这里挂的是 CFBox 的**参考集成场景**。**CFBox 自己的 CMake/CI 不依赖本目录** ——
各场景跑自己的 build flow;CFBox 这边只产出二进制喂给它们。两边解耦。

## imx-forge-demo

[imx-forge](https://github.com/Awesome-Embedded-Learning-Studio/imx-forge) 的子模块快照:
面向 NXP i.MX6ULL 的嵌入式 Linux 开发工坊,rootfs 阶段用 **BusyBox** 当 init + 工具集。

**本 demo 目的**:验证 CFBox 能否在 i.MX6ULL(armhf)rootfs 里**替代 BusyBox** —— 当
PID 1(init + inittab)+ shell + mdev + 核心工具,撑起 imx-forge 的 rootfs 启动闭环。
这是 CFBox「立得住 / 不是玩具」的真实锚定场景。

**hack 流程**:
1. CFBox 产 armhf static 二进制:
   `cmake -B build-armhf-static -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain/Toolchain-armhf.cmake -DCMAKE_BUILD_TYPE=Release -DCFBOX_OPTIMIZE_FOR_SIZE=ON -DCFBOX_STATIC_LINK=ON`
2. symlink 成 imx-forge rootfs 要的命令(`init sh cp ls mount mdev ... → cfbox`)
3. 塞进 `imx-forge-demo/rootfs/` 替换 busybox
4. qemu / 板子启动,看到哪挂 → 真实 gap = CFBox v1.0 优先级依据

> 子模块**未递归** imx-forge 的 `third_party`(linux/uboot),保持轻量。需要内核/uboot
> 源码时手动:`git -C projects/imx-forge-demo submodule update --init`。
