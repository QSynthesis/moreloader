# 项目状态

## 现有内容

| 部分 | 状态 |
|---|---|
| 构建环境 | 32 位工具链文件、无管理员权限的 i386 overlay、32 位 qmsetup 与 Boost.Test，见 [`20260928-repo-setup.md`](claude/20260928-repo-setup.md) |
| 指导文档 | `CLAUDE.md`、[`Development.md`](Development.md)、[`ReverseGuide.md`](ReverseGuide.md)、[`TaskSpec.md`](TaskSpec.md) |
| 逆向 | Ghidra 工程已建立，bridge 可用；导入函数的调用点、文件打开模式与 `sinh` 的调用点已统计；OpenMP 的线程数设置与分析的并行区域已定位并在 Ghidra 中命名，证据记录见 [`20260929-openmp-evidence.md`](claude/20260929-openmp-evidence.md) |
| msvcrt 实测 | 探针 `moreloader/tests/probe/msvcrt` 与黄金数据，结论见 [`20260928-msvcrt-measurements.md`](claude/20260928-msvcrt-measurements.md) |
| `MoreLoaderSupport` | `CodePage`（Windows 规则的 UTF-8 编解码与缓冲区不足时的部分写入）、`CommandLine`（`CommandLineToArgvW` 规则）、`PathMapping`（`Z:` 映射、`GetFullPathNameA` 规则、命令行参数的路径规则）、`CharacterType`（CT_CTYPE1、大小写、`NORM_IGNORECASE` 比较）、`Diagnostics`、`FloatingPoint`，均有测试 |
| `MoreLoaderCRT` | msvcrt 的纯计算部分：printf 引擎、`qsort`、`rand`、`strtol` 族与 `atof`、字符分类、`strerror` 与 `asctime`、导入的数学函数，均与黄金数据比对；`--check-heap` 的堆保护区 |
| `MoreLoaderImage` | PE 解析与固定基址映射，尚无自动测试 |
| `MoreLoaderRuntime` | 导出注册表、TEB 与 FS 段（`set_thread_area`）、客体线程、内核对象与等待、进程启动与退出、桩代码与故障报告；已在 WSL、FEX、qemu-i386 下运行完整工程，尚无自动测试 |
| `MoreLoaderWinAPI` 与 CRT 的导出包装 | moresampler 与 resampler.exe 所需的 kernel32 与 msvcrt 函数，见 [`20260928-loader-first-run.md`](claude/20260928-loader-first-run.md)、[`20260929-resampler.md`](claude/20260929-resampler.md) |
| 手动测试 | `moreloader/tests/manual/compare`（与 Windows 的逐字节比较）、`moreloader/tests/manual/x87`（x87 探针、记录的重放、参数约化的模型） |

## 比较的前提

moresampler 的输出随进程可用的处理器数而变，Windows 上同样如此（8 个与 16 个处理器的结果从第 3 步起不同）。以下「一致」均指两侧处理器数相同时的比较，见 [`TaskSpec.md`](TaskSpec.md) 第 6 节第 7 条与 [`20260929-render-comparison.md`](claude/20260929-render-comparison.md) 第 7.3 节。

## 里程碑 1（Linux x86_64）

验收标准见 [`TaskSpec.md`](TaskSpec.md) 第 8 节。

- [x] 子库骨架与构建验证
- [x] Windows 探针：`msvcrt.dll` 的 printf、`qsort`、`rand`、文件模式、数学函数
- [x] kernel32 与 msvcrt 的导出包装
- [x] 驱动运行到 `main`
- [x] 频率表生成的比较（`desc.mrq`，排除时间戳，与 `.llsm`）：逐字节一致，见 [`20260929-first-comparison.md`](claude/20260929-first-comparison.md)
- [x] 单个音符的渲染（resampler 模式）：逐字节一致
- [x] wavtool 模式的比较：`compare.py` 的 2 次串接与完整工程中的 106 次 wavtool 全部逐字节一致
- [x] 真实工程的完整渲染：helloutau 生成成对的批处理与 shell 脚本，逐步比较快照。修正以 `/` 开头的音高曲线被当作路径的缺陷后，`moreconfig.txt` 的四种组合（兼容开关 × 多线程开关）的 203 步与最终 wav 全部一致，`desc.mrq` 只有时间戳不同，见 [`20260929-render-comparison.md`](claude/20260929-render-comparison.md)
- [x] Windows 与 WSL 都限定 8 个处理器时，两种组合的完整工程同样全部一致
- [x] `0x433836` 处 `sinh` 的 80 位结果：不需要按 msvcrt 的 x87 算法复现，全部比较中未出现由它引起的差异
- [x] README 的构建、用法与许可证说明（中文）

