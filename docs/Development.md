# 代码编写原则

本文档规定 moreloader 仓库通用的代码组织与 C++ 编写原则。具体格式以仓库根目录的 `.clang-format` 为准。

本文档沿用 HelloUtau 的 `docs/Development.md`（`E:\GitHub\helloutau\docs\Development.md`）。两者冲突时以本文档为准，冲突之处在下文「与 HelloUtau 的差异」中逐条说明理由。

## 模块

共一个模块，是**一组库**而非单个库：

| 模块 | 命名空间 | 产出 | 依赖 |
|---|---|---|---|
| `moreloader/` | `more::loader` | 静态子库 `MoreLoaderSupport` 等，以及可执行文件 `moreloader` | C++17 标准库、glibc（i386，静态链接）、stdcorelib（私有） |

`more` 仅作为外层命名空间，代码一律位于第二层。不要在 `more` 中直接声明内容，也不要再增加第三层。

**例外：模拟 Windows 语义的代码位于第三层命名空间 `more::loader::msvcrt` 与 `more::loader::win32`。** 其中的名字与被模拟的函数一一对应（`msvcrt::quickSort` 模拟 `qsort`，`msvcrt::atof` 模拟 `atof`），第三层命名空间使这些名字不带 `msvcrt` 前缀，同时不与主机的同名函数混淆。在这两个命名空间内调用主机的 C 函数时一律加 `std::` 或全局限定，否则 `tan(x)` 会解析为 `msvcrt::tan`。

**应用由库和一个薄驱动组成**，结构参照 lldb 的 `liblldb` 与 `tools/driver`。`tools/driver/main.cpp` 只包含入口，其余逻辑均位于库中，因此同样可以测试。

子库按层次划分，上层依赖下层：

| 子库 | 目录 | 内容 |
|---|---|---|
| `MoreLoaderSupport` | `Support/` | 与客体无关的基础设施：UTF-16 字符串与 UTF-8 的转换、客体路径与主机路径的映射、Windows 命令行的拼接与拆分、诊断输出。可在主机上直接测试 |
| `MoreLoaderImage` | `Image/` | PE 文件的解析（头、节、导入、TLS 目录），映像在固定基址的映射，IAT 的填写，未解析导入的报错桩与调用跟踪桩 |
| `MoreLoaderRuntime` | `Runtime/` | 与客体 ABI 相关的基础设施：导出注册表，PEB、TEB 与 LDT，x87 控制字，静态 TLS 与 TLS 回调，句柄表与可等待对象，客体线程，进程的启动与退出，故障诊断 |
| `MoreLoaderWin32` | `Win32/` | kernel32、shell32、shlwapi、user32 的导出 |
| `MoreLoaderCRT` | `CRT/` | msvcrt 的导出：启动与退出、stdio、printf 族、宽字符串、`qsort`、`rand`、errno、时间、数学函数、`_setjmp3` 与 `longjmp` |
| `moreloader` | `tools/driver/` | 薄驱动：解析命令行，建立注册表，注册 Win32 与 CRT 的导出，运行映像 |

`MoreLoaderWin32` 与 `MoreLoaderCRT` 互不依赖，二者共同需要的设施（句柄、线程、errno 以外的每线程状态）位于 `MoreLoaderRuntime`。它们对外只公开注册函数，以及为测试而公开的纯计算部分（例如 printf 的格式化引擎）。

**基础设施优先使用 stdcorelib**，与 HelloUtau 相同，作为子库的私有依赖（`LINKS_PRIVATE stdcorelib::stdcorelib`），不出现在公开头文件中。stdcorelib 为通用目的设计，其行为与 Windows 不一致之处不能用于模拟 Windows 的语义，例如 `stdc::system::split_command_line` 不按 `CommandLineToArgvW` 的反斜杠规则拆分。

## 目录与文件

模块包含一个 `include/` 和一个 `lib/`，各子库在其中各占一个目录：

```
moreloader/include/moreloader/Image/PEFile.h   ← #include <moreloader/Image/PEFile.h>
moreloader/lib/Image/PEFile.cpp                ← 目标 MoreLoaderImage
moreloader/tools/driver/main.cpp               ← 目标 moreloader
moreloader/tests/auto/Image/test_PeFile.cpp    ← 测试 PEFile.h
moreloader/tests/manual/                       ← 与 Windows 原生运行的比较
```

