# 逆向指导

本文档记录本仓库的 Ghidra 逆向工作流与命名规则。规则取自 frqeditor-reverse（`E:\Gitee-skyint\frqeditor-reverse\docs\1-reverse-guide.md`）与 ra2-reverse（`E:\Gitee-skyint\ra2-reverse\docs\1-reverse-guide.md`），按本仓库的目标作了调整。

## 逆向的目的

本仓库不重新实现 moresampler，而是为它提供运行环境。逆向服务于两个问题：

1. **moresampler 如何使用每个导入函数**：传入的参数、文件的打开模式、读取了结构体的哪些字段、依赖了哪些返回约定。包装层只需实现被使用的语义，但被使用的语义必须与 Windows 完全一致。
2. **输出不一致时，差异产生于哪个函数**：按 [`TaskSpec.md`](TaskSpec.md) 第 6 节的清单排查之后，须把差异定位到具体函数与具体调用，再判断是包装层的哪个语义有误。

因此，逆向的产物是证据记录与包装层中的注释，而不是 moresampler 函数的 C++ 重实现。但「先修 Ghidra 工程，再下结论」的流程与 frqeditor-reverse 相同，不得跳过整理符号、类型与注释这一步。

## 分析对象

| 项 | 值 |
|---|---|
| 原件 | `E:\temp-repos\UTAU\UTAU\tools\moresampler 0.8.4\moresampler.exe`，只读，不在原位运行 |
| 副本 | `work/moresampler/moresampler.exe`，由 `scripts/PrepareWork.ps1` 生成 |
| SHA-256 | `29e88e1c79645a46d3eb346e0b5209cd4e32f4f800a8fe1db7435c0e9e4ebdf2` |
| 编译器 | MinGW，`GCC: (GNU) 6.2.1 20161119`，C 运行库为 `msvcrt.dll`，x87 浮点 |
| 其余事实 | 见 [`TaskSpec.md`](TaskSpec.md) 第 3 节 |

## Ghidra 基础设施

### 工程

- 工程：`C:\Users\user\ghidra-projects\projects\moresampler\moresampler_exe_084.gpr`，程序名 `moresampler.exe`，导入的是原件（内容与副本相同）。
- 无界面运行时工程不能同时在 GUI 中打开，否则会被锁住。
- 无界面运行：`D:\usr\tools\ghidra_12.0.4_PUBLIC\support\analyzeHeadless.bat`，运行前设 `JAVA_HOME=C:\Users\user\.jdks\temurin-21.0.11`。

### Ghidra Bridge

- 先打开工程，再加载 `CodexGhidraBridge` 扩展。每个打开的程序一个会话，端口每次可能不同。会话信息（端口、令牌、程序名）记录在 `%USERPROFILE%\.config\ghidra-re\bridge-sessions\<session_id>.json`。
- **本仓库通过 `scripts/BridgeCall.py` 调用 bridge**：`python scripts/BridgeCall.py /endpoint '<json>'`。脚本按工程名与程序名查找会话，因此 Ghidra 重启后不必修改脚本。批量操作可在 Python 中 `from BridgeCall import call`。
- 耗时较长的 Ghidra 脚本使用 `scripts/BridgeRun.py <script.java> [arg ...] [--timeout <sec>]`，默认超时 1200 秒。`ghidra-re` CLI 的请求超时只有 30 秒。
- 解释器使用 `D:\usr\tools\miniconda3\envs\venv1\python.exe`，其中有 `requests` 与 `ghidra-re`。
- **本机环境变量中配置了 HTTP 代理**，发往 `127.0.0.1` 的请求也会被交给代理。`BridgeCall.py` 已关闭 `trust_env`；直接调用 `ghidra-re` CLI 时须设置 `NO_PROXY=127.0.0.1,localhost`，直接使用 `curl` 时加 `--noproxy '*'`。
- **在 Git Bash 中调用时**，以 `/` 开头的参数（`/functions/search`、`path=/...`）会被转换成 Windows 路径，须先设置 `MSYS_NO_PATHCONV=1`。PowerShell 不受此影响。

