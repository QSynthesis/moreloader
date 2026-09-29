# 2026-09-28 msvcrt.dll 的实测与 Support、CRT 的纯计算部分

## 1. 探针

做了什么：`moreloader/tests/probe/msvcrt/MsvcrtProbe.cpp` 是以 MSVC 2022 编译的 32 位程序，以 `LoadLibrary("msvcrt.dll")` 与 `GetProcAddress` 调用系统的 msvcrt.dll，把结果写入 `moreloader/tests/auto/data/msvcrt/`。`Build.ps1` 编译并运行探针。自动测试读取这些数据，逐项比较包装层的实现。

测量环境：Windows 11（26100），`C:\WINDOWS\SysWOW64\msvcrt.dll` 7.0.26100.8875，ANSI 代码页 936。版本记录在 `source.txt`。

依据类别：以下结论除特别注明外均为实测。

## 2. 结论

### printf 族（`printf.txt`、`snprintf.txt`）

- 浮点数先取 17 位有效数字，再在所需位置对数字串逢五进位（`_fptostr` 的做法）。因此 `%.0f` 的 2.5 为 `3`，`%.2f` 的 0.125 为 `0.13`，与 glibc 的「逢中取偶」不同。
- 17 位有效数字本身是正确舍入的结果，但**恰好位于两个 17 位数正中的值一律舍去**：构造的 120 个中点值全部如此，与末位奇偶无关（`test_Format` 的数据中含这些值）。
- 指数至少三位（`e+000`）。零的「小数点位置」为 0，使 `%#g` 的零为 `0.000000`，而 `%e` 的零指数固定为 0。
- 无穷大与 NaN 的数字串为 `1#INF`、`1#QNAN`、`1#SNAN`、`1#IND`（仅 `0xFFF8000000000000`），与普通数字串同样参与舍入，因此 `%.2f` 的无穷大为 `1.#J`，`%.1f` 为 `1.$`。符号取符号位。
- `%p` 为 8 位大写十六进制，`%#p` 的前缀为大写 `0X`；`%a` 的默认精度为 6；`%5%` 不填充；非法转换字符原样输出且不消耗参数；`%n` 使整个调用返回 -1。
- 窄字符格式中的宽字符参数按 C locale 逐字转换，大于 0xFF 的字符使 `%S` 失败（返回 -1，失败前已写出的填充保留），使 `%C` 不输出且不算失败。宽字符格式中的窄字符参数逐字节扩展。
- `_vsnprintf`：长度小于 count 时写终止符并返回长度，等于 count 时不写终止符并返回长度，大于 count 时写 count 个字符并返回 -1。

### 其他 CRT 函数

- `qsort`：VS2005 以来 CRT 的快速排序（三数取中，8 个元素以下用选择排序，pivot 随交换移动）。80 组数据的排序结果与比较调用序列全部一致（`qsort.txt`）。
- `rand`：`seed = seed × 214013 + 2531011`，返回 `(seed >> 16) & 0x7FFF`；新线程的初始种子为 1（`rand.txt`）。
- `strerror`、`asctime`（日期补零，如 `Jan 02`）、`_ultoa`、`_stricmp`（按小写比较，只返回 ±1）、C locale 的字符分类（只分类 ASCII）均已按实测实现。
- `atof` 不识别十六进制、`inf`、`nan`；无数字时返回 +0（包括 `-INF`）。两个极端输入的舍入与正确舍入不同，未复现（见 `Numbers.h`）。
- `strtol` 族：`0x` 后无十六进制数字时结束位置回到开头；`strtoul` 溢出后仍应用负号，`"-2147483649"`（16 进制）得 1。
- `_mbslen` 按 ACP 的双字节规则计数（UTF-8 的「你好」计为 3），取决于测量机器的代码页，尚未决定模拟方式。

### 文件模式（`fopen.txt`）

- **`_wfopen(L"rw")` 返回 NULL，errno 为 22**；窄字符的 `fopen("rw")` 则按 `r` 打开。moresampler 在 `0x40d5b5` 以 `_wfopen` 和 `"rw"` 打开文件（静态分析），因此在 Windows 上这次打开总是失败，包装层必须同样失败。
- 文本模式读取把 `\r\n` 变为 `\n`，孤立的 `\r` 保留，`0x1A` 结束读取；写入把 `\n` 变为 `\r\n`。
- 文本模式下 `ftell` 的结果与原始偏移不一致（`fgets` 读完 `a\n` 后为 4，原始偏移为 3）。moresampler 的文本模式读取只出现在推断属于 Lua 的代码中，暂不模拟。

### Windows 函数

