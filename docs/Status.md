# 项目状态

## 现有内容

| 部分 | 状态 |
|---|---|
| 构建环境 | 32 位工具链文件、无管理员权限的 i386 overlay、32 位 qmsetup 与 Boost.Test，见 [`20260928-repo-setup.md`](claude/20260928-repo-setup.md) |
| 指导文档 | `CLAUDE.md`、[`Development.md`](Development.md)、[`ReverseGuide.md`](ReverseGuide.md)、[`TaskSpec.md`](TaskSpec.md) |
| 逆向 | Ghidra 工程已建立，bridge 可用；导入函数的调用点、文件打开模式与 `sinh` 的调用点已统计 |
| msvcrt 实测 | 探针 `moreloader/tests/probe/msvcrt` 与黄金数据，结论见 [`20260928-msvcrt-measurements.md`](claude/20260928-msvcrt-measurements.md) |
| `MoreLoaderSupport` | `CodePage`（Windows 规则的 UTF-8 解码）、`CommandLine`（`CommandLineToArgvW` 规则）、`PathMapping`（`Z:` 映射）、`Diagnostics`、`FloatingPoint`，均有测试 |
| `MoreLoaderCRT` | msvcrt 的纯计算部分：printf 引擎、`qsort`、`rand`、`strtol` 族与 `atof`、字符分类、`strerror` 与 `asctime`、导入的数学函数，均与黄金数据比对 |
| `MoreLoaderImage` | PE 解析与固定基址映射，尚无测试 |
| `MoreLoaderRuntime` | 导出注册表、TEB 与 FS 段、客体线程、内核对象与等待、进程启动与退出、桩代码与故障报告，尚未运行 |
| `MoreLoaderWinAPI` 与 CRT 的导出包装 | 已实现；不带参数运行 moresampler 的行为与 Windows 一致，见 [`20260928-loader-first-run.md`](claude/20260928-loader-first-run.md) |

## 待办

里程碑 1（Linux x86_64，验收标准见 [`TaskSpec.md`](TaskSpec.md) 第 8 节）：

- [x] 子库骨架与构建验证
- [x] Windows 探针：`msvcrt.dll` 的 printf、`qsort`、`rand`、文件模式、数学函数
- [x] kernel32 与 msvcrt 的导出包装
- [x] 驱动运行到 `main`
- [x] 频率表生成的比较（`desc.mrq`，排除时间戳，与 `.llsm`）：逐字节一致，见 [`20260929-first-comparison.md`](claude/20260929-first-comparison.md)
- [x] 单个音符的渲染（resampler 模式）：逐字节一致
- [x] 真实工程的完整渲染：helloutau 生成成对的批处理与 shell 脚本，逐步比较采样。修正以 `/` 开头的音高曲线被当作路径的缺陷后，203 步与最终 wav 全部一致，见 [`20260929-render-comparison.md`](claude/20260929-render-comparison.md)
- [ ] 渲染比较的其余三种 `moreconfig.txt` 组合，以及 FEX 与 qemu-i386 上的同一工程
- [ ] wavtool 模式的比较
- [ ] 若比较因 `0x433836` 处 `sinh` 的 80 位结果不一致，按 msvcrt 的 x87 算法复现 `sinh`
- [ ] README 的构建、用法与许可证说明

UTAU 自带的 `resampler.exe`（作者 2026-09-29 决定支持）：

- [x] kernel32 的文件句柄、堆、环境变量、代码页与字符分类、`_stat` 所需的函数，按实测实现，见 [`20260929-resampler.md`](claude/20260929-resampler.md)
- [x] 4 次渲染（含复用 `.frq`、flags、调制、拉伸）的 11 个文件与 Windows 逐字节一致，WSL 与 FEX 均如此
- UTAU 自带的 `wavtool.exe` 不支持（作者 2026-09-29 决定）。

里程碑 2（Linux ARM64，经 FEX-Emu 或 box64）：

- 环境：`ssh spark`（aarch64），FEX 位于 `/home/functioner/Documents/rover2024/FEX/build/RelWithDebInfo/Bin/FEX`。
- [x] 定位 FEX 下的 SIGILL：FEX 对 32 位客体的 `modify_ldt` 是中止进程的桩。FS 段改用 `set_thread_area` 的 GDT TLS 项后，频率表生成、resampler 与 wavtool 的 13 个文件与 Windows 逐字节一致，多线程合成同样一致，见 [`20260929-fex.md`](claude/20260929-fex.md)

RISC-V（`ssh dp1000`，riscv64），见 [`20260929-riscv.md`](claude/20260929-riscv.md)：

- [x] box64（BOX32）：需要动态链接的加载器（`MORE_STATIC=OFF`）与 `BOX64_DYNAREC_FASTROUND=0`；resampler 全部一致，moresampler 因 box64 以 double 模拟 x87 而大量不同，差异源已定位到指令
- [x] qemu-i386：静态链接的加载器可直接运行；moresampler 仅 `bam.wav` 的两个文件不同，原因为 `fsin`、`fcos`、`fsincos`、`fptan` 以 double 计算
- [x] qemu-i386 加 binary128 超越函数的补丁：两个程序全部逐字节一致。作者决定采用该方案，补丁以 v11.1.2 为基线保存在 [`third-party/qemu/`](../third-party/qemu/README.md)
- [x] 诊断选项 `--check-heap`（msvcrt 堆的越界检查）