### `/script/run` 约定

- bridge 执行 Ghidra Script Manager 中已注册脚本目录下的 `GhidraScript`，并回传 `println` 输出。
- 已注册的共享脚本目录：`E:\Gitee-skyint\ghidra_scripts\common`、`E:\Gitee-skyint\ghidra_scripts\ra2`、`E:\Gitee-skyint\ghidra_scripts\frqeditor`。本仓库新增的脚本放在 `E:\Gitee-skyint\ghidra_scripts\common`（与业务无关）或新建的 `E:\Gitee-skyint\ghidra_scripts\moresampler`，并在本文档中登记。
- 请求体形如 `{"script":"DumpInstructions.java","write":true,"args":["0x401000","32"]}`。`args` 为位置参数数组。`write=true` 在 bridge 侧是必要的，即使脚本本身只读。
- 常用脚本（均位于 `common`，用法见 frqeditor-reverse 的 `docs/1-reverse-guide.md`「当前保留的通用脚本」）：
  - 导出与检查：`DecompileFunctionCompat.java`、`BatchDecompileToDir.java`、`DumpFunctionLocals.java`、`DumpHighSymbols.java`、`DumpInstructions.java`、`DumpInsnRange.java`、`ListCodeXrefsTo.java`。
  - 标量与字符串：`DumpTerminatedStrings.java`、`DumpWideStrings.java`、`DumpDoubleScalarsCompat.java`、`DumpPointerScalarsCompat.java`、`ForceDoubleData.java`；`ra2` 中的 `DumpUintTable.java`。
  - 改名与签名：`RenameHighVars.java`、`RenameLocalsByName.java`、`RenameSymbolAtAddress.java`、`BulkRenameFunctions.java`、`BulkRenameVars.java`、`SetFunctionSignature.java`、`BulkSetSignatures.java`、`ApplyFunctionSpec.java`、`SetFtolSignature.java`。
  - 类型：`EnsureStruct.java`、`EnsureEnum.java`、`AddEnumMembers.java`、`SetStructFieldType.java`、`ApplyDataTypeAtAddr.java`、`ApplyEnumEquatesInFunction.java`、`ApplyEquateAtCallArg.java`。
  - 分析辅助：`RunFidAnalyzer.java`、`ForceFid.java`、`ListUnnamedFunctions.java`、`TrivialProbe.java`；`ra2` 中的 `DumpAddressXrefs.java`、`DumpCallGraph.java`、`ScanStructFieldRefs.java`。

### 反汇编的辅助手段

- WSL 中的 `objdump -d -M intel work/moresampler/moresampler.exe > work/moresampler.dis` 产出全文反汇编，适合以正则检索某类指令（例如 `fs:` 前缀、`fldcw`）或统计导入函数的引用次数。
- `work/iat.txt` 列出每个导入函数的 IAT 地址，由 `pefile` 生成。msvcrt 的函数多经由 `jmp [iat]` 形式的跳转桩调用，统计调用点时须先找到跳转桩，再统计 `call <跳转桩>`。
- 这两个文件是可再生的分析产物，位于 `work/`，不纳入版本库。

## `decomp/` 目录

- 导出的伪代码放在 `decomp/moresampler/<slice>/`，`<slice>` 为主题名，例如 `startup`、`resampler-args`、`mrq-database`、`wavtool`。不要把导出的 `.c` 堆在 `decomp/` 根目录。
- 文件名与 Ghidra 中的函数名一致：`<函数名>.c`，形如 `FUN_<地址>_<语义名>.c`，不要再在前面加多余的地址前缀。
- `decomp/` 下的文件视为缓存产物。任何改名、改类型、改签名完成后，都要重新导出对应的 `.c` 文件，不保证自动同步。
- 类型与函数的批量规格文件（供 `ApplyFunctionSpec.java` 等脚本使用的 `.tsv`）放在对应的切片目录中。
- **伪代码只作为分析材料**，不复制进 `moreloader/` 的源代码，也不粘贴进 `docs/` 的文档。文档以文字、表格和伪代码描述行为。

## 推荐流程

