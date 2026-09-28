# 全局指导

此文件适用于本仓库。`AGENTS.md` 指向本文件。

## 项目目标

moreloader 是不依赖 Wine 的最小 PE 加载器与 Windows API 包装层，使未经修改的 `moresampler.exe`（0.8.4，32 位）在 Linux 上作为命令行程序运行，输出与 Windows 原生运行逐字节相同。里程碑 1 为 Linux x86_64（以 32 位进程运行），里程碑 2 为 Linux ARM64（经 FEX-Emu 或 box64 的 32 位模式）。macOS 与 64 位 moresampler 不在范围内。

目标、范围、交付物与验收标准**以 [`docs/TaskSpec.md`](docs/TaskSpec.md) 为唯一权威**。该文档与代码冲突时修改代码；确需修改设计时，先与作者确认。

## 快速开始

### 指导文档

| 内容 | 位置 |
|---|---|
| **所有 session 必读**：任务书（目标、约束、已知事实、设计要点、验收） | [`docs/TaskSpec.md`](docs/TaskSpec.md) |
| 代码编写：模块、目录、命名、注释、头文件引用 | [`docs/Development.md`](docs/Development.md) |
| 逆向：Ghidra 工程、bridge、`decomp/` 目录、命名与类型修复 | [`docs/ReverseGuide.md`](docs/ReverseGuide.md) |
| 逆向证据记录模板 | [`docs/reverse-evidence-template.md`](docs/reverse-evidence-template.md) |
| 项目状态与待办事项 | [`docs/Status.md`](docs/Status.md) |
| 工作日志 | `docs/<yyyymmdd>-<task>.md` |

### 工具链

- **构建在 WSL 中进行**（`Ubuntu-24.04`，GCC 13）。加载器必须是 32 位 x86 Linux 程序，配置时指定 `-DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/linux-i386.cmake`。
- WSL 与 overworld 均未安装 `gcc-multilib`，`sudo` 需要密码。`scripts/fetch-i386-overlay.sh` 在无管理员权限的情况下把 32 位 glibc 与 libgcc 解包到 `~/.local/share/moreloader-i386`，配置时以 `-DMORE_I386_OVERLAY=<目录>` 传入。安装了 `gcc-multilib` 的系统不需要该目录。overlay 中的动态链接程序在本机缺少 `/lib/ld-linux.so.2`，无法运行，因此在构建机上运行的程序一律静态链接。
- 加载器默认静态链接（`MORE_STATIC`），运行时不需要任何 32 位库。
- **依赖**：构建系统使用 qmsetup（与 HelloUtau 相同），基础设施使用 stdcorelib，自动测试使用 Boost.Test（与 stdcorelib 相同）。三者都必须是 32 位版本，由 `scripts/build-i386-deps.sh <qmsetup 源码目录> <stdcorelib 源码目录>` 编译并安装到 `~/.local/opt/moreloader-i386`：qmsetup 取自 `D:\GitHub\qmsetup`，其 `qmcorecmd` 静态链接；stdcorelib 取自 `D:\GitHub\stdcorelib`，为静态库；Boost.Test 为 1.83 的静态库。WSL 中这两个目录为 `/mnt/d/GitHub/...`。不要使用系统的 64 位版本，它们的包配置文件会以指针宽度不符为由被 CMake 拒绝。
- 构建命令：

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/linux-i386.cmake \
    -DMORE_I386_OVERLAY=$HOME/.local/share/moreloader-i386 \
    -Dqmsetup_DIR=$HOME/.local/opt/moreloader-i386/lib/cmake/qmsetup \
    -Dstdcorelib_DIR=$HOME/.local/opt/moreloader-i386/lib/cmake/stdcorelib \
    -DBoost_DIR=$HOME/.local/opt/moreloader-i386/lib/cmake/Boost-1.83.0 \
    -DMORE_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --no-tests=error
