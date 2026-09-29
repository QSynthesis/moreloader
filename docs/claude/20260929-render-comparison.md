# 2026-09-29 真实工程的渲染比较

## 1. 来源

helloutau 按任务书（`.cache/claude/helloutau-render-comparison-task.md`）交付了脚本生成工具 `ustrender` 与一次完整的比较，交接见 `E:\GitHub\helloutau\.cache\claude\2026-09-29-render-comparison-delivery.md`，说明见 helloutau 的 `docs/claude/render-comparison.md`。

- 工程：`New Geping UTAU Database` 的一个工程，203 步（96 次 resampler、106 次 wavtool、1 次拼接），`resampler-compatibility off`、`multithread-synthesis on`。
- helloutau 的结果：Windows 可重复；Linux（WSL，moreloader）与 Windows 相比，6 次 resampler 的 `.llsm.tmp` 不同（第 16、104、162、181、190、196 步），最终 wav 从第 79059 个样本起不同。第 16 步单独运行即可复现。

## 2. 定位

第 16 步：`ei_.wav <缓存> G4 100 B0 129 250 105 182 100 0 !134 /N/i/y/8AA#36#ABANAtBf`。

1. 在 `work/repro16/` 下以同一份音源副本分别在 Windows 原生与 WSL 中运行（实测）：`.llsm.tmp` 从第 166 字节起不同，与 `multithread-synthesis` 无关。
2. `.llsm.tmp` 的偏移表之后，第一条记录的长度相差 35 字节。该记录中基频相关的值 Windows 为 219.1（及其 3 倍 657.35，谐波数 19），moreloader 为 341.5（1024.37，谐波数 12）：第一帧的 f0 不同。
3. f0 由音高曲线决定。第 16 步的音高曲线以 `/` 开头（UTAU 的 base64 中 `/` 为 63，即负的音分）。加载器的规则是「以 `/` 开头的参数视为主机绝对路径并转换为 `Z:\…`」（`guestArgumentFromHost`），moresampler 收到的是 `Z:\N\i\y\8AA#36#ABANAtBf`。
4. 清单中音高曲线以 `/` 开头的 resampler 步骤恰好是第 16、104、162、181、190、196 步，与 helloutau 报告的 6 步完全相同（实测，`win/manifest.json`）。

排查中排除的其他可能（实测）：`setlocale(LC_ALL, "ja_JP.utf8")`（`0x407e92`，msvcrt 同样返回 NULL，不切换 locale）、`atof`（9 次，均为整数）、x87 控制字（moresampler 的 `_fpreset` 在 `0x48edb0` 执行 `fninit`，Windows 上同样为 `0x37F`）、OpenMP 的线程数、`rand` 的初始状态（主线程为 1，以 32 位探针在导入 user32、shell32、shlwapi 的进程中实测）。

## 3. 修复

`guestArgumentFromHost(argument, exists)`：以 `/` 开头的参数，只有本身存在于主机，或位于根目录以外的已有目录中时，才转换为 `Z:\…`。输入文件存在；输出文件的目录由调用方预先建立（helloutau 的脚本先建立目录，moresampler 本身也不建立目录）。音高曲线不会以存在的路径出现：`/N/i/y/…` 的目录不存在，`/2AA#30#` 的目录是根目录。测试以清单中的音高曲线覆盖，放回缺陷可被检出。

推断：若某个音高曲线恰好构成一个已有目录中的路径（例如以 `/bin/` 开头），仍会被误转换。UTAU 的音高曲线由 base64 字符组成，这种情形需要连续多个值恰好拼成已有目录名，未见实例。

## 4. 复现中的两处干扰

