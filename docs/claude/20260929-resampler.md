# 2026-09-29 UTAU 自带的 resampler.exe

## 1. 目标与来源

作者决定让加载器同时支持 UTAU 自带的 `resampler.exe`，与 moresampler 一样以逐字节一致为标准。

| 项 | 值 |
|---|---|
| 原件 | `E:\temp-repos\UTAU\UTAU\resampler.exe`（118784 字节，2012-12-19），只读 |
| 副本 | `work/resampler/resampler.exe`，比较脚本从这里复制 |
| 编译器 | Visual C++ 6，C 运行库静态链接（字符串 `Microsoft Visual C++ Runtime Library`、`__MSVCRT_HEAP_SELECT`） |
| 映像 | 基址 `0x400000`，没有重定位表，没有 TLS，控制台子系统 |
| 导入 | 只有 `KERNEL32.dll` 的 61 个函数，不导入 msvcrt |

同目录的 `resampler.dll` 是 `utau.exe` 的插件（导出 `exec`、`loadfrqdata` 等），`resampler.exe` 不加载它（静态分析：exe 中没有该文件名）。

与 moresampler 的区别：C 运行库的 stdio、堆、环境变量与多字节字符表都在 exe 内部，直接调用 Win32 API。加载器因此需要以 Win32 句柄实现的文件读写、堆，以及 VC6 运行库启动时使用的代码页与字符分类函数。

## 2. 实现方式

先以 `--trace-imports` 运行，未实现的导入被调用时加载器报出函数名并中止，逐个补齐，每一步的语义以实测或反汇编为准。

| 源文件 | 新增函数 |
|---|---|
| `Kernel32FileIO.cpp`（新） | `CreateFileA`、`ReadFile`、`WriteFile`、`SetFilePointer`、`SetEndOfFile`、`FlushFileBuffers`、`GetStdHandle`、`GetFileType`、`SetHandleCount` |
| `Kernel32Environment.cpp`（新） | `GetEnvironmentStrings`、`GetEnvironmentStringsW`、`FreeEnvironmentStringsA/W`、`GetEnvironmentVariableA` |
| `Kernel32Process.cpp` | `GetCommandLineA`、`GetModuleFileNameA`、`GetVersion`、`GetVersionExA`、`LoadLibraryA`、`FreeLibrary`、`ExitProcess` |
| `Kernel32Memory.cpp` | `HeapCreate`、`HeapDestroy`、`HeapAlloc`、`HeapFree`、`HeapReAlloc` |
| `Kernel32String.cpp` | `GetACP`、`GetOEMCP`、`GetCPInfo`、`GetStringTypeW`、`LCMapStringW`、`CompareStringW`、`lstrcpyA`；转换函数在缓冲区不足时的部分写入 |
| `Kernel32File.cpp` | `FindFirstFileA`、`FindClose`、`GetCurrentDirectoryA`、`GetFullPathNameA`、`GetDriveTypeA` |
| `Kernel32Time.cpp` | `FileTimeToSystemTime`、`FileTimeToLocalFileTime`、`GetTimeZoneInformation` |
| `Support/CharacterType`（新） | `CT_CTYPE1` 类型、大小写映射与 `NORM_IGNORECASE` 比较，与黄金数据比对 |
| `Support/CodePage` | `wholeCharacterPrefix`：`WideCharToMultiByte` 部分写入的长度 |
| `Support/PathMapping` | `fullGuestPath`：`GetFullPathNameA` 的规范化 |
| `Runtime/KernelObjects` | `FileObject` 增加所有权：`CreateFileA` 打开的描述符随句柄关闭 |

调用序列（实测，`--trace-imports`）：`GetVersion`、`HeapCreate`、`GetVersionExA`（VC6 的 `__heap_select`）、`GetStartupInfoA`、三次 `GetStdHandle` 加 `GetFileType`（`_ioinit`）、`GetCommandLineA`、`GetEnvironmentStringsW`、`GetACP`、`GetCPInfo`、`GetStringTypeW`、`LCMapStringW`（`_setmbcp`）、`GetModuleFileNameA`（`_setargv`）、若干次 `CompareStringW`（`getenv`），随后读取 WAV，`GetCurrentDirectoryA` 与 `GetFullPathNameA`，写出 `.frq` 与输出 WAV，`ExitProcess`。

## 3. 静态分析（反汇编 `work/resampler/resampler.dis`，由 `objdump` 生成）

