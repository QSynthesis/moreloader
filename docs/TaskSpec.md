# 任务书：moreloader（在 Linux 与 ARM 上运行 32 位 moresampler 0.8.4 的最小 PE 加载器）

写于 2026-09-28。本任务书由 frqeditor-reverse 仓库的会话整理，交给新仓库的 agent。所有回复与文档使用中文，书面语。

## 1. 目标

编写一个不依赖 Wine 的最小 PE 加载器与 Windows API 包装层，使未经修改的 `moresampler.exe`（0.8.4，32 位）在 Linux 上作为命令行程序运行，功能与 Windows 原生运行一致，输出逐字节相同。

- 里程碑 1（本任务书的交付范围）：Linux x86_64（以 32 位进程运行）跑通 resampler 模式与 wavtool 模式，并与 Windows 原生运行的输出逐字节比较。
- 里程碑 2（之后）：Linux ARM64 上经 x86 转译器（FEX-Emu 或 box64 的 32 位模式）运行同一个加载器。
- 追加（作者 2026-09-29 决定）：UTAU 自带的 `resampler.exe`（Visual C++ 6 编译，C 运行库静态链接，只导入 kernel32），以相同的逐字节标准支持，见 [`20260929-resampler.md`](20260929-resampler.md)。
- 不做：macOS（10.15 起不能运行 32 位进程，Rosetta 2 不转译 32 位代码）、64 位 moresampler（作者手上只有 32 位版，决定只支持 32 位）、图形界面、moresampler 的 oto 自动生成模式（可以顺带支持，不作为验收项）。

## 2. 作者的决定与约束

- **只支持 32 位 moresampler 0.8.4**。
- **仓库独立**：本项目与 frqeditor-reverse 无关，作者新建仓库。
- **许可证**：moresampler 的 `license.txt` 允许原样再分发，但「作者未许可时不得作为其他软件的一部分分发」。因此：
  - 不得把 `moresampler.exe` 提交进仓库，不得与加载器一起打包分发；用户自行提供 exe。
  - 不得修改 exe 文件本身。加载后在内存中打补丁原则上不需要，确需时须先向作者说明。
- **只用副本**：原件位于 `E:\temp-repos\UTAU\UTAU\tools\moresampler 0.8.4\`，不得在该目录内运行或写入。分析与测试使用复制到仓库 `work/`（加入 `.gitignore`）的副本。
- **提交**：未经作者明确同意不 commit，从不 push。提交信息为一行英文祈使句，首字母大写，不写正文，不带任何 AI 署名，一个提交只做一件事。
- **文体**：注释与文档为正式技术文体，不拟人，不写口语；注释用 `///`（声明）与 `//`（实现），不用破折号。
- **推理原则**：输出与 Windows 不一致时，不得归因于「浮点精度」了事；x87 与 double 的差异只可能造成末位差异，任何可听出的差异或大量字节差异都是逻辑错误（常见原因见第 6 节），必须定位到具体函数。

## 3. 已知事实（静态分析，`pefile`）

### 3.1 文件

| 项 | 值 |
|---|---|
| 路径 | `E:\temp-repos\UTAU\UTAU\tools\moresampler 0.8.4\moresampler.exe` |
| SHA-256 | `29e88e1c79645a46d3eb346e0b5209cd4e32f4f800a8fe1db7435c0e9e4ebdf2`（942080 字节） |
| 机器 | i386，控制台子系统（3），OS/子系统版本 4.0 |
| 编译器 | MinGW，`GCC: (GNU) 6.2.1 20161119`，链接器 2.28，C 运行库为 `msvcrt.dll` |
| 映像基址 | `0x400000`，映像大小 `0xed000`，**没有重定位表**，必须装在 `0x400000` |
| 入口点 | `0x4014e0`（MinGW 的 `mainCRTStartup`） |
| 栈 | 保留 `0x200000`，提交 `0x1000` |
| 节 | `.text` `0x1000`（`0x9bf68`）、`.data` `0x9d000`、`.rdata` `0xac000`、`.bss` `0xde000`、`.idata` `0xe0000`、`.CRT` `0xe2000`、`.tls` `0xe3000`、`.rsrc` `0xe4000` |
| TLS | 模板 `0x4e3000`–`0x4e301c`，索引变量 `0x4de4b0`，回调 3 个：`0x48ec60`、`0x48ec10`、`0x48a5d0` |
| 延迟导入 | 无 |
| 同目录文件 | `moreconfig.txt`（exe 在自身目录读取）、`readme.txt`、`how-to-use.txt`、`license.txt`、`docs/`（静态链接的开源库许可证：libllsm、libpyin、libgvps、liblrhsmm、WORLD 等） |

