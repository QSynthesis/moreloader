# 2026-09-28 仓库建立与环境准备

## 1. 仓库骨架

做了什么：

- 按 HelloUtau 的结构建立仓库：根目录 `CMakeLists.txt`（qmsetup），模块 `moreloader/`（`conf.cmake`、`lib/`、`tools/`、`tests/`），`.clang-format` 取自 HelloUtau，`.gitignore` 忽略 `work/`。
- `CLAUDE.md` 为全局指导，`AGENTS.md` 指向它。代码规范见 `docs/Development.md`，逆向规范见 `docs/ReverseGuide.md`（取自 frqeditor-reverse 与 ra2-reverse），任务书复制为 `docs/TaskSpec.md`。
- `scripts/PrepareWork.ps1` 把 moresampler 0.8.4 复制到 `work/moresampler/` 并校验 SHA-256；`scripts/BridgeCall.py`、`scripts/BridgeRun.py` 按工程名 `moresampler_exe_084` 查找 Ghidra bridge 会话。

现状：副本的 SHA-256 为 `29e88e1c79645a46d3eb346e0b5209cd4e32f4f800a8fe1db7435c0e9e4ebdf2`，与任务书一致。Ghidra 工程由作者建立，bridge 可用，`TrivialProbe.java` 报告 1307 个函数。

## 2. 32 位构建环境

问题：WSL（Ubuntu 24.04，GCC 13.3）与 overworld（Ubuntu 22.04，GCC 11.4）均未安装 `gcc-multilib`，`gcc -m32` 找不到 `Scrt1.o`、`crti.o` 与 32 位 `libgcc`，两台机器的 `sudo` 都需要密码。

处理：

- `scripts/fetch-i386-overlay.sh` 以私有的状态目录运行 `apt-get update` 与 `apt-get download`，无需管理员权限，再以 `dpkg-deb -x` 把 `libc6-dev-i386`、`libc6-i386`、`lib32gcc-13-dev`、`lib32gcc-s1`、`lib32stdc++-13-dev`、`lib32stdc++6` 解包到 `~/.local/share/moreloader-i386`。
- 不带 multilib 的 GCC 在 `-m32` 时的 multiarch 目录是 `i386-linux-gnu`，而 Debian 系把双架构头文件装在 `x86_64-linux-gnu`。工具链文件 `cmake/toolchains/linux-i386.cmake` 以 `-idirafter` 在标准目录之后追加这两个目录，以免破坏 libstdc++ 的 `#include_next` 链；`asm/` 目录由 `gcc-multilib` 提供，overlay 中以符号链接补上。
- overlay 中的 `libc.so` 与 `libm.so` 是链接脚本，以绝对路径 `/lib32/libc.so.6` 引用成员。脚本把这些路径改写到 overlay 内。动态链接的 32 位程序在本机仍缺少 `/lib/ld-linux.so.2`，因此在构建机上运行的程序一律静态链接。

实测（WSL）：以 overlay 静态链接的 32 位 C++ 程序可以运行；`modify_ldt` 写入 LDT 项后以选择子 `0x7` 装入 FS，`mov eax, fs:0x18` 读回写入 TEB 的值；`std::thread` 正常；进程初始的 x87 控制字为 `0x37F`。WSL2 内核配置为 `CONFIG_IA32_EMULATION=y`、`CONFIG_MODIFY_LDT_SYSCALL=y`。

## 3. 依赖的 32 位版本

作者要求构建系统使用 qmsetup，测试使用 Boost.Test（参照 stdcorelib）。系统中的 Boost 1.83 与 qmsetup 均为 64 位，不能用于 32 位目标。

`scripts/build-i386-deps.sh` 编译并安装到 `~/.local/opt/moreloader-i386`：

- qmsetup：取自 `D:\GitHub\qmsetup`（提交 `0bd29c4`），以本仓库的工具链文件配置，`qmcorecmd` 静态链接。实测 `qmcorecmd --help` 可在 WSL 中运行。
- Boost.Test 1.83.0：源码取自 `archives.boost.io`，以 `b2 address-model=32 link=static` 编译，安装的 CMake 包为 `Boost-1.83.0` 与 `boost_unit_test_framework-1.83.0`。最初加了 `runtime-link=static`，Boost 的 CMake 包因此以「static runtime, Boost_USE_STATIC_RUNTIME not ON」拒绝该变体；Linux 上该选项不改变静态库的内容，已改为默认值。

