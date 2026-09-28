# 2026-09-29 与 Windows 原生运行的第一次比较

## 1. 方法

`moreloader/tests/manual/compare/compare.py`（在 Windows 上运行）：从 Extra-Jinkela 复制 `ae.wav`、`baf.wav`、`bam.wav` 与对应的 `oto.ini` 行，Windows 一侧在 `work/compare/windows/`，Linux 一侧在 WSL 本地文件系统的 `~/moreloader-compare/linux/`。两侧使用同一份测试用 `moreconfig.txt`（`resampler-compatibility on`、`multithread-synthesis off`、44100 Hz 16 位），同一步的两侧并行运行，进度写入 `work/compare/progress.log`。

用例：

1. 频率表生成，frqeditor 的命令 `<wav> nul 100 100 GN 0 50`，三个 wav。
2. resampler 模式，`<wav> <out> C4 100 "" 0 500 0 0 100 0 !120 AA#5#`，两个 wav。
3. wavtool 模式，按 UTAU 的参数追加两个音符。

## 2. 结果（实测）

全部 13 个文件逐字节一致：3 个 `.llsm`、`desc.mrq`（排除每个条目的时间戳）、2 个渲染出的 wav（0.52 秒，峰值 21178）与其 `.llsm.tmp`、wavtool 写出的索引。

wavtool 模式下 `mix.wav` 是 moresampler 自己的数据索引，以 UTF-16 记录各片段的绝对路径（`E:\...` 与 `Z:\home\...`），比较时把两侧的根路径替换为同一个占位符。

## 3. 问题与处理

- **`LockFileEx` 在 Linux 一侧反复失败，客体无限重试。** moresampler 在 `0x40d5b5` 以 `"rw"` 打开失败（与 Windows 相同）后，在 `0x40d69b` 以 `"w"` 打开并加锁（静态分析与导入跟踪）。Windows 对只写句柄同样授予共享锁，而 `fcntl` 的读锁要求描述符可读。`FileObject` 改为在第一次加锁时经 `/proc/self/fd/N` 另开一个读写描述符专门用于加锁。
- **`/mnt/e` 上的文件访问很慢。** 工作目录放在 `/mnt/e` 时一轮比较需要两分钟以上，放到 WSL 本地文件系统后整轮 1.3 秒，单次运行 0.1 至 0.3 秒。

## 4. 下一步

以真实工程检验完整的渲染流程：由 helloutau 为同一个工程生成 Windows 的批处理与 Linux 的 shell 脚本，moreloader 一侧运行二者并逐步比较中间产物与最终结果的采样。任务书交给 helloutau 准备脚本生成。