### 3.2 导入（共 187 个）

- `KERNEL32.dll`（60）：AddVectoredExceptionHandler CloseHandle CreateEventA CreateSemaphoreA CreateSemaphoreW DeleteCriticalSection DuplicateHandle EnterCriticalSection FindFirstFileW FindNextFileW GetCommandLineW GetCurrentProcess GetCurrentProcessId GetCurrentThread GetCurrentThreadId GetHandleInformation GetLastError GetModuleFileNameW GetModuleHandleA GetModuleHandleW GetProcAddress GetProcessAffinityMask GetStartupInfoA GetSystemTimeAsFileTime GetThreadContext GetThreadPriority GetTickCount InitializeCriticalSection IsDBCSLeadByteEx IsDebuggerPresent LeaveCriticalSection LockFileEx MultiByteToWideChar OutputDebugStringA QueryPerformanceCounter RaiseException ReleaseSemaphore RemoveVectoredExceptionHandler ResetEvent ResumeThread SetEvent SetLastError SetProcessAffinityMask SetThreadContext SetThreadPriority SetUnhandledExceptionFilter Sleep SuspendThread TerminateProcess TlsAlloc TlsGetValue TlsSetValue TryEnterCriticalSection UnhandledExceptionFilter UnlockFileEx VirtualProtect VirtualQuery WaitForMultipleObjects WaitForSingleObject WideCharToMultiByte
- `msvcrt.dll`（124）：__dllonexit __getmainargs __initenv __lconv_init __mb_cur_max __set_app_type __setusermatherr _acmdln _amsg_exit _beginthreadex _cexit _endthreadex _errno _exit _fileno _fmode _get_osfhandle _initterm _iob _lock _mbslen _onexit _setjmp3 _snwprintf _strdup _stricmp _strnicmp _ultoa _unlock _vsnwprintf _wcsdup _wfopen _wgetcwd _wstat abort acos asctime asin atof atoi calloc clock cosh exit fclose feof ferror fflush fgetc fgets fopen fprintf fputc fputs fread free freopen frexp fseek ftell fwprintf fwrite getc getchar getenv gmtime isalnum isalpha iscntrl isgraph islower ispunct isspace isupper isxdigit localeconv log10 longjmp malloc memchr memcmp memcpy memmove memset printf putchar puts qsort raise rand realloc setlocale signal sinh sprintf srand strchr strcmp strcoll strcpy strerror strlen strncmp strncpy strpbrk strspn strstr strtol strtoul system tan tanh time tolower toupper vfprintf vprintf wcscat wcscmp wcscpy wcslen wcsncmp wcsncpy wcstol
- `SHELL32.dll`：CommandLineToArgvW；`SHLWAPI.dll`：PathIsDirectoryW；`USER32.dll`：MessageBoxW

### 3.3 数据导入

以下导入被代码当作变量读取（导入地址在 `.text` 中出现而没有 `call [iat]` 形式），必须导出为数据而非函数：`_iob`（约 120 处，即 `stdin`、`stdout`、`stderr` 为 `&_iob[0..2]`）、`__mb_cur_max`（7 处）、`__initenv`、`_acmdln`、`_fmode`。`_stricmp`、`_strnicmp` 也被这样检出，推断为经寄存器间接调用的函数（`mov reg, [iat]` 后 `call reg`），实现时仍作为函数导出并在反汇编中确认。