- `CommandLineToArgvW` 的 19 个用例与 shell32 的三引号规则一致（`cmdline.txt`）。
- 新线程的浮点状态（`threads.txt`，2026-09-29 补测）：创建者的控制字为 `0x37F` 或 `0x07F` 时，`CreateThread` 与 msvcrt.dll 的 `_beginthreadex` 创建的新线程控制字均为 `0x27F`、MXCSR 均为 `0x1F80`，不继承创建者。moresampler 的输出随处理器数变化即源于此，见 [`20260929-openmp-evidence.md`](20260929-openmp-evidence.md)。测试为 `tests/auto/Support/test_FloatingPoint.cpp`，把 `windowsControlWord` 改为 `0x37F` 时失败。
- `MultiByteToWideChar(CP_UTF8)` 对非法序列的替换**不是** Unicode 的最大子部分：第二字节为续字节但超出首字节允许的范围时，首字节与该字节合为一个 U+FFFD。stdcorelib 按最大子部分替换，因此解码方向在 `Support/CodePage` 中自行实现（作者决定保留在 moreloader 内）。

### 数学函数（`math.txt`、`math-st0.txt`）

- **msvcrt 的数学函数经 `ST0` 返回未舍入的 80 位值**（`tan(0.5)` 的尾数低位为 `…5219`）。moresampler 在返回后继续在 x87 栈上计算，因此包装函数返回 `long double`。
- 在控制字 `0x27F` 下：`acos`、`asin`、`log10` 的 glibc double 版本与 msvcrt 舍入后一致；`tan`、`tanh`、`sinh`、`cosh` 的 glibc double 版本有大量末位差异。
- 以 64 位精度计算 `sinhl`、`coshl`、`tanhl` 后舍入为 double，818 个参数全部一致；`tan` 用 `fptan`，除 |x| ≥ 2⁶³ 外一致；`log10l` 与 `fptan` 在 80 位层面也一致。
- **`sinh`、`cosh`、`tanh`、`acos` 的 80 位结果与 glibc 不同**（例如 `sinh` 只有 189/820 一致）。msvcrt 由 `0x100a62c1` 的超越函数分派器按描述符（`0x100bcc0a` 起，含名称 `sinh`、`cosh`、`tanh`）选择 x87 算法（静态分析）。
- 静态分析：除推断属于 Lua 的 `0x4746xx`–`0x474dxx` 外，moresampler 只在 `0x433836` 调用 `sinh`，参数为 k/6（`0x4b9488` 处的 double 为 1/6），推断为 Bark 尺度的逆变换；结果在 x87 栈上乘以常数后存为 float。k/6（k = 0…400）的 401 个参数上，扩展精度实现舍入为 double 后与 msvcrt 全部一致，但 80 位值不完全一致。

## 3. 问题与处理

- **按值传递 double 会使 signaling NaN 变为 quiet NaN**：i386 上 `double` 参数经 x87 的 `fld`，处理器加载 sNaN 时将其静默。printf 引擎因此全程按位处理客体的 double（`GuestArguments` 不提供返回 `double` 的函数）。包装层中所有客体的 double 同样须按位转交，直到确实需要计算为止。
- **不区分大小写的 NTFS**：名为 `Strings.h` 的头文件被 glibc 的 `<string.h>` 当作 `<strings.h>` 包含，已改名为 `StringFunctions.h`，并写入 `CLAUDE.md`「已知问题」。
- **新增测试未被编译**：测试目录的 `file(GLOB_RECURSE)` 缺少 `CONFIGURE_DEPENDS`，新文件在重新配置之前不参与构建，ctest 仍报告通过。已为所有 glob 加上 `CONFIGURE_DEPENDS`，并逐项确认 11 个测试套件均出现在报告中。
- 一次头文件损坏使构建失败，而旧的测试程序仍被 ctest 运行并报告通过。构建脚本改为构建失败即停止。

## 4. 现状

`test_auto` 包含 11 个测试套件，全部通过：Support 的 `CodePage`、`CommandLine`、`PathMapping`；CRT 的 `Format`、`Sort`、`Random`、`StringFunctions`、`CharacterClass`、`Numbers`、`MathFunctions`。放回缺陷的检验已对 `CommandLine`、`CodePage`、`Format` 进行。

## 5. 下一步

1. 映像装载（`Image`）、TEB/FS/TLS 与线程（`Runtime`），驱动运行到 `main`。
2. kernel32 与 msvcrt 的导出包装。
3. 端到端比较。若输出因 `0x433836` 处 `sinh` 的 80 位结果不同而不一致，则按 msvcrt 分派器的 x87 指令序列复现 `sinh`。
