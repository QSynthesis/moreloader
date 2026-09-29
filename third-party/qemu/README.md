# QEMU 的 x87 超越函数补丁

在没有 x86 处理器的主机上（RISC-V），moreloader 以 qemu-i386（用户态）运行。`x87-binary128-transcendentals.patch` 使 qemu-i386 下 moresampler 与 UTAU 的 resampler.exe 的输出与 Windows 原生运行逐字节一致。

## 基线

| 项 | 值 |
|---|---|
| 仓库 | https://gitlab.com/qemu-project/qemu.git |
| tag | `v11.1.2` |
| 提交 | `4fc49f46dc95d4a27de2509e7fceb2931e91faeb`（2026-09-28） |
| 修改的文件 | `target/i386/tcg/fpu_helper.c` |
| 许可证 | QEMU 为 GPL-2.0，补丁包含其上下文行，按 QEMU 的许可证分发 |

## 内容

QEMU 的 `helper_fsin`、`helper_fcos`、`helper_fsincos`、`helper_fptan` 把 80 位的参数转换为 double，以主机的 `sin`、`cos`、`tan` 计算后转换回 80 位，参数与结果都只有 53 位。补丁把参数无损转换为主机的 binary128 `long double`，以 `sinl`、`cosl`、`tanl` 计算，再按 64 位尾数舍入回 80 位。参数范围的检查不变。补丁要求主机的 `long double` 为 binary128（riscv64 与 aarch64 的 Linux），否则编译失败。

依据与实测见 [`docs/claude/20260929-riscv.md`](../../docs/claude/20260929-riscv.md)。

## 构建

```sh
git clone --depth 1 --branch v11.1.2 https://gitlab.com/qemu-project/qemu.git qemu
git -C qemu apply /path/to/moreloader/third-party/qemu/x87-binary128-transcendentals.patch
mkdir qemu-build && cd qemu-build
../qemu/configure --target-list=i386-linux-user --disable-system --disable-docs --disable-tools --disable-werror
make -j$(nproc) qemu-i386
./qemu-i386 /path/to/moreloader <moresampler.exe> …
```

moreloader 使用静态链接的构建（默认），qemu-i386 不需要 x86 的 rootfs。