| 地址 | 内容 |
|---|---|
| `0x413060` 起 | `_setmbcp`：代码页不在内置表中时调用 `GetCPInfo`；`MaxCharSize` 大于 1 且没有前导字节时把 1–254 标为尾字节，`__mblcid`（`0x420924`）由 `0x413251` 按代码页给出（非 932/936/949/950 时为 0） |
| `0x4132ad` | `setSBUpLow`：把字节 0–255（0 换成空格）以 `__crtGetStringTypeA` 分类，以 `__crtLCMapStringA` 求小写与大写，写入 `_mbctype`（`0x420820`）与 `_mbcasemap`（`0x420720`） |
| `0x414bfe` | `__crtGetStringTypeA`：先以 `GetStringTypeW(CT_CTYPE1, "", 1, …)` 探测，成功则用 W 版本：`MultiByteToWideChar(code_page, MB_PRECOMPOSED)` 后 `GetStringTypeW` |
| `0x414d47` | `__crtLCMapStringA`：`LCMapStringW` 后以 `WideCharToMultiByte` 写回 256 字节的缓冲区 |
| `0x412df1` | `__crtCompareStringA`：先以 `CompareStringW(0, 0, "", 1, "", 1)` 探测 |
| `0x40ff50` | `_mbsnbicoll`：以 `__mblcid`、`NORM_IGNORECASE` 与 `__mbcodepage` 调用 `__crtCompareStringA`，失败时返回 `0x7FFFFFFF` |
| `0x40b696` | `getenv`：在环境表中找到长度相同且其后为 `=` 的条目时，以 `_mbsnbicoll` 比较名字，只检验结果是否为 0 |
| `0x416697`、`0x41675d`、`0x4167e4`、`0x416a88` | 读取 `_mbcasemap` 的 `_mbs*` 大小写函数 |

## 4. 实测（探针 `moreloader/tests/probe/msvcrt`，黄金数据 `nls.txt`、`fullpath.txt`）

测量系统的 ACP 为 936（`source.txt`）。

- `MultiByteToWideChar(CP_UTF8)` 接受 `MB_PRECOMPOSED` 与 `MB_PRECOMPOSED | MB_ERR_INVALID_CHARS`，结果与 flags 为 0 时相同。文档称 UTF-8 只允许 0 与 `MB_ERR_INVALID_CHARS`，与实测不符。
- 缓冲区不足时：`MultiByteToWideChar` 写满缓冲区（代理对也会只写入前半）后返回 0 与 122；`WideCharToMultiByte` 只写入完整的字符后返回 0 与 122。此前的实现在失败时不写入，已按实测修正。
- `GetStringTypeW(CT_CTYPE1)` 与 `LCMapStringW` 对 U+0000–U+00FF 与 U+FFFD 的结果：U+FFFD 为 `C1_DEFINED`，大小写映射为自身；U+00FF 的大写为 U+0178；U+00AA、U+00B5、U+00BA 为小写字母兼标点，没有大写映射。
- `setSBUpLow` 的完整序列在 UTF-8 下：256 字节转换为 256 个码元（0x80 起每个字节为 U+FFFD）；`WideCharToMultiByte` 写入 254 字节（128 个 ASCII 与 42 个完整的 U+FFFD）后失败，最后 2 字节不写入。这两个字节对应的码元类型没有大小写标志，其 `_mbcasemap` 取 0，因此未写入的字节不影响结果。
- `CompareStringW(NORM_IGNORECASE)`，LCID 0 与 0x804 结果相同：没有 ASCII 字符被整体忽略；单字符之间只有字母的大小写相等；单字符顺序为控制字符、`'`、`-`、空白、标点、`+<=>`、数字、字母。`A-B` 与 `AB-` 不相等（连字符的权重另计）。flags 为 0 时，相同的串相等，`ab` 小于 `AB`。
- `GetFullPathNameA`：26 个输入，规则见 `PathMapping.h` 中 `fullGuestPath` 的说明。缓冲区不足时返回包含结束符的长度，`GetCurrentDirectoryA` 相同。

## 5. 设计决定与推断

以下各项不是实测结论，在代码注释中同样注明：