- 第一轮只选一个足够小的切片，标准是：有明确的字符串、导入调用或状态转换作为锚点；调用链较短；涉及的数据结构局部且可控；修完后能明显改善后续阅读。
- 第一轮的目标是在 Ghidra 工程内修好函数名、参数名与局部变量名、函数签名、关键类型、枚举与结构体字段，以及必要的注释。
- 修复范围控制为「目标函数加相邻调用链」：目标函数本身，直接调用它的 1 到 2 个函数，它直接调用的 1 到 3 个关键函数，以及少量相关的全局变量或结构体字段。
- 工程内修复完成后再重新导出 `decomp/` 下的 `.c`，不要只改导出的 `.c`。
- 同步用 [`reverse-evidence-template.md`](reverse-evidence-template.md) 写一份证据记录，固定函数锚点、`Confirmed` 事实、`Inferred` 推断，以及对包装层的约束。
- 包装层中依据逆向结论实现的语义，在实现附近的注释中回指调用点地址或证据记录，例如「moresampler opens \c desc.mrq with mode \c w+b at 0x41dca8」。

## Ghidra 命名与同步规则

- 改名必须先改 Ghidra 数据库里的真实符号，不要只改导出的 `.c`。
- 函数名保留地址前缀，格式统一为 `FUN_<地址>_<语义名>`，语义名采用小驼峰。
- 数据名保留 `DAT_` / `_DAT_` 和原地址前缀。
- 参数与局部变量在有依据的情况下改为其实际语义名。临时变量与循环变量也尽量改为 `i`、`j` 等现代程序中常用的形式。
- 如果有足够的证据，在 Ghidra 中建立 enum 替换伪代码中的纯数值（例如 `LOCKFILE_EXCLUSIVE_LOCK`、`WAIT_OBJECT_0`、`DLL_PROCESS_ATTACH`、`_IOREAD`），建立 struct 替换伪代码中的字节偏移读写（例如 msvcrt 的 `FILE`、`CRITICAL_SECTION`、`WIN32_FIND_DATAW`、TEB）。
- 客体可见结构的布局以包装层中带静态断言的定义为准。Ghidra 中建立的同名结构与之一致，二者不一致时先核对，再同时修正。

### 字符串与常量的硬规则

**给任何 `s_*` / `DAT_*` 改名之前，必须先导出其字节内容**：

```
DumpTerminatedStrings.java    null 结尾的窄字符串
DumpWideStrings.java          UTF-16 字符串
DumpDoubleScalarsCompat.java  double 常量
DumpPointerScalarsCompat.java 指针常量
DumpUintTable.java            uint 数组（位于 ghidra_scripts/ra2）
```

理由：**命名是隐式断言**。名字一旦写成 `s_FopenModeBinary`，后续所有阅读者都会把它当作「这是 `rb`」的既成事实，并在包装层中照搬，而没有任何机制反向校验。frqeditor-reverse 的实例：`0x0041417c` 实际是 `"r"` 而非 `"rb"`，被误名后导致重实现的文件模式错误。本仓库已知的实例：`0x40d5b5` 处 `_wfopen` 的模式字符串是 `"rw"`，这不是合法的 msvcrt 模式，其行为须以实测决定，不能按名字推断。

正确流程：先导出字节值，再以字节值本身为命名根据（例如 `s_fopenMode_wplusb`），包装层中需要比较时使用同样的字节值。

## 变量与反编译原则

- 遇到被 Ghidra 合并成同一栈槽的多个生命周期变量，优先保留最常用类型，并在异类型使用点补注释说明。
- 遇到逻辑读不通的反编译结果，回到机器码核对，不要直接相信伪代码字面量。
- printf 风格变参调用点附近出现的 `in_stack_*` 往往是被打印的实参，不是新的数据流。
- MinGW 的 `___chkstk_ms`（`0x48fa00`）在分配大栈帧前按页探测，调用前以 `EAX` 传入大小，调用后的 `sub esp, eax` 才是真正的栈分配。Ghidra 可能把它渲染成无参调用。

## x87 说明