```

- Windows 侧工具：
  - Python：`D:\usr\tools\miniconda3\envs\venv1\python.exe`（含 `pefile`、`requests`、`ghidra-re`），不在 PATH 中。
  - JDK：`C:\Users\user\.jdks\temurin-21.0.11`。
  - Ghidra：`D:\usr\tools\ghidra_12.0.4_PUBLIC`，本仓库的工程为 `C:\Users\user\ghidra-projects\projects\moresampler\moresampler_exe_084.gpr`，见 [`docs/ReverseGuide.md`](docs/ReverseGuide.md)。
  - MSVC 2022：用于编译在 Windows 上测量 `msvcrt.dll` 行为的探针程序。
- 另一台 Linux 主机 `ssh overworld`（Ubuntu 22.04，GCC 11）可作为备用测试环境，同样没有 `gcc-multilib`。

## 本仓库的约束

详见 [`docs/TaskSpec.md`](docs/TaskSpec.md) 第 2 节，要点如下。

- **许可证**：`moresampler.exe` 不得提交进仓库，不得与加载器一起打包分发，由用户自备。**不得修改 exe 文件本身**；在内存中打补丁原则上不需要，确需时须先向作者说明。
- **只用副本**：原件位于 `E:\temp-repos\UTAU\UTAU\tools\moresampler 0.8.4\`，不得在该目录内运行或写入。分析与测试只使用 `scripts/PrepareWork.ps1` 复制到 `work/moresampler/` 的副本；测试用音源同样只使用复制到 `work/` 下的副本。`work/` 已被 `.gitignore` 忽略。
- **不复制代码**：Wine（LGPL-2.1）与 Tavis Ormandy 的 loadlibrary（GPL-2.0）只作为语义参考，不复制其代码。反编译得到的 moresampler 伪代码不粘贴进源代码与文档，文档以文字、表格和伪代码描述行为。
- **来源的区分**：文档中每条关于 Windows、msvcrt 或 moresampler 行为的结论注明依据，依据分为四类：静态分析（函数地址与名称）、实测（步骤、程序与样本）、公开资料（链接）、推断（推断的根据）。推断不得写成定论。

## 推理原则

- 如果代码更改是通过逆向工程或一致性证据而合理得出的，应在附近添加源代码注释。
- 当逆向证据不完整时，在最终报告中陈述剩余的假设，而不是将其表述为已证实的内容。
- 如果任何已实现的逻辑、参数或偏移量仍然只是推断得出的，而非有具体的逆向证据或实测支持，应以明显的方式向用户指出这一点。
- 对于一致性工作，不要猜测逻辑、参数或偏移量。首先遵循原始的逆向证据与实测结果，只有在明确指出该限制的情况下才公布推断的行为。
- **不许把输出差异归因于浮点精度。** x87 80 位与 double 的差异只可能造成末位差异，只能解释逐字节比较失败而数值几乎相同的情形。任何可听出的差异或大量字节差异都是逻辑错误（值、索引、分支、舍入模式、调用约定），必须定位到具体函数，不得写「精度残差」「浮点口径」了事。唯一例外是浮点差异翻转了某个离散判定（取整、阈值比较），那也是可定位的具体事件，须找到该翻转点。常见原因见 [`docs/TaskSpec.md`](docs/TaskSpec.md) 第 6 节。
- **msvcrt 与 Windows 的行为以实测为准。** 凭记忆描述的 msvcrt 行为（printf 的格式、`qsort` 的算法、文件模式的解析）必须在 Windows 上以探针程序调用 `msvcrt.dll` 验证，探针与其输出作为测试的黄金数据保存在仓库中。

### 实现原则

- 包装层的第一要义是与 Windows 行为一致，而不是与 POSIX 或 glibc 一致。转交 glibc 之前确认二者在 moresampler 实际使用的输入范围内结果相同，不同之处自行实现。
- 客体可见的内存布局（结构体、`FILE`、TEB、`jmp_buf`、`wchar_t`）以 32 位 Windows 为准，布局写成带静态断言的结构体，不以散落的字节偏移访问。
- 包装函数的参数个数与调用约定必须与 Windows 的声明一致。`__stdcall` 函数由被调方弹出参数，参数个数不符会破坏客体的栈，并且不会立即表现出来。
- 只实现 moresampler 实际需要的语义。未实现的分支（例如 `SuspendThread`）明确地报告并失败，不要静默返回成功。
- 不要为了「更优雅」的结构改写 Windows 语义所规定的阶段顺序，例如进程启动时 TLS 回调、入口点、`_initterm` 的次序，以及退出时 onexit 表、流的刷新与 `DLL_PROCESS_DETACH` 的次序。

## 代码编写

目录、命名、格式、注释与头文件引用见 [`docs/Development.md`](docs/Development.md)，它才是权威，以下仅为摘要。规则取自 HelloUtau 的 `AGENTS.md` 与 `docs/Development.md`，本仓库特有的差异在 `docs/Development.md` 中说明理由。

- 一个模块 `moreloader`（命名空间 `more::loader`；模拟 msvcrt 与 Windows 语义的代码位于 `more::loader::msvcrt` 与 `more::loader::win32`），由若干静态子库与一个薄驱动组成：`MoreLoaderSupport`、`MoreLoaderImage`、`MoreLoaderRuntime`、`MoreLoaderWin32`、`MoreLoaderCRT`，以及 `tools/driver/main.cpp` 产出的可执行文件 `moreloader`。
- 模块包含一个 `include/` 和一个 `lib/`：`moreloader/include/moreloader/Image/PEFile.h` 对应 `moreloader/lib/Image/PEFile.cpp`。include 的命名空间是模块名，写 `<moreloader/Image/PEFile.h>`。私有头文件与源文件放在一起，加 `_p.h` 后缀，尽量少用。
- **缩写词全大写**（`PEFile`、`threadID`、`loadFS`、`MoreLoaderCRT`），位于小驼峰名字开头时全小写（`teb()`）；逐字对应 Windows API 与 SDK 的名字保留原拼写（`kernel32_GetCurrentProcessId`、`TEB32::TlsSlots`）。
- 文件名、类型名采用大驼峰；函数、参数、变量、命名空间采用小驼峰；枚举成员采用大驼峰；私有数据成员使用 `m_` 前缀；常量使用小驼峰。**唯一的例外是客体可见的包装函数**，命名为 `<dll>_<导出名>`（如 `kernel32_GetLastError`、`msvcrt_fopen`），以便与导入表直接对照检索。
- **基础设施优先使用 stdcorelib**（UTF 转换 `stdc::utf`、字符串工具 `stdc::str` 等），作为各子库的私有依赖。stdcorelib 的行为与 Windows 不一致之处（例如 `stdc::system::split_command_line` 不按 `CommandLineToArgvW` 的反斜杠规则）不能用于模拟 Windows 的语义。
- 可能不存在结果的函数返回 `std::optional<T>`，不要使用「bool 加输出参数」，也不要用某个特定值表示「不存在」。模拟 Windows API 的包装函数保持 Windows 的返回约定。
- **编译器相关的属性一律经由 `<moreloader/Support/MoreLoaderSupportGlobal.h>` 中的宏书写**（`MORE_WINAPI`、`MORE_CDECL`、`MORE_PRINTF_FORMAT` 等），不直接写 `__attribute__`、`__declspec` 或 `__builtin_*`，使源码在 MSVC 下同样可以解析。
- 头文件中实现的函数一律显式写出 `inline`；初始化表达式为指针时写 `auto name = ...`；析构函数不写 `override`，头文件中被继承的类不写 `final`；命名空间结束处不添加注释。

### 文体

本节适用于注释、文档（包括 `docs/` 下的日志）、README、帮助文本和诊断消息。

- **采用正式的技术写作文体。** 注释与文档是规范性文本而非叙述。每句陈述一项事实、约束或理由，不写铺垫、感想和修辞。
- **不拟人。** 代码、文件、格式、程序和测试不作为有意志的主语：不写 says、tells、knows、asks、wants、means、cares、decides、promises、is told，也不写「它说」「它知道」「它不认」「它想要」。改用 returns、indicates、records、specifies、reports、detects、requires、rejects，或「返回」「表示」「记录」「规定」「报告」「拒绝」。用户、作者、调用方等真实行为主体可以作主语。
- **使用术语，不用描述性转述。** 写 invalid byte sequence，不写 bytes that do not decode。没有通用术语时，首次出现给出定义，之后始终沿用同一名称。
- **标题、分组名和列表标签使用名词或名词短语。** 写 Motivation、Behavior、Rationale，不写 Why、What it does、How it works。能用名词表达时，不用 what 引导的名词从句作主语或宾语。
- **条件用 if，where 只表示处所。** 不写 empty where there is none，写 empty if absent。不用 one 回指前文名词，直接重复该名词。
- **不使用口语短语。** 不写 whatever else、for good、as it stands、on its own、on the way out、at a glance、there and back、is given up on、the rest of why、and all 等说法，改为准确的书面表达。
- **句子完整。** 不写片段句、逗号粘连句和反问句。不以 So、And so、Which is why、That is why、Hence 开头叙述因果，改为在同一句中用 because、therefore 表明。不对读者使用第二人称。
- **函数说明以动词开头**（Returns、Decodes、Reads、Rejects）。`\return` 写明每种情况的返回值。布尔查询写 Returns whether …。
- 中文文本同样适用：使用书面语，不用「别」「搞」「就行」「得（表必须）」「啥」「拿来」「反正」「其实」「说白了」「这玩意儿」等口语词。标题不用「为什么」「怎么做」，改用「动机」「设计理由」「实现方式」。「不要」「必须」等规范性祈使句不属于口语，照常使用。

### 注释

- LLVM 风格，`///` 写在声明上方。**从不使用 `\brief`。**
- 类的 private 成员和 `.cpp` 中的实现细节使用普通的 `//`。私有头文件中的类型和非 private 声明仍使用 `///`。
- Doxygen 命令使用 `\c` `\a` `\note` `\warning` `<tt>`，不要使用反引号或引号。注释中书写以 `@` 开头的字面词时，必须转义为 `\@`。
- 指向其他声明或文件的引用写成 `\sa`，放在注释的最后，只列引用对象，多个以逗号分隔。正文须在去掉引用后仍然完整。
- **注释中不要使用破折号，也不要用分号连接从句。** 应断句处即断句。使用美式拼写。
- **几个词能说清的内容不写成一段。** 注释说明约束、所有权、生命周期，以及「为何只能如此实现」。不复述签名已经表明的内容。
- **不写考古式注释。** 保留「为何现在必须如此实现」，删除「以前如何、后来修正」，修改历史由版本控制保存。
- 头文件说明调用方据以行动的内容，cpp 说明实现理由，或不写注释。
- 逆向与实测的依据（moresampler 中的调用点地址、`msvcrt.dll` 的实测结果、Windows 文档）属于「为何只能如此实现」，写在声明或实现附近。