- **ANSI 代码页为 UTF-8**（`GetACP` 返回 65001）。主机的文件名与命令行是 UTF-8，与 moresampler 中 `CP_ACP` 按 UTF-8 处理一致。Windows 参考机的 ACP 为 936，两者只在含非 ASCII 字节的文件名与 `_mbs*` 函数上有差别，比较使用的文件名都是 ASCII。
- **`GetVersion` 返回 6.2，build 9200**：Windows 8 起对没有兼容性清单的程序报告的版本（公开资料）。VC6 的 `__heap_select` 因此选择系统堆，`VirtualAlloc` 不被调用（实测：比较中从未调用）。
- **堆**：所有堆共用主机的分配器，`HeapAlloc` 总是清零，使读取未初始化内存的客体每次运行结果相同。Windows 不清零，这一点与 Windows 不同，但只影响本身依赖未定义内容的程序。
- **`CompareStringW` 不相等时的顺序**：按实测的单字符顺序逐位比较，是对默认排序主权重的推断；只在两串都不含控制字符、`'`、`-` 时给出，否则报告并失败。唯一的调用方 `getenv` 只检验相等，失败即「不相等」，结果正确。
- **时区**：`GetTimeZoneInformation` 只报告当前的偏移，不含夏令时的切换日期；`FileTimeToLocalFileTime` 使用同一偏移（Windows 文档规定使用当前设置）。二者一致，VC6 的 `_stat` 由此得到的时间戳可以还原为 UTC。
- **驱动器**：`GetDriveTypeA` 只把 `Z:\` 报告为固定磁盘；非当前驱动器的相对路径按该驱动器的根解析，因为加载器不记录每个驱动器的当前目录。

## 6. 仍未实现的导入

以下 9 个导入不注册，被调用时加载器报告函数名并中止：

| 函数 | 调用点 | 不需要的理由 |
|---|---|---|
| `VirtualAlloc`、`VirtualFree` | 各 4 处 | VC6 小块堆，仅在 `__heap_select` 选择 SBH 时使用；系统版本为 NT 6.2 时选择系统堆（推断，依据 VC6 运行库的公开行为与实测中从未调用） |
| `RtlUnwind` | 1 处 | SEH 展开，仅在异常时使用 |
| `CompareStringA`、`GetStringTypeA`、`LCMapStringA` | 各 2 处 | W 版本的探测失败时的后备路径（静态分析：`0x412e2f`、`0x414c3d` 的探测） |
| `SetEnvironmentVariableA` | 2 处 | `_putenv` |
| `SetStdHandle` | 2 处 | 关闭或复制标准句柄 |
| `SetCurrentDirectoryA` | 1 处 | `_chdir` 或 `_chdrive` |

## 7. 比较

`moreloader/tests/manual/compare/compare.py` 增加 `--program resampler`：Windows 与 Linux 两侧各运行 4 次渲染，比较全部文件。

| 步骤 | 参数（音高、力度、flags、偏移、长度、辅音、截断、音量、调制、速度、音高曲线） |
|---|---|
| `ae.wav` 首次渲染，生成 `ae_wav.frq` | C4 100 "" 0 500 0 0 100 0 !120 AA#5# |
| `ae.wav` 再次渲染，读取已有的 `.frq` | 同上 |
| `baf.wav` | D4 150 g-5 20 600 50 30 80 50 !140 AA#3#ABAC |
| `bam.wav`（拉伸） | A3 50 "" 0 1500 100 0 120 100 !90 AA#10# |

结果（实测）：

| 环境 | 结果 |
|---|---|
| WSL，原生 | 4 个输出 WAV 与 3 个 `.frq` 等 11 个文件逐字节一致 |
| spark，FEX | 同样 11 个文件逐字节一致，单步 3 至 17 秒 |

- 单独运行 `ae.wav` 的首次渲染时，stdout（657 字节，Shift-JIS 的日文消息）与 stderr（2650 字节）也与 Windows 逐字节一致。
- 截断为负值（`-30`）时，`resampler.exe` 在 Windows 上以访问违例（`0xC0000005`）结束，在加载器中于同一位置（`eip 0x402cf9`，空指针）以 SIGSEGV 结束。这是 exe 自身的缺陷，两侧行为一致，只是退出码的形式不同，因为加载器不模拟 SEH。比较脚本因此不使用负的截断值。
- moresampler 的比较在这些修改之后仍然 13 个文件全部一致。

## 8. 测试

新增与修改的测试均与黄金数据比对，且逐项放回缺陷确认会失败（8 项全部检出）：`test_CharacterType`（类型与大小写 257 个码元、单字符顺序、字符串比较），`test_CodePage`（部分写入），`test_PathMapping`（26 个 `GetFullPathNameA` 用例）。`test_auto` 共 27 个用例、4716 个断言。

## 9. 下一步

1. 真实工程的完整渲染比较（helloutau 的脚本交付后），resampler 与 moresampler 两者都做。

UTAU 自带的 `wavtool.exe` 不支持（作者决定）。
