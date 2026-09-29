# moresampler.exe OpenMP 线程数与分析的并行区域

逆向证据记录，按 [`reverse-evidence-template.md`](../reverse-evidence-template.md) 整理。定位过程见 [`20260929-render-comparison.md`](20260929-render-comparison.md) 第 7.3 节。

## 范围

- Binary：`moresampler.exe` 0.8.4（32 位，MinGW GCC 6.2.1）。
- 主题：分析阶段的线程数从何而来，以及为什么输出随线程数变化。
- 本次回答：线程数的设置点、`multithread-synthesis` 的取值、使输出随线程数变化的并行区域与机制。
- 本次不回答：`stftForwardParallel` 的参数语义与调用者，其余 11 个并行区域的内容，合成阶段的并行。

## 逆向锚点

- 函数：
  - `0047c190 FUN_0047c190_gomp_count_avail_process_cpus`：以 `GetProcessAffinityMask` 取进程掩码，返回其中的位数，失败时返回 1。
  - `0047c1f0 FUN_0047c1f0_gomp_gomp_init_num_threads`：以上述位数初始化 ICV `nthreads_var`（`0x4aaf28`）。
  - `0047c210 FUN_0047c210_gomp_gomp_dynamic_max_threads`：返回处理器数与 `nthreads_var` 中的较小者。
  - `0047c250 FUN_0047c250_gomp_omp_get_num_procs`：跳转到 `0x47c190`。
  - `00479d60 FUN_00479d60_gomp_omp_set_num_threads`：取当前任务的 ICV，写入 `n > 0 ? n : 1`。
  - `0047a590 FUN_0047a590_gomp_GOMP_parallel`：确定线程数（`0x47a3c0`）、建立线程组（`0x47aa40`）、启动线程组（`0x47abf0`）、在当前线程调用 `fn(data)`、结束并行区域（`0x47a510`）。
  - `004195a0 FUN_004195a0_mainResampler`：resampler 模式的主函数（字符串 `main_resampler: …`）。
  - `00419e20 FUN_00419e20_mainWavtool`：wavtool 模式的主函数（字符串 `main_wavtool: …`）。
  - `0044e2dc FUN_0044e2dc_stftForwardParallel`：把 11 个参数存入结构，以 `GOMP_parallel(0x44d6a6, &结构, 0, 0)` 执行。
  - `0044d6a6 FUN_0044d6a6_stftForwardParallel_ompFn`：并行区域的函数体，逐帧加窗、FFT（`0x42f7b0`）、求幅度与相位。原先未被 Ghidra 识别为函数，本次以 `CreateFunctionAt.java` 建立。
- 调用链：
  - `mainResampler -> omp_get_num_procs -> count_avail_process_cpus -> GetProcessAffinityMask`
  - `mainResampler -> omp_set_num_threads`，随后调用分析与读取函数 `0x412660`
  - `stftForwardParallel -> GOMP_parallel -> stftForwardParallel_ompFn`（第 60 步中 6 次）
- 关键地址：
  - `0x419ae5`：`omp_set_num_threads(omp_get_num_procs())`，位于调用 `0x412660` 之前，无条件执行。
  - `0x419b3b`：`multithreadSynthesis == 0` 时 `omp_set_num_threads(1)`，位于调用 `0x412660` 之后。
  - `0x41a3d8`：wavtool 模式 5 中 `multithreadSynthesis == 2` 时以处理器数为线程数，否则为 1。
  - `0x407861` 至 `0x407cb2`：moreconfig 的解析，`multithread-synthesis` 的值为 `on` 时记 1、`full` 时记 2、其余记 0。
  - `0x44d717` 至 `0x44d74e`：静态调度的划分（`0x47a6e0` 与 `0x47a710` 分别返回线程数与线程号，未命名），`q = n / nthreads`，`r = n % nthreads`，线程号小于 `r` 的线程多分一帧，各线程处理连续的一段。
- 相关数据：
  - `DAT_004de080_multithreadSynthesis`：上述 0、1、2。
  - `0x4aaf28`：libgomp 的全局 ICV `nthreads_var`。

## 证据来源