1. **Git Bash 的参数转换**：从 Git Bash 启动 Windows 程序时，以 `/` 开头的参数被当作 MSYS 路径转换。第一次在 Windows 上复现时音高曲线因此被改写，得到的 Windows 结果（18198 字节）同样是错的。Windows 一侧须设 `MSYS_NO_PATHCONV=1`，路径参数以 `cygpath -w` 给出。
2. **音源副本的修改时间**：moresampler 在 `0x4126fa` 与 `0x41272e` 分别以 `_wstat` 取 wav 与 `.llsm` 的修改时间，wav 较新时（`0x41273d`）报告「The .wav file is newer than the data record」，并在本次运行中重新分析；`0x41295c` 以 `desc.mrq` 条目的时间戳作同样的比较。以不保留时间的 `cp -r` 复制音源时，wav 可能比 `.llsm` 新，第一次运行的输出因而不同（推断：重新分析时合成使用内存中的分析结果，而不是从文件读回的量化结果）。复制音源时须保留修改时间（`cp -p`）。32 位 msvcrt 的 `_wstat` 返回的修改时间与 moreloader 一致（实测）。

## 5. 结果（实测）

- 第 16 步单独运行，`multithread-synthesis` 开与关：`.llsm.tmp` 与 wav 逐字节一致，与 helloutau 的 Windows 快照相同。
- 完整工程重跑（`~/moreloader-compare-render/linux`，从不含派生文件的 `voice-linux` 恢复音源，22 秒）：以 helloutau 的 `compare_snapshots.py` 与 Windows 的快照比较，203 步全部相同，最终 wav 相同；最终 `desc.mrq` 只有条目的时间戳不同（去掉时间戳后相同，原始数据中不同的 90 字节都在时间戳中）。快照在 `work/render/`。

## 6. `moreconfig.txt` 的四种组合

每种组合在 `work/render4/<组合>/` 中独立进行：以 `ustrender copy-voice` 从原始音源复制一份不含派生文件的副本，Linux 一侧的音源在 Windows 运行之前以保留修改时间的方式复制（`voice-linux`）；两份脚本从同一份副本生成，`compare-manifests` 确认 203 步相同；Windows 以 `cmd.exe` 运行 `temp.bat`，WSL 运行 `temp.sh`；以 `compare_snapshots.py` 逐步比较。

| `resampler-compatibility` | `multithread-synthesis` | Windows | WSL | 结果（实测） |
|---|---|---|---|---|
| off | on | 11 秒 | 22 秒 | 203 步、最终 wav 相同 |
| off | off | 11 秒 | 20 秒 | 203 步、最终 wav 相同 |
| on | off | 14 秒 | 23 秒 | 203 步、最终 wav 相同 |
| on | on | 13 秒 | 24 秒 | 203 步、最终 wav 相同 |

- 四种组合的 `desc.mrq` 去掉时间戳后都相同。
- 设置确实生效：兼容模式开时 resampler 输出真正的 wav（第 16 步 24108 字节），关时为 180 字节的占位文件；四种组合在 Windows 上的最终 wav 互不相同，多线程开与关的结果也不同。
- 两侧的拼接一步在四种组合下都被跳过：moresampler 在兼容模式开与关时都不写 `.whd`、`.dat`（实测）。helloutau 的说明中「非兼容模式下不写」一句只覆盖了一半。

## 7. FEX 上的结果与处理器数

### 7.1 FEX（spark，20 个逻辑处理器）上的完整工程（实测）

以 `remote4.sh` 在 spark 上经 FEX 运行同一工程，结果在 `work/render4/<组合>/spark-snap`，汇总在 `work/render4/fex.log`：

| `resampler-compatibility` | `multithread-synthesis` | 首个不同的步骤 | 最终 wav |
|---|---|---|---|
| off | off | 第 60 步 | 7 个样本不同，最大差 1 |
| off | on | 第 60 步 | 474 个样本不同，最大差 21 |
| on | off | 第 60 步 | 7 个样本不同，最大差 1 |
| on | on | 第 24 步 | 474 个样本不同，最大差 21 |

第 60 步为 `31__en_D4` 的分析，`.llsm.tmp` 有 247 字节不同。

### 7.2 第 60 步的定位

