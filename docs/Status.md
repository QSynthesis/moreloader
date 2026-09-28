# 项目状态

## 现有内容

| 部分 | 状态 |
|---|---|
| 构建环境 | 32 位工具链文件、无管理员权限的 i386 overlay、32 位 qmsetup、stdcorelib 与 Boost.Test，见 [`20260928-repo-setup.md`](20260928-repo-setup.md) |
| 指导文档 | `CLAUDE.md`、[`Development.md`](Development.md)、[`ReverseGuide.md`](ReverseGuide.md)、[`TaskSpec.md`](TaskSpec.md) |
| 逆向 | Ghidra 工程已建立，bridge 可用；导入函数的调用点、文件打开模式与 `sinh` 的调用点已统计 |
| msvcrt 实测 | 探针 `moreloader/tests/probe/msvcrt` 与黄金数据，结论见 [`20260928-msvcrt-measurements.md`](20260928-msvcrt-measurements.md) |
| `MoreLoaderSupport` | `CodePage`（Windows 规则的 UTF-8 解码）、`CommandLine`（`CommandLineToArgvW` 规则）、`PathMapping`（`Z:` 映射）、`Diagnostics`、`FloatingPoint`，均有测试 |
| `MoreLoaderCRT` | msvcrt 的纯计算部分：printf 引擎、`qsort`、`rand`、`strtol` 族与 `atof`、字符分类、`strerror` 与 `asctime`、导入的数学函数，均与黄金数据比对 |
| `MoreLoaderImage` | PE 解析与固定基址映射，尚无测试 |
| `MoreLoaderRuntime` | 导出注册表、TEB/LDT/FS、客体线程、内核对象与等待、进程启动与退出、桩代码与故障报告，尚未运行 |
| `MoreLoaderWinAPI` 与 CRT 的导出包装 | 已实现；不带参数运行 moresampler 的行为与 Windows 一致，见 [`20260928-loader-first-run.md`](20260928-loader-first-run.md) |

## 待办

里程碑 1（Linux x86_64，验收标准见 [`TaskSpec.md`](TaskSpec.md) 第 8 节）：

- [x] 子库骨架与构建验证
- [x] Windows 探针：`msvcrt.dll` 的 printf、`qsort`、`rand`、文件模式、数学函数
- [x] kernel32 与 msvcrt 的导出包装
- [x] 驱动运行到 `main`
- [ ] 频率表生成的比较（`desc.mrq`，排除时间戳，与 `.llsm`）
- [ ] 渲染的比较（resampler 模式）
- [ ] wavtool 模式的比较
- [ ] 若比较因 `0x433836` 处 `sinh` 的 80 位结果不一致，按 msvcrt 的 x87 算法复现 `sinh`
- [ ] README 的构建、用法与许可证说明

里程碑 2（Linux ARM64，经 FEX-Emu 或 box64）：尚未开始。