### Markdown

- **一段即一行**，不要按 80 或 100 列手动折行。代码块、表格、列表项照常处理。此规则仅适用于 `.md`，C++ 注释仍为 100 列。
- 文件末尾不留多余的空行。

修改后只对自己改动过的 C++ 文件运行 `clang-format`。**不要用 `sed -i` 处理整个目录。**

## 构建与验证

- **测试使用 Boost.Test**，做法与 stdcorelib 相同：`moreloader/tests/auto/` 编译为一个测试程序 `test_auto`，`main.cpp` 只定义 `BOOST_TEST_MAIN`，每个测试文件是一个 `BOOST_AUTO_TEST_SUITE`，由 ctest 以一项运行。本仓库不依赖 Qt，32 位构建环境中也没有 Qt，因此不使用 HelloUtau 的 QtTest。
- **一个 `test_XXX.cpp` 对应一个 `XXX.h`**，目录结构与 `include/moreloader/` 相同：`tests/auto/Support/test_Utf16.cpp` 对应 `include/moreloader/Support/Utf16.h`。这样只看目录列表即可知道哪些头文件尚无测试。
- `ctest` 找不到任何测试时同样以 0 退出，运行时加 `--no-tests=error`。
- 与 Windows 原生运行的逐字节比较需要用户自备的 exe 与音源，不纳入 ctest，放在 `moreloader/tests/manual/`，步骤与结果记入工作日志。比较时先关闭多线程合成，一致后再打开。
- **确认测试通过之前，先确认构建的退出码为 0。** 构建失败时 ctest 运行的是上一次构建的旧程序，会给出虚假的通过结果。
- 新增测试后确认断言确实被执行，空的测试集同样会「通过」。
- 区分断言的是当前行为还是设计意图。测试可能只是将缺陷固化了下来。
- **完成一项行为的测试后，将缺陷放回，确认测试会失败。** 修改一行、重新构建、运行、还原。