- Ghidra 伪代码：`decomp/moresampler/openmp/FUN_004195a0_mainResampler.c`、`FUN_0044e2dc_stftForwardParallel.c`、`FUN_0044d6a6_stftForwardParallel_ompFn.c`。libgomp 的函数只核对，不导出。
- 汇编核对：`objdump -d` 的 `0x47c190` 至 `0x47c255`、`0x419ae5` 至 `0x419b54`、`0x41aad0` 至 `0x41aadd`、`0x4078a2` 与 `0x407c88` 至 `0x407cb2`。
- 字符串：exe 中有 `OMP_NUM_THREADS`、`GOMP_SPINCOUNT`、`OMP_DISPLAY_ENV` 等 libgomp 的环境变量名。配置键 `multithread-synthesis`（`0x4ac398`）、值 `on`（`0x4ac33c`）与 `full`（`0x4ac3c4`），均以 UTF-16 存储。
- 公开资料：GCC 6 的 libgomp 源码，`config/mingw32/proc.c`（`count_avail_process_cpus`、`gomp_init_num_threads`、`gomp_dynamic_max_threads`、`omp_get_num_procs`）、`env.c`（`omp_set_num_threads`）、`parallel.c`（`GOMP_parallel`）。
- 运行时观察（实测，WSL 与 overworld 上的 gdb，脚本 `work/render4/compat-off-mt-off/gomp.py`）：
  - 第 60 步执行 8 个并行区域：`0x416129`、`0x417602`、`0x44e356`（6 次）、`0x45223b`、`0x453d70`、`0x453dda`、`0x4564e0`（2 次）、`0x45b8b9`（5 次），`GOMP_parallel` 的线程数参数均为 0。
  - 只把 `0x44e356` 的线程数参数改为 8，输出与 8 个处理器时相同。改其余任何一处，输出不变。
  - `0x44d6a6` 入口处，主线程的控制字为 `0x37F`，工作线程均为 `0x27F`。所有线程统一为 `0x37F` 或 `0x27F` 时，16 个与 8 个线程的输出逐字节相同。
  - `--trace-imports`：16 个处理器时启动 15 个工作线程，8 个时 7 个。`rand` 只由主线程调用，两种情况下都是 45212 次。
  - `OMP_NUM_THREADS` 取 1、2、8、16 时，第 60 步的输出不变。
- 探针（实测）：`moreloader/tests/probe/msvcrt/MsvcrtProbe.cpp` 的 `probeThreads`，黄金数据 `moreloader/tests/auto/data/msvcrt/threads.txt`。Windows 上以 `CreateThread` 与 msvcrt.dll 的 `_beginthreadex` 创建的新线程，控制字均为 `0x27F`、MXCSR 均为 `0x1F80`，与创建者的控制字（`0x37F` 或 `0x07F`）无关。

## Confirmed

- 分析阶段的 OpenMP 线程数等于 `GetProcessAffinityMask` 返回的进程掩码的位数，在 `0x419ae5` 设置，与 `multithread-synthesis` 无关。
- `multithread-synthesis` 为 off 时，分析之后才把线程数改为 1（`0x419b3b`）。
- 运行时的 `omp_set_num_threads` 覆盖 `OMP_NUM_THREADS`。
- `stftForwardParallel_ompFn` 以静态调度把帧分为连续的段，第一段由主线程计算。
- 主线程的控制字为 `0x37F`（`_fpreset` 在 `0x48edb0` 执行 `fninit`），工作线程为 `0x27F`（Windows 新线程的初始值，不继承创建者）。
- 输出随处理器数变化的原因只有主线程与工作线程的控制字不同：统一控制字后，第 60 步与完整工程（兼容关、多线程关）在 16 个与 8 个处理器下的输出相同。

## Inferred

- `0x44e2dc` 为短时傅里叶变换的正变换，所属的库未确定。
  - 依赖证据：窗函数名 `blackman_harris` 的字符串比较、逐帧的中心与长度数组、FFT 之后求幅度（平方和开方）与相位（两参数的反正切）、相位展开的循环。
  - 与 ciglet 的核对（公开资料，[Sleepwalking/ciglet](https://github.com/Sleepwalking/ciglet) 的 `895ba9b1c0eabee83d3544208bbc82420efa3206`，2019-09-08）：`cig_stft_forward` 同为 `#pragma omp parallel for` 的逐帧循环，窗函数经 `get_window` 按名称选取，帧的放置方式（前半帧放在缓冲区末尾、后半帧放在开头）相同。不同之处：ciglet 有 13 个参数，每帧以 `calloc` 分配缓冲区，只求幅度与相位。moresampler 的函数在 Ghidra 中识别为 12 个参数，缓冲区在栈上（约 512 KB），另有相位展开与 -100 的下限。ciglet 中没有其他函数与之相符。二者可能有共同来源（作者相同的 libllsm 或 ciglet 的早期版本），但不是同一函数，因此不以 `ciglet_` 前缀命名。
- 输出随控制字变化的行为不是 moresampler 有意的设计。
  - 依赖证据：精度的分配只取决于帧落在哪个线程，与算法无关。
  - 还缺什么：无法从二进制中确认意图。

## 伪代码整理动作

- 已修正的函数名：上述 10 个函数，其中 6 个为 libgomp，以 `gomp_` 为前缀。
- 已修正的数据名：`DAT_004de080_multithreadSynthesis`。
- 尚未修正但会影响理解的点：`stftForwardParallel` 与 `ompFn` 的参数名与参数结构，未命名的 `0x42f7b0`（FFT），以及 `GOMP_parallel` 调用的 4 个未逐个核对、未命名的内部函数。

## 对包装层的约束

- `GetProcessAffinityMask` 按主机进程可用的处理器数返回掩码，不另设限制（作者 2026-09-29 决定），因此输出与在同一台机器上运行 Windows 版相同。
- 新线程的控制字必须为 `0x27F`，不得继承创建者的控制字（`GuestThread.cpp`）。
- 与 Windows 的逐字节比较须在两侧处理器数相同时进行（TaskSpec 第 6 节第 7 条、第 8 节）。

## 待办

- [x] 与 ciglet 的 `cig_stft_forward` 核对：不是同一函数，见 Inferred。

## 备注

- libgomp 的函数只核对身份，不逆向，符合 [`ReverseGuide.md`](../ReverseGuide.md)「静态链接的库」的规定。