## UTAU 自带的 `resampler.exe`

作者 2026-09-29 决定支持。

- [x] kernel32 的文件句柄、堆、环境变量、代码页与字符分类、`_stat` 所需的函数，按实测实现，见 [`20260929-resampler.md`](claude/20260929-resampler.md)
- [x] 4 次渲染（含复用 `.frq`、flags、调制、拉伸）的 11 个文件与 Windows 逐字节一致，WSL、FEX 与 qemu-i386 均如此
- UTAU 自带的 `wavtool.exe` 不支持（作者 2026-09-29 决定）。

## 里程碑 2（Linux ARM64，FEX-Emu）

- 环境：`ssh spark`（aarch64，20 个逻辑处理器），FEX 位于 `/home/functioner/Documents/rover2024/FEX/build/RelWithDebInfo/Bin/FEX`。
- [x] 定位 FEX 下的 SIGILL：FEX 对 32 位客体的 `modify_ldt` 是中止进程的桩。FS 段改用 `set_thread_area` 的 GDT TLS 项后，频率表生成、resampler 与 wavtool 的 13 个文件与 Windows 逐字节一致，多线程合成同样一致，见 [`20260929-fex.md`](claude/20260929-fex.md)
- [x] 真实工程：以 `taskset -c 0-15` 使处理器数与 Windows 相同后，四种组合的 203 步与最终 wav 全部一致，每种组合耗时 314 至 508 秒。不限定时从第 24 或 60 步起不同，原因为处理器数（20 个），不是指令的模拟
- box64 未用于 ARM64 的验证；box64 在 RISC-V 上的结果见下节。

## RISC-V（qemu-i386）

环境：`ssh dp1000`（riscv64，8 个逻辑处理器），见 [`20260929-riscv.md`](claude/20260929-riscv.md)。

- [x] box64（BOX32）：需要动态链接的加载器（`MORE_STATIC=OFF`）与 `BOX64_DYNAREC_FASTROUND=0`；resampler 全部一致，moresampler 因 box64 以 double 模拟 x87 而大量不同，差异源已定位到指令。不采用
- [x] qemu-i386：静态链接的加载器可直接运行；未打补丁时 moresampler 仅 `bam.wav` 的两个文件不同，原因为 `fsin`、`fcos`、`fsincos`、`fptan` 以 double 计算
- [x] qemu-i386 加 binary128 超越函数的补丁：作者决定采用，补丁以 v11.1.2 为基线保存在 [`third-party/qemu/`](../third-party/qemu/README.md)
- [x] 补丁按处理器的做法以舍入到 66 位的 π/2 做参数约化：与 AMD、Intel 的 `fsin`、`fcos`、`fptan` 只有 ±1 ulp 的差异（两种处理器之间同样如此），x87 探针与处理器逐位相同，两个程序的 `compare.py` 全部逐字节一致，见 [`20260929-riscv.md`](claude/20260929-riscv.md) 第 8 节
- [x] 真实工程（兼容关、多线程开）：与限定 8 个处理器的 Windows 结果全部一致
- 补丁不提交给 QEMU 上游，只保存在本仓库（作者 2026-09-29 决定）。
- qemu-i386 的速度：完整工程约 6800 秒，WSL 原生约 20 秒、FEX 约 300 至 500 秒。作者接受，不作优化（2026-09-29）。

## 其他

- [x] 诊断选项 `--check-heap`（msvcrt 堆的越界检查）
- [x] 不提供指定处理器数的选项：加载器按主机进程可用的处理器数运行，与在同一台机器上运行 Windows 版相同（作者 2026-09-29 决定）