## 提交

- **提交信息不带任何 AI 署名**，不写 `Co-Authored-By`，不写 `Generated with`。
- **只写一行**，使用英文、首字母大写的祈使句和美式拼写，不写正文。设计理由写在代码注释和 `docs/` 中。
- **一个提交只做一件事。** 同一个文件包含两批改动时，用 `git show HEAD:<path>` 取出旧内容，只叠加其中一批改动后提交，再恢复完整版本，不要为图省事暂存整个文件。
- 每个提交自身必须能够构建并通过测试，拆分出的中间状态同样如此。
- **未经授权不要 commit，更不要 push。** 修改完成后保留工作区，待作者同意后再提交。

## 判断与沟通

- **断言之前先验证。** 「文档是这样写的」不等于「`msvcrt.dll` 确实这样实现」。
- **先测量，再下结论。** 结论必须来自编译器、运行时探针、反汇编或参考实现，不要凭记忆断言。
- **探针本身也可能出错。** 结果异常时先怀疑探针。
- 用户的质疑通常是正确的，**应先重新验证，而不是辩护**。
- 发现自己有误时直接更正，将结论写回工作日志的对应条目，并注明原判断的错误所在。
- 不确定时明确说明不确定，不要以推测填补。

## 建议工作流

1. 逆向采用 HTTP bridge 与本地导出的 `.c` 文件两种模式共存。简单操作通过 bridge 向 Ghidra 发送请求，批量操作使用 Java 脚本，见 [`docs/ReverseGuide.md`](docs/ReverseGuide.md)。
2. 需要理解 moresampler 如何使用某个导入函数，或定位输出差异时，先把相关函数的伪代码导出到 `decomp/moresampler/<slice>/`，先修复 Ghidra 工程内的函数名、变量名、类型与签名并补充注释，再重新导出，然后才下结论。不要只修改导出的 `.c`。
3. 如果有足够的证据，修复类型时可以在 Ghidra 中建立 enum 替换伪代码中的纯数值，建立 struct 替换伪代码中的字节偏移读写。除全局变量外，参数与局部变量也应尽可能修复。
4. 确定函数名以后，将导出文件命名为 `<函数名>.c`（与 Ghidra 中的函数名一致，形如 `FUN_<地址>_<语义名>.c`）。
5. 静态链接的库（MinGW 运行库、winpthreads、libgcc、Lua、libllsm 等）只识别并标名，不逆向，见 [`docs/ReverseGuide.md`](docs/ReverseGuide.md)「静态链接的库」。
6. 在 `docs/` 中为每项任务建立 `<yyyymmdd>-<task>.md` 作为日志，每一步记录做了什么、遇到了什么问题、目前情况如何、下一步做什么，以及执行中值得记录的关键信息。任务进度到达里程碑时可以建立新的日志。