1. **确定性**（实测，`.cache/claude/tools/step60.sh`）：在 WSL 中原生运行前 59 步，打包为 `prestate60.tar`，再从这份状态分别在 WSL 原生与 spark 的 FEX 下只运行第 60 步，路径相同。原生的结果与 Windows 0 字节不同，FEX 为 247 字节，与完整运行相同。
2. **x87 超越函数的记录**（实测）：在 dp1000 上以 v11.1.2 加 `third-party/qemu` 补丁的 qemu-i386 另建一个记录每次 `fsin`、`fcos`、`fsincos`、`fptan` 的操作数与结果的版本，运行第 60 步：`fsin` 761149 次、`fcos` 294152 次、`fsincos` 297153 次，没有 `fptan`。该 qemu 的输出与 Windows 有 401 字节不同。
3. **在处理器上重放**（实测，`moreloader/tests/manual/x87/x87replay.c`）：以相同的 80 位操作数在处理器上执行同一指令。AMD Ryzen 7 9700X（即运行 Windows 参考的机器）上 1649607 个结果中 210076 个与 qemu 不同；其中约 18 万个差 1 或 2 ulp，约 3.4 万个差异很大，都位于 sin 或 cos 的零点附近。Intel Core i7-10700（overworld）上重放同一记录，Intel 与 AMD 的结果在 908347 个不同操作数中有 16506 个不同，均为 ±1 ulp，没有大的差异。
4. **66 位的 π**（实测）：处理器以 π/2 舍入到 66 位（`c90fdaa22168c234` 后接二进制 `11`）为模数做参数约化。以此常数精确约化、以 binary128 求值、舍入到 80 位的模型，与 AMD 的结果在 908347 个操作数中 902689 个相同，其余 5658 个差不超过 4 ulp，没有大的差异；对 Intel 为 893357 个相同，14990 个差不超过 4 ulp。以 64 或 65 位的 π 为模数时有 110994 个大的差异，69、70、72 位分别有 19187、10447、13713 个，完整精度（128 位）有 12922 个；66 至 68 位等价，因为 π 的第 67 至 69 位为 0。这与 Intel 手册对 `FSIN`、`FCOS` 使用 66 位 π 的说明一致（公开资料）。第 2 步的 qemu 以 binary128 直接计算，不做 66 位约化，因此在零点附近与处理器不同。
5. **替换实验**（实测，`work/qemu/pi66/fpu_helper.c`，只用于本次实验，不属于补丁）：qemu 改用 66 位约化后，`fsin`、`fcos`、`fsincos` 的结果与处理器不同的只剩 ±4 ulp 以内，第 60 步的输出仍然有 401 字节不同。继续记录 `f2xm1`（115517 次）、`fyl2x`（249267 次）、`fyl2xp1`（6897 次）、`fpatan`（214743 次），重放得到 12604 个 ±1 ulp 的差异。再让 qemu 从表中读取处理器的结果替换上述 7 种指令的全部结果（只有 106 次调用不在表中），输出与未修改的 qemu **逐字节相同**。第 60 步的输出与这些指令的末位无关；moresampler 中其余的 x87 指令（算术、`fsqrt`、`fprem`、`frndint`、`fscale`、转换）在 qemu 中以 softfloat 精确实现，moresampler 不含 SSE 指令（`objdump` 中只有一处 `ldmxcsr` 与一处 `stmxcsr`）。
6. **处理器数**（实测）：spark 有 20 个逻辑处理器，dp1000 有 8 个，运行 Windows 参考的机器与 overworld 各有 16 个。从 `prestate60.tar` 出发：
   - WSL 原生以 `taskset -c 0-7` 运行第 60 步，输出与 qemu（8 个处理器）**逐字节相同**，与 Windows 有 401 字节不同；
   - FEX 以 `taskset -c 0-15` 运行第 60 步，输出与 Windows **0 字节不同**；
   - overworld（Intel）原生运行第 60 步，与 Windows 0 字节不同。