**include 的命名空间是模块名，而非目标名。** 写 `<moreloader/Image/PEFile.h>`，不写 `<MoreLoaderImage/PEFile.h>`。

不使用 qmsetup 的 `sync_include`，`include/` 是实际存在的目录。

仅供实现使用的私有头文件放在源文件旁边，并使用 `_p.h` 后缀。私有头文件会增加实现之间的耦合，应尽量少用。

仅供多个实现文件复用且不独立编译的实现片段可以使用 `.cpp.inc` 后缀。

文件名采用大驼峰命名并与其中的主要类型一致。程序入口 `main.cpp` 保持小写。子库的全局宏放在 `<目标名>Global.h`，与 HelloUtau 相同，例如 `MoreLoaderSupportGlobal.h`。包装层按 DLL 与功能分文件，例如 `lib/Win32/Kernel32Sync.cpp`、`lib/CRT/Stdio.cpp`。

## 大小写

| 层次 | 写法 | 示例 |
|---|---|---|
| CMake 包名、`project()` | 小写 | `moreloader`、`moreloaderkit` |
| 子库目标名 | 大驼峰 | `MoreLoaderSupport` |
| include 命名空间 | 小写模块名 | `<moreloader/Support/...>` |
| 可执行文件 | 小写 | `moreloader` |

子库目录采用大驼峰命名，与去掉族前缀后的目标名一致：`lib/Support/` 对应 `MoreLoaderSupport`。缩写词全大写：`lib/CRT/` 对应 `MoreLoaderCRT`。

## C++ 命名

- 类名及其他类型名使用大驼峰命名。
- 函数名、参数名、变量名和命名空间使用小驼峰命名。
- 枚举成员使用大驼峰命名。
- 类的私有数据成员使用 `m_` 前缀。公有数据成员不使用前缀。
- getter 使用所读取的属性名，setter 使用 `set` 加属性名。
- 全局非静态变量使用 `g_` 前缀，全局静态变量使用 `s_` 前缀。应尽量避免引入全局可变状态。
- 常量（`const`、`constexpr`）使用小驼峰，不加 `g_`、`s_` 前缀。
- **缩写词全大写**，与 LLVM 相同（`COFFObjectFile`、`getTypeID`）：写 `PEFile`、`PETLSDirectory`、`LDT.h`、`loadFS`、`threadID`、`entryPointRVA`、`DLLReason`、`toLowerASCII`、`MoreLoaderCRT`。缩写位于小驼峰名字的开头时全小写：`teb()`、`tlsTemplate()`、`pebAddress()`。
- 缩写规则的例外只有逐字对应 Windows 的名字：Windows API 与 SDK 结构的名字保留原拼写，例如包装函数 `kernel32_GetCurrentProcessId`、`TEB32::TlsSlots`、`CRITICAL_SECTION32::LockSemaphore`。本仓库自己设计的类型即使描述的是 Windows 的概念，也适用缩写规则，例如 `PETLSDirectory`。
- 命名空间结束处不添加注释。

### 客体可见的名字

以下名字的拼写由 Windows 规定，不适用上述规则：

- **包装函数**命名为 `<dll>_<导出名>`，DLL 名小写且不带扩展名，导出名保持原样：`kernel32_GetLastError`、`kernel32_WaitForSingleObject`、`msvcrt__beginthreadex`、`msvcrt_fopen`。理由是必须能以导入表中的名字直接检索到实现。包装函数一律定义在各 `.cpp` 的匿名命名空间中，只经由注册表被引用。
- **客体数据结构**的类型名与字段名沿用 Windows SDK 的拼写，以便与文档对照，类型名加后缀 `32` 表示 32 位布局：写 `CRITICAL_SECTION32`、`TEB32`，字段写 `LockCount`、`OwningThread`。定义集中在 `<moreloader/Runtime/GuestLayout.h>`。这些类型与其余代码一样位于 `more::loader` 中。主机上没有 Windows SDK 的头文件，因此不存在名字冲突；与 glibc 同名的类型（`FILE`、`jmp_buf`）加 `Msvcrt` 前缀，写作 `MsvcrtFILE`。
- 导出的数据变量（`_iob`、`__mb_cur_max`、`_fmode` 等）同样以 `<dll>_<导出名>` 命名。