验证：只含 `MoreLoaderSupport`（`Utf16`）、驱动桩与 `test_auto` 的骨架在 WSL 中配置、构建、测试均通过，产物为静态链接的 32 位 ELF。把 `Utf16.cpp` 中 `0xED` 的第二字节上限改回 `0xBF` 后 `test_invalid_utf8_maximal_subparts` 失败，还原后通过。

## 4. 静态分析的初步结果

依据：WSL 中 `objdump -d` 的全文反汇编（`work/moresampler.dis`），`pefile` 导出的 IAT（`work/iat.txt`）。

- **FS 的直接访问只有一处**：`0x4011e2` 的 `mov eax, fs:0x18`，属于 MinGW `__tmainCRTStartup` 的启动锁，读取 `NtCurrentTeb()->NtTib.StackBase`（`+0x04`）作为锁的所有者。因此 TEB 的 `+0x18` 自身指针与 `+0x04` 必须有效且每线程不同。（静态分析）
- **`srand` 的调用点**：`0x408a91` 以常数 0 调用，位于 `0x408a80` 起始的函数开头；`0x4748da` 以浮点数取整后的值调用，推断属于 Lua 的 `math.randomseed`。`rand` 的跳转桩（`0x49a168`）有 19 处调用。因此合成结果是确定的，而且 `rand` 必须按 msvcrt 的线性同余算法与每线程状态实现。（静态分析，Lua 的归属为推断）
- **`qsort` 的调用点**：`0x42e33b`、`0x42e3b0`。msvcrt 的排序算法在相等元素上的顺序与 glibc 不同，须照 msvcrt 实现并实测核对。（静态分析）
- **文件打开模式**（取自调用点前压栈的字符串）：

| 调用点 | 函数 | 模式 |
|---|---|---|
| `0x40d5b5` | `_wfopen` | `rw` |
| `0x40d69b`、`0x40d6b7`、`0x40e36a` | `_wfopen` | `w` |
| `0x41dca8` | `fopen` | `w+b` |
| `0x41de65`、`0x42df74` | `fopen` | `rb` |
| `0x42d43a` | `fopen` | `wb` |
| `0x435526` | `_wfopen` | `rb` |
| `0x46ff76` | `fopen` | `r`（格式串 `@%s` 相邻，推断属于 Lua 的 `loadfile`） |
| `0x46ffe5` | `freopen` | `rb`（推断属于 Lua） |

  `rw` 不是合法的 msvcrt 模式，其行为须在 Windows 上实测。文本模式 `w` 与 `r` 的输出涉及 `\n` 与 `\r\n` 的转换。
- `setlocale`、`localeconv`、`strcoll`、`system`、`signal`、`raise`、`_setjmp3`、`longjmp`、`clock`、`getenv` 等导入推断由内嵌的 Lua 使用，只在 oto 生成模式中执行，尚未逐一核对调用者。
- PE 头的 `Characteristics` 为 `0x30F`，**不含** `IMAGE_FILE_LARGE_ADDRESS_AWARE`。在 Windows 上该进程的用户地址空间为 2GB，而 i386 Linux 进程为 3GB 至 4GB，`malloc` 可能返回 `0x80000000` 以上的地址。GCC 生成的指针比较为无符号比较，一般不受影响，出现异常时须检查这一点。（静态分析，影响为推断）

## 下一步

1. 建立各子库的最小骨架，确认 qmsetup 与 Boost.Test 的配置与构建可行。
2. 编写 Windows 探针，测量 `msvcrt.dll` 的 printf 格式、`qsort` 的比较序列、`rand` 与文件模式 `rw` 的行为。
3. 实现映像装载与 TEB/FS，先使 `mainCRTStartup` 运行到 `main`，以未解析导入的报错桩逐个补齐包装函数。