7. **线程**（实测，`--trace-imports`）：即使 `multithread-synthesis` 为 off，第 60 步的分析也启动工作线程：16 个处理器时 `ResumeThread` 15 次，8 个处理器时 7 次。线程数来自 `GetProcessAffinityMask`（`kernel32_GetProcessAffinityMask` 以 `sched_getaffinity` 的处理器数返回掩码）。
8. **Windows 本身**（实测，`.cache/claude/tools/render4-cpus.sh off off 8`，结果在 `work/render4/compat-off-mt-off-cpu8/`）：Windows 以 `start /affinity FF` 运行整个工程，WSL 以 `taskset -c 0-7` 运行：两侧 203 步与最终 wav 全部相同。与 Windows 使用全部 16 个处理器的结果相比，从第 3 步起不同，最终 wav 有 275176 个样本不同，最大差 3039。`off on 8`（`work/render4/compat-off-mt-on-cpu8/`）的结果相同：两侧全部一致；与 16 个处理器相比从第 3 步起不同，275468 个样本不同，最大差 3039。
9. **FEX 以 16 个处理器运行完整工程**（实测，`remote4.sh <组合> spark /home/functioner <FEX> "taskset -c 0-15" spark16`，汇总在 `work/render4/fex16.log`）：四种组合的 203 步与最终 wav 全部与 Windows 相同。最终 `desc.mrq` 的首个差异位于第 512 字节，与 WSL 的比较相同（WSL 上已确认只有时间戳不同，FEX 一侧未逐字节检查）。耗时 314 至 508 秒。

结论：FEX 在第 60 步的差异，以及 qemu 在第 60 步的差异，都来自主机报告的处理器数，而不是指令的模拟。moresampler 在 Windows 上的输出本身随进程可用的处理器数而变。加载器按主机的处理器数返回掩码，行为与 Windows 相同；比较时两侧的处理器数必须相同。

### 7.3 输出随处理器数变化的机制

原先的推断「每个线程的 `rand` 状态独立」是错误的：第 60 步中 `rand` 只由主线程调用，8 个与 16 个处理器时都是 45212 次，工作线程只调用内存分配与同步函数（实测，`--trace-imports`）。机制如下。

1. **OpenMP**（静态分析）：moresampler 静态链接了 libgomp（exe 中有 `OMP_NUM_THREADS`、`GOMP_SPINCOUNT` 等字符串）。`0x47c190` 统计 `GetProcessAffinityMask` 返回的掩码的位数，为 libgomp 在 MinGW 上的处理器数函数；`0x47c1f0` 以其初始化 ICV `nthreads_var`（`0x4aaf28`）；`0x47c250` 为 `omp_get_num_procs`；`0x479d60` 为 `omp_set_num_threads`；`0x47a590` 为 `GOMP_parallel`。
2. **线程数的设置**（静态分析）：`main_resampler`（`0x4195a0`）在调用分析与读取函数 `0x412660` 之前无条件执行 `omp_set_num_threads(omp_get_num_procs())`（`0x419ae5`），之后仅当 `multithread-synthesis` 为 off 时改为 1（`0x419b3b`）。分析阶段因此总是使用全部处理器，与该设置无关。`main_wavtool`（`0x419e20`）的模式 5 仅当该设置为 `full` 时使用全部处理器（`0x41a3d8`）。moreconfig 的解析（`0x407861` 至 `0x407cb2`）把 `on` 记为 1、`full` 记为 2、其余记为 0，存于 `0x4de080`；`full` 未见于公开说明。运行时的 `omp_set_num_threads` 覆盖 `OMP_NUM_THREADS`：以 1、2、8、16 设置该变量运行第 60 步，输出均与 Windows 相同（实测）。
3. **起作用的并行区域**（实测，overworld 上的 gdb，`work/render4/compat-off-mt-off/gomp.py`）：第 60 步执行 8 个并行区域。以 16 个处理器为基准，每次只把一个区域的线程数改为 8，只有 `0x44e356`（`FUN_0044e2dc` 调用并行函数 `0x44d6a6`，执行 6 次）改变输出，改变后的输出与 8 个处理器时的 401 字节差异相同；其余 7 个区域不影响输出。
4. **该区域的工作划分**（静态分析）：`0x44d6a6` 以静态调度把 n 帧分为连续的段，每段由一个线程逐帧处理（加窗、FFT `0x42f7b0`、幅度与相位）。各帧的计算互不依赖，FFT 不使用全局表。
5. **x87 控制字**（实测，gdb 在 `0x44d6a6` 入口读取 `$fctrl`）：主线程（团队中的 0 号线程）的控制字为 `0x37F`，工作线程均为 `0x27F`。主线程的 `0x37F` 来自 moresampler 启动时的 `_fpreset`（`0x48edb0` 执行 `fninit`，Windows 上同样如此，见第 2 节）；工作线程的 `0x27F` 为 Windows 新线程的默认值，加载器按此设置（`GuestThread.cpp`）。第一段帧因此以 64 位精度计算，其余帧以 53 位精度计算。线程数改变第一段的长度，也就改变了哪些帧以 64 位精度计算。
6. **验证**（实测）：在 gdb 中把所有线程在 `0x44d6a6` 入口的控制字统一为 `0x37F` 或 `0x27F`，16 个与 8 个线程的第 60 步输出逐字节相同（与 Windows 分别有 606 与 240 字节不同）。以临时构建的加载器（新线程的控制字由环境变量指定，只用于本次实验，源码已恢复）令所有线程为 `0x37F`，WSL 上以 16 个与 8 个处理器运行完整工程（兼容关、多线程关）：301 个快照中只有 `final_desc.mrq` 不同。整个工程随处理器数变化的原因全部在于主线程与工作线程的控制字不同。