## 已知问题

- **本机环境变量中配置了 HTTP 代理**，发往 `127.0.0.1` 的请求也会被交给代理并返回 502。直接用 `curl` 调用 bridge 时加 `--noproxy '*'`；`scripts/BridgeCall.py` 已关闭 `trust_env`。
- **Git Bash 会把以 `/` 开头的参数转换为 Windows 路径**，`/functions/search` 会变成 `C:/Program Files/Git/functions/search`。在 Git Bash 中调用 `scripts/BridgeCall.py` 或 `ghidra-re` 时先设置 `MSYS_NO_PATHCONV=1`。
- **在 Windows 上经 `wsl -- bash -c` 执行命令时，标准输出可能丢失或以 UTF-16 输出**（`wsl.exe` 的提示信息为 UTF-16）。需要可靠的输出时，让 WSL 中的脚本把输出写入文件，再在 Windows 侧读取。
- **仓库位于不区分大小写的 NTFS（WSL 中为 `/mnt/e`），而子库的 include 目录在编译器的搜索路径中。** 头文件名不得在忽略大小写后与系统头文件同名：名为 `Strings.h` 的头文件会被 glibc 的 `<string.h>` 当作 `<strings.h>` 包含，产生大量 C 链接的模板错误。命名时避开 `string`、`strings`、`math`、`time`、`signal`、`random`、`locale`、`errno` 等。
- **测试目录与子库的源文件由 `file(GLOB_RECURSE ... CONFIGURE_DEPENDS ...)` 收集。** 缺少 `CONFIGURE_DEPENDS` 时，新增的测试文件在重新配置之前不会被编译，ctest 仍会报告通过。新增测试后以 `test_auto --report_level=detailed` 确认新的测试套件确实出现在报告中。
- **`sed -i` 会将 CRLF 转换为 LF。** 修改前用 `grep -qU $'\r'` 判断，CRLF 文件改用编辑工具修改。
- **绝不使用 bash heredoc 编写含反斜杠的 C++ 文本或含非 ASCII 字符的脚本**，因为 shell 会改写反斜杠。应先写入文件再执行。
- **被信号终止的进程不会刷新缓冲的 stdout。** 没有任何输出、看似未运行时，实际上可能是进程已终止。
