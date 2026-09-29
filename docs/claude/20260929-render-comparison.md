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

## 7. 下一步

1. FEX（spark）与打补丁的 qemu-i386（dp1000）上重跑同一工程。