## 4. 平台结论

| 平台 | 方案 | 结论 |
|---|---|---|
| Linux x86_64 | 加载器编译为 32 位 Linux 程序（`-m32`，非 PIE，避开 `0x400000`–`0x4ed000`）；以 `set_thread_area` 分配一个 GDT 的 TLS 项（该项属于每个线程），每个线程在其中写入自己伪造的 TEB 的基址，装入 FS（i386 glibc 用 GS 作 TLS，FS 空闲）。不使用 `modify_ldt`，因为 FEX-Emu 对 32 位客体的 `modify_ldt` 是中止进程的桩，见 [`20260929-fex.md`](20260929-fex.md) | 里程碑 1 |
| Linux ARM64 | 同一个 32 位加载器，由 FEX-Emu 或 box64（BOX32）转译运行；FEX-Emu 支持 32 位的 `set_thread_area` 与 `mov fs`（实测），box64 待确认 | 里程碑 2 |
| macOS | 不支持（第 1 节） | 不做 |

## 5. 设计要点

### 5.1 装载

1. 以 `mmap(MAP_FIXED_NOREPLACE)` 在 `0x400000` 保留 `0xed000` 字节，复制文件头与各节，按节属性 `mprotect`。
2. 解析导入表：按（DLL 名，函数名）在加载器的包装表中查找，写入 IAT。数据导入写入加载器中对应变量的地址。找不到的导入写入一个会打印 DLL 名与函数名后中止的桩。
3. 建立 PEB 与主线程 TEB，装入 FS，设置 TLS（5.3），以 `DLL_PROCESS_ATTACH` 调用 TLS 回调，然后跳到入口点。入口点为 MinGW 的 `mainCRTStartup`，它自行调用 `__set_app_type`、`__getmainargs`、`_initterm`、`main`、`exit`。
4. 进程退出码为 `exit` 或 `_exit` 的参数。

### 5.2 TEB 与 FS

- 32 位 TEB 至少提供：`+0x00` 异常链表头（置 `0xFFFFFFFF`）、`+0x04` 栈顶、`+0x08` 栈底、`+0x18` 自身地址、`+0x20` 进程 ID、`+0x24` 线程 ID、`+0x2c` `ThreadLocalStoragePointer`、`+0x30` PEB、`+0x34` LastError、`+0xe10` `TlsSlots[64]`。`GetLastError`、`TlsGetValue` 等直接读写 TEB，以便与可能内联访问 FS 的代码一致。
- 每个线程（包括 `_beginthreadex` 创建的）在执行任何客体代码前装好自己的 FS。
- **x87 控制字**：Windows 进程初始为 `0x27F`（53 位精度），Linux 为 `0x37F`（64 位扩展精度）。进入客体代码前以及每个新线程开始时须设为 `0x27F`，否则 x87 运算结果与 Windows 不同，无法逐字节一致。MXCSR 两者缺省相同（`0x1F80`），仍应显式设置。

### 5.3 TLS

- 静态 TLS：每个线程按模板复制一块数据（`EndAddressOfRawData − StartAddressOfRawData` 加 `SizeOfZeroFill`），填入 `ThreadLocalStoragePointer[index]`，索引写入 `0x4de4b0`（取 0）。
- 回调：进程开始时 `DLL_PROCESS_ATTACH`，新线程开始时 `DLL_THREAD_ATTACH`，线程结束时 `DLL_THREAD_DETACH`，进程退出时 `DLL_PROCESS_DETACH`。回调约定为 `__stdcall (HINSTANCE, DWORD, LPVOID)`。它们属于 MinGW 运行库与 winpthreads。
- `TlsAlloc`、`TlsGetValue`、`TlsSetValue` 使用 TEB 的 `TlsSlots`。