评价：两种处理器数下的输出都是 moresampler 在 Windows 上的真实输出，加载器与之一致。差异源于 moresampler 自身：同一分析中的各帧以不同精度计算，精度取决于帧由哪个线程处理。该行为不是有意的设计（推断，依据为精度的分配只取决于帧落在哪个线程，与算法无关）。两种结果都不是错误的输出，区别只在于部分帧的中间运算多保留了 11 位有效位，不能说其中哪一种是 moresampler 的「正确」结果。第 60 步的差异为末位级别（247 至 401 字节）；完整工程中最大 3039 的样本差是末位差异在后续步骤中被放大的结果，放大发生在哪一步、是否可以听出，尚未检查。

### 7.4 qemu-i386 上的完整工程（实测）

dp1000（8 个逻辑处理器）上以 v11.1.2 加 `third-party/qemu` 补丁的 qemu-i386 运行兼容关、多线程开的完整工程，耗时 6821 秒，快照在 `work/render4/compat-off-mt-on/dp1000-snap`。与 Windows 限定 8 个处理器的结果（`compat-off-mt-on-cpu8`）相比：96 次 resampler 的输出与最终 wav 全部相同；107 个 wavtool 中间文件由 `compare_snapshots.py` 报告不同，这些文件内嵌输出的绝对路径，而两次 Windows 运行位于不同目录，路径的长度不同，比较工具的路径归一化因而失效；去掉内嵌的 UTF-16 路径后，107 个文件全部相同。最终 `desc.mrq` 的首个差异位于第 512 字节（时间戳）。与 16 个处理器的 Windows 结果相比则从第 6 步起不同，与第 7.3 节的机制一致。

### 7.5 x87 超越函数的结论

- 处理器之间（AMD 与 Intel）的 `fsin`、`fcos` 结果有 ±1 ulp 的不同，因此不存在唯一的「处理器结果」。至少在第 60 步，这一级别的差异不改变输出（Intel 与 AMD 的原生输出相同）。
- 现有 qemu 补丁在 sin、cos 的零点附近与处理器差异很大（第 60 步的记录中相对差最大为 3.3e-5，约 2^-15）。改为 66 位约化后，与处理器的差异降到 4 ulp 以内。是否把这一改动加入 `third-party/qemu` 的补丁，由作者决定。

## 8. 作者的决定

- 不提供指定处理器数的选项（2026-09-29）。加载器按主机进程可用的处理器数运行，结果与在同一台机器上运行 Windows 版相同；处理器数不同的 Windows 机器之间，输出本来就不同。验证时以 `start /affinity` 与 `taskset` 使两侧的处理器数相同。TaskSpec 第 5.6 节、第 6 节第 7 条与第 8 节、README 已据此修改。
- 把 66 位约化加入 qemu 补丁：同意，优先级较低。

## 9. 下一步

1. qemu 补丁加入 66 位约化：先以 x87 探针在 AMD 与 Intel 上补测 sin、cos 零点附近的操作数与 `fptan`，再更新补丁与 `third-party/qemu/README.md`，并在 dp1000 上重跑 compare.py。