## 调用约定与 ABI

- **编译器相关的属性一律经由宏书写，不得直接写 `__attribute__`、`__declspec` 或 `__builtin_*`。** 宏定义在 `<moreloader/Support/MoreLoaderSupportGlobal.h>`，分别为 GCC/Clang 与 MSVC 展开，使源码在 MSVC 下同样可以解析（探针与编辑器使用 MSVC 的前端）。新的属性先在该文件中增加宏。
- 包装函数必须使用以下两个宏之一：
  - `MORE_WINAPI`：stdcall 加 `force_align_arg_pointer`，用于 kernel32、shell32、shlwapi、user32 与 msvcrt 中声明为 `__stdcall` 的函数。
  - `MORE_CDECL`：cdecl 加 `force_align_arg_pointer`，用于 msvcrt 的其余函数。
- printf 风格的函数以 `MORE_PRINTF_FORMAT_STRING` 标注格式参数，以 `MORE_PRINTF_FORMAT(格式参数序号, 首个可变参数序号)` 标注声明。
- `force_align_arg_pointer` 是必需的：MinGW 编译的客体只保证栈 4 字节对齐，而 i386 glibc 与 GCC 生成的代码假定 16 字节对齐，未对齐的栈会使 SSE 指令引发 `SIGSEGV`。
- 包装函数的参数个数与类型宽度必须与 Windows 的声明一致。每个包装函数上方以 `///` 注明其 Windows 原型中未体现在 C++ 签名中的约定，例如「Returns \c TRUE and sets the last error to 0」。
- 客体可见的结构体写成带 `static_assert(sizeof(...) == ...)` 与 `static_assert(offsetof(...) == ...)` 的定义。不以散落的字节偏移访问客体内存。
- 客体的 `wchar_t` 为 16 位，在代码中写作 `char16_t`。**不得把客体的宽字符串交给 glibc 的 `wcs*` 函数**，因为主机的 `wchar_t` 为 32 位。
- 包装函数不得抛出异常，也不得让主机的 C++ 异常穿过客体的栈帧，因为客体代码没有主机能识别的展开信息。

## 格式与内联

所有改动过的 C++ 文件在提交前使用仓库的 `.clang-format` 格式化。不要手工制造与格式化配置相冲突的对齐或换行。

初始化表达式的类型为指针时，使用 `auto name = ...`，不要写 `auto *name = ...`。

**头文件中实现的函数一律显式写出 `inline` 关键字**，包括类成员函数。成员函数在类定义之外实现时，类内的声明与类外的定义都写 `inline`。

析构函数不要使用 `override` 关键字。头文件内继承的类不要使用 `final`。

## 返回值

本仓库新设计的接口中，可能不存在结果的函数一律返回 `std::optional<T>`，不要使用「bool 加输出参数」，也不要用某个特定值表示「不存在」。需要同时报告失败原因时，返回值与原因分开：成功值用 `std::optional<T>`，原因写入调用方提供的 `std::string &errorMessage`。

模拟 Windows API 与 msvcrt 的包装函数保持 Windows 的返回约定（`BOOL`、`INVALID_HANDLE_VALUE`、`-1` 加 `errno` 等），这是被模拟的接口本身的一部分。

## 注释

公开声明使用 LLVM 风格的 `///` 文档注释，不使用 `\brief`。使用 Doxygen 的 `\c` 标识符、`\a` 参数、`\note`、`\warning` 等命令表达结构化含义。

文档注释中指向其他声明或文件的引用写成 `\sa`，放在注释的最后，只列引用对象。引用须附带说明时写成句子，如「See docs/ReverseGuide.md for the naming rules.」。

注释应解释约束、所有权、生命周期以及当前实现必须如此设计的原因。依据逆向或实测确定的语义，注释中写明依据：moresampler 的调用点地址、`msvcrt.dll` 探针的结果，或 Windows 文档的条目。

注释使用美式英语。不要用破折号连接从句，也不要用分号代替应有的断句。

文体规则见 `CLAUDE.md`「文体」。

## 头文件引用

引用块从上到下依次为系统库、标准库、第三方库、项目内被依赖的其他目标和当前目标内的头文件。不同来源的引用块之间留一个空行，同一引用块中的头文件应来自同一个库。不要依赖其他头文件偶然提供的传递引用。