### 5.4 调用约定与 ABI

- Windows API（kernel32、shell32、shlwapi、user32）为 `__stdcall`，以 `__attribute__((stdcall))` 实现；msvcrt 为 `__cdecl`，与 i386 Linux 相同。
- 可变参数：i386 上两者的 `va_list` 均为栈上指针，可直接转交。
- 结构布局以 32 位 Windows 为准：`CRITICAL_SECTION` 24 字节、`WIN32_FIND_DATAW` 592 字节、`FILETIME`、`MEMORY_BASIC_INFORMATION`、`SYSTEM_INFO`、msvcrt 的 `FILE` 32 字节、`struct _stat`（`_wstat` 的 32 位时间版本）、`struct tm`（前 9 个 `int`）、`jmp_buf`。
- `wchar_t` 在 Windows 为 16 位 UTF-16，在 Linux 为 32 位。所有宽字符函数（`wcs*`、`_wfopen`、`_wstat`、`_snwprintf`、`_vsnwprintf`、`fwprintf`、`_wcsdup`、`_wgetcwd`、`wcstol`、`CommandLineToArgvW`、`GetCommandLineW` 等）必须按 `uint16_t` 自行实现，不得转交 glibc 的 `wcs*`。
- 宽字符 printf 族须自行解析格式：msvcrt 中宽字符版的 `%s` 表示宽字符串，`%S` 表示窄字符串。数值转换可逐个转交 `snprintf`。窄字符 printf 族须把 msvcrt 特有的长度修饰 `I64`、`I32` 改写为 glibc 的形式；`%Lf` 在 msvcrt 中与 `%f` 相同（`long double` 即 `double`）。

### 5.5 msvcrt

- **stdio**：自建 32 字节的 msvcrt `FILE` 数组 `_iob[20]`，前三项对应主机的 stdin、stdout、stderr；`fopen` 等返回自建的 `FILE`，在旁表中记录主机 `FILE *`。所有 stdio 函数先把客体 `FILE *` 映射到主机 `FILE *`。`_fileno`、`_get_osfhandle` 返回可被 `LockFileEx` 识别的句柄。文本模式：msvcrt 的 `fopen` 默认文本模式（`\r\n` 转换），检查 moresampler 以何种模式打开文件，输出比较时注意。
- **errno**：`_errno` 返回每线程变量的地址，主机 errno 值映射到 msvcrt 值（常用值相同，仍须建表）。
- **启动**：`__getmainargs` 以窄字符串（UTF-8）提供 `argc`、`argv`、`envp`；`_acmdln` 为窄命令行；`__initenv` 为环境；`__set_app_type`、`__setusermatherr`、`__lconv_init`、`_amsg_exit`、`_initterm`、`_onexit`、`__dllonexit`、`_cexit`、`_lock`、`_unlock` 按 MinGW 的 `crtexe.c` 语义实现。
- **setjmp**：`_setjmp3` 与 `longjmp` 为一对，均只在客体代码中使用，按 msvcrt 的 `jmp_buf` 布局以汇编自行实现（保存 EBX、ESI、EDI、EBP、ESP、EIP）。
- **时间**：`time`、`gmtime`、`asctime` 为 32 位 `time_t`；`clock` 的 `CLOCKS_PER_SEC` 在 msvcrt 为 1000。
- **数学**：`acos`、`asin`、`cosh`、`sinh`、`tan`、`tanh`、`log10`、`frexp`、`atof` 转交 glibc libm（x87 返回值约定相同）。其余数学函数静态链接在 exe 中。若输出出现末位差异，先比较这些函数在两边的结果，定位到具体调用。
- **其他**：`malloc` 族、`qsort`、字符分类、`str*` 直接转交；`rand`、`srand` 须实现 msvcrt 的线性同余算法（`seed = seed × 214013 + 2531011`，返回 `(seed >> 16) & 0x7fff`），不得用 glibc 的 `rand`；`system` 记录调用并返回失败，或按需实现；`signal`、`raise` 最小实现；`setlocale`、`localeconv` 返回 C locale。