- moresampler 由 GCC 6 以 x87 浮点编译，没有 SSE2 路径。x87 是反编译器最容易失真的区域之一。
- 若调用点前存在明显的 FPU 计算，而反编译器把辅助函数渲染成无参调用，用 `SetFtolSignature.java` 补 `ST0` 原型，再重新导出伪代码。
- 若补完原型后结果仍异常，回到汇编核对 `FLD`、`FILD`、`FMUL`、`FDIV`、`FISTP`、`FRNDINT`、`FLDCW` 等关键指令，不要把逻辑错误归咎于浮点精度残差。
- 精度控制字影响所有 x87 运算。Windows 的初始控制字为 `0x27F`（53 位精度），Linux 为 `0x37F`（64 位精度）。GCC 的取整序列会临时改写控制字（`or ah, 0xc` 后 `fldcw`），核对时注意区分临时值与初始值。

## 静态链接的库

moresampler 静态链接了大量库，这些库不是逆向对象。识别之后在 Ghidra 中标名即可：

1. **不要逆它、不要重写它、不要导出它的 `.c`。**
2. 改名为 `FUN_<地址>_<库前缀>_<原名>`，让后续 session 一眼识别为库函数。改名前必须先反编译核对身份，不要只凭调用形态猜测。库前缀如下：

| 库 | 前缀 | 来源与识别依据 |
|---|---|---|
| MinGW 运行库（`crtexe.c`、`pseudo-reloc.c`、`tlssup.c`、`___chkstk_ms` 等） | `crt_` | mingw-w64 源码 |
| msvcrt 导入的跳转桩（`jmp [iat]`） | `imp_` | 导入表 |
| winpthreads | `pthread_` | mingw-w64 的 winpthreads 源码 |
| libgcc（`__udivdi3`、`__umoddi3`、DWARF 注册等） | `gcc_` | libgcc 源码 |
| Lua（oto 生成模式使用） | `lua_` | readme 0.7.2 的更新记录；`setlocale`、`system`、`signal` 等导入的调用者 |
| libllsm、libpyin、libgvps、liblrhsmm、WORLD、ciglet 等 | `llsm_`、`pyin_`、`gvps_`、`lrhsmm_`、`world_`、`ciglet_` | `work/moresampler/docs/` 中的许可证与各库的公开源码 |

3. 这些库调用导入函数的方式仍然属于逆向的范围。例如 winpthreads 创建线程时以 `CREATE_SUSPENDED` 调用 `_beginthreadex` 再调用 `ResumeThread`，这决定了包装层必须实现挂起创建。结论记入证据记录，库函数本身不必逐行整理。
4. Lua 的代码路径（oto 生成）不在验收范围内。遇到只被 Lua 使用的导入函数，记录其用途后按最小语义实现。

## 证据记录

- 开始依据逆向结论修改包装层之前，先用 [`reverse-evidence-template.md`](reverse-evidence-template.md) 固定当前函数、调用链或小切片的逆向证据，存为 `docs/<yyyymmdd>-<task>.md` 的一节或单独的文件。
- 证据记录优先引用具体函数、地址、调用点和指令区间，不要只写「看了 strings、symbols 或某个大文件」。
- `Confirmed` 与 `Inferred` 必须分开写。没有直接证据支持的行为不要混入 `Confirmed`。

## 实测

静态分析得出的结论，尽量以实际运行验证。

- **Windows 原生运行**只在 `work/` 下的副本上进行，音源同样只用 `work/` 下的副本。moresampler 会在音源目录中写入 `desc.mrq` 与 `.llsm`，不得在作者原有的音源目录中运行。
- **msvcrt 的行为**以探针程序测量：以 MSVC 编译的 32 位程序 `LoadLibrary("msvcrt.dll")` 后调用被测函数，输出作为黄金数据保存在 `moreloader/tests/`，由自动测试比对包装层的实现。探针的源码与生成步骤一并保存，在日志中写明可重复执行的步骤。
- **加载器中的运行时观察**使用 `moreloader --trace`（打印每次导入调用），以及加载器在 `SIGSEGV` 时打印的客体 `EIP` 与最近一次导入调用。