在头文件中引用项目公开头文件时使用完整公共路径，同一子库内部也不例外：

```cpp
#include <moreloader/Support/Utf16.h>
```

在源文件中，同一构建目标内的头文件一律使用双引号直接引用。与源文件同名的公开头文件和 `_p.h` 私有头文件组成源文件最上方的第一个引用块。系统库、标准库、第三方库和项目内其他目标的头文件依次放在其后。当前目标内的其余头文件组成最底部的独立引用块。

```cpp
#include "PEFile.h"

#include <sys/mman.h>

#include <cstring>
#include <vector>

#include <moreloader/Support/Diagnostics.h>

#include "ImageLayout_p.h"
```

## 构建系统

构建系统使用 qmsetup，写法与 HelloUtau 相同：根目录的 `CMakeLists.txt` 执行 `find_package(qmsetup)` 与 `qm_init_directories()`；`moreloader/conf.cmake` 设置模块常量与 `_moreloader_common_configure_target`，并执行 `qm_setup_build_repo_helpers(moreloader)`；子库以 `moreloader_add_library(... STATIC ...)` 添加，驱动以 `moreloader_add_executable(...)` 添加。

**模块级前缀必须显式指定**，因为 `qm_setup_build_repo_helpers()` 默认取 `PROJECT_NAME`，而子目录中的 `PROJECT_NAME` 已是 `MoreLoaderSupport` 等子库名。

| 用途 | 前缀 | 示例 |
|---|---|---|
| 仓库级 CMake 选项与变量 | `MORE_` | `MORE_BUILD_TESTS`、`MORE_I386_OVERLAY` |
| 模块级 CMake 变量 | `MORELOADER_` | `MORELOADER_INCLUDE_DIR` |
| 模块级 CMake 函数 | `moreloader_` | `moreloader_add_library` |
| 头文件保护 | 按 include 路径 | `MORELOADER_IMAGE_PEFILE_H` |

CMake 中判断平台而非编译器。

## 测试

- 自动测试使用 Boost.Test，组织方式与 stdcorelib（`D:\GitHub\stdcorelib\tests\auto`）相同：`moreloader/tests/auto/` 编译为一个程序 `test_auto`，`main.cpp` 只定义 `BOOST_TEST_MAIN`，每个测试文件是一个 `BOOST_AUTO_TEST_SUITE`。
- 测试文件的目录与 `include/moreloader/` 相同，一个 `test_XXX.cpp` 对应一个 `XXX.h`，测试套件名与文件名相同。
- 与 Windows 原生运行的比较放在 `moreloader/tests/manual/`，不纳入 ctest，见 `CLAUDE.md`「构建与验证」。
- msvcrt 行为的黄金数据（探针在 Windows 上的输出）与生成它的探针源码一起保存在 `moreloader/tests/`，并在数据文件旁写明生成步骤。

## 与 HelloUtau 的差异

| 项 | HelloUtau | 本仓库 | 理由 |
|---|---|---|---|
| 目标平台 | Windows、macOS、Linux，64 位 | 仅 32 位 x86 Linux | 客体是由处理器直接执行的 32 位代码，与加载器共享地址空间与调用约定 |
| 子库类型 | 动态库，`<目标名>Global.h` 中有导出宏 | 静态库，`<目标名>Global.h` 中只有编译器属性的宏 | 子库只链接进单个可执行文件，加载器以静态链接分发 |
| 测试框架 | QtTest，每个测试文件一个程序 | Boost.Test，一个程序 | 本仓库不依赖 Qt，32 位构建环境中也没有 Qt；做法与 stdcorelib 相同 |
| 依赖的获取 | vcpkg 或分别安装 | `scripts/build-i386-deps.sh` 编译 qmsetup、stdcorelib 与 Boost.Test 的 32 位版本 | 系统与 vcpkg 提供的是 64 位版本 |
| 客体可见的名字 | 无 | 包装函数 `<dll>_<导出名>`，结构体沿用 Windows SDK 的拼写 | 须能以 Windows 的名字直接检索 |
| 返回约定 | 一律 `std::optional` | 包装函数保持 Windows 的约定 | 返回约定是被模拟的接口的一部分 |