### 5.6 kernel32 等

- **句柄**：统一句柄表，类型有文件、事件、信号量、线程、查找句柄；伪句柄 `GetCurrentProcess() = −1`、`GetCurrentThread() = −2`。`DuplicateHandle`、`GetHandleInformation` 按 winpthreads 的用法实现。
- **同步**：`CRITICAL_SECTION`（递归互斥，主机互斥量指针存于结构内）、事件、信号量、`WaitForSingleObject`、`WaitForMultipleObjects`（含等待线程结束），以 pthread 互斥量与条件变量实现。
- **线程**：`_beginthreadex` 以 pthread 创建线程，栈不小于 `0x200000`；新线程先建 TEB、装 FS、设 x87 控制字与 TLS、调用 TLS 回调，再调用 `__stdcall` 的线程函数。`SuspendThread`、`ResumeThread`、`GetThreadContext`、`SetThreadContext` 为 winpthreads 的取消与信号支持，先实现为记录并返回失败，确认 moresampler 不调用。`SetThreadPriority`、`GetThreadPriority`、`SetProcessAffinityMask` 为无操作；`GetProcessAffinityMask` 按 CPU 数返回掩码（决定 moresampler 的线程数）。
- **内存**：`VirtualQuery`、`VirtualProtect` 为 MinGW 伪重定位（`_pei386_runtime_relocator`）所用，按映像的实际映射返回信息并以 `mprotect` 实现。
- **异常**：`AddVectoredExceptionHandler`、`RemoveVectoredExceptionHandler`、`SetUnhandledExceptionFilter`、`UnhandledExceptionFilter` 记录处理函数；里程碑 1 不模拟 SEH。`RaiseException` 忽略线程命名异常 `0x406D1388`，其余打印后中止。
- **文件**：`FindFirstFileW`、`FindNextFileW`（`opendir` 加通配符匹配，填 `WIN32_FIND_DATAW`）、`LockFileEx`、`UnlockFileEx`（`fcntl` 记录锁，moresampler 多进程共享 `desc.mrq` 时使用）、`GetModuleFileNameW`（返回 exe 的客体路径，moresampler 据此找到 `moreconfig.txt`）。
- **编码**：`MultiByteToWideChar`、`WideCharToMultiByte` 支持 `CP_UTF8` 与 `CP_ACP`（`CP_ACP` 按 UTF-8 处理）；`IsDBCSLeadByteEx` 返回假。
- **其他**：`GetCommandLineW`、`CommandLineToArgvW`（按 Windows 的引号与反斜杠规则）、`PathIsDirectoryW`、`MessageBoxW`（输出到 stderr，返回 `IDOK`）、`GetStartupInfoA`（清零）、`GetSystemTimeAsFileTime`、`QueryPerformanceCounter`、`GetTickCount`、`Sleep`、`TerminateProcess`、`OutputDebugStringA`（调试开关打开时输出）、`IsDebuggerPresent`（假）、`GetModuleHandleA/W`（本 exe 返回 `0x400000`，其他返回空）、`GetProcAddress`（MinGW 运行库会查找 `libgcc_s_dw2-1.dll` 等，返回空）。

### 5.7 路径

客体看到的是 Windows 路径。采用与 Wine 相同的映射：主机路径 `/a/b` 对客体呈现为 `Z:\a\b`；客体传入的路径在文件 API 中反向映射（`Z:\` 前缀去掉并把 `\` 换成 `/`；相对路径只替换分隔符；`nul` 映射为 `/dev/null`）。命令行参数中以 `/` 开头的参数视为主机绝对路径并转换为 `Z:\…` 形式，其余原样传递。`GetModuleFileNameW` 与 `_wgetcwd` 返回映射后的路径。

## 6. 容易造成输出不一致的原因（先查这些）

1. x87 控制字未设为 `0x27F`（5.2）。
2. `rand` 未按 msvcrt 算法实现。
3. 文本模式与二进制模式的换行转换。
4. 导入的 libm 函数（5.5）的末位差异：先定位到具体调用再处理，不得笼统归因于浮点。
5. 多线程合成的执行顺序（`multithread-synthesis` 配置）：比较时先关闭多线程，一致后再打开。
6. 文件中写入的时间戳：`desc.mrq` 的每个条目含写出时刻的 Unix 时间戳（frqeditor-reverse 实测，`third-party/mrq/mrq.h`），比较时排除该字段；`.llsm` 是否含时间戳须先确认。

## 7. 环境

- Windows：`D:\usr\tools\miniconda3\envs\venv1\python.exe` 有 `pefile`。Ghidra 12.0.4 在 `D:\usr\tools\ghidra_12.0.4_PUBLIC`，需要时可导入 exe 分析（为新仓库建立单独的 Ghidra 项目）。MSVC 2022 可用于在 Windows 上编译辅助工具。
- WSL：`Ubuntu-24.04`（x86_64，gcc 13）。**当前缺少 32 位编译支持**（`gcc -m32` 报找不到 `Scrt1.o`、`libgcc`），且 WSL 中 `sudo` 需要密码，须请作者执行：`sudo apt install gcc-multilib g++-multilib`（里程碑 2 另需 `qemu-user-static` 与 FEX-Emu 或 box64）。
- Docker Desktop 已安装（当前停止），可作为备选环境。

## 8. 验收（里程碑 1）

1. 在 WSL 中构建加载器：`moreloader <moresampler.exe> <参数…>`。
2. 准备测试数据：一个小型音源副本（例如 Extra-Jinkela 中的数个 wav 与 `oto.ini`，原件在 `C:\Users\user\Downloads\歌声合成软件 UTAU v0.4.18 完整汉化版【修复4.5】\…\UTAU\voice\Extra-Jinkela`，只复制，不得写入原目录），以及 `moreconfig.txt`（`resampler-compatibility on`、`multithread-synthesis off`、输出 44100 Hz 16 位）。
3. 以相同参数分别在 Windows 原生与 moreloader 上运行，比较全部输出文件：
   - 频率表生成：`"<wav>" nul 100 100 GN 0 50`（frqeditor 的生成命令），比较 `desc.mrq`（排除时间戳）与 `.llsm`。
   - 渲染：UTAU resampler 约定的 13 个参数，例如 `in.wav out.wav C4 100 "" 0 500 0 0 100 0 !120 AA#5#`，比较 `out.wav`。
   - wavtool 模式：按 moresampler 的 wavtool 参数串接两个音符，比较输出。
4. 通过条件：输出逐字节相同（时间戳字段除外）。不一致时按第 6 节定位，并在 `docs/` 中记录原因与处理。
5. 文档：`docs/` 下的设计说明与工作日志（每步做了什么、问题、现状、下一步），以及 README（构建、用法、许可证说明：moresampler 由用户自备）。

## 9. 参考

- MinGW-w64 运行库源码：`crtexe.c`（启动顺序）、`pseudo-reloc.c`（`VirtualQuery`、`VirtualProtect` 的用法）、`tlssup.c`（TLS 回调）、winpthreads（线程与 TLS 的用法）。
- Wine：`dlls/msvcrt`（msvcrt 语义、`FILE` 布局、printf 格式）、`dlls/ntdll/unix/signal_i386.c`（LDT 与 FS 的设置）。
- Tavis Ormandy 的 loadlibrary（GPL-2.0，Linux 上的 32 位 PE 加载器）：只参考思路，许可证不兼容时不得复制代码。
- moresampler 的 `readme.txt`（命令行模式与配置项）。
