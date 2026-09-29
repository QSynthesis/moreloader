# moreloader

不依赖 Wine 的最小 PE 加载器与 Windows API 包装层，使未经修改的 32 位 `moresampler.exe` 0.8.4 与 UTAU 自带的 `resampler.exe` 在 Linux 上作为命令行程序运行。目标是输出与 Windows 原生运行逐字节相同。

## 现状

| 平台 | 运行方式 | 与 Windows 的比较 |
|---|---|---|
| Linux x86_64 | 直接运行 | moresampler 与 resampler.exe 逐字节一致，包括 UTAU 工程的完整渲染 |
| Linux ARM64 | FEX-Emu | 逐字节一致 |
| Linux RISC-V | qemu-i386 加 [`third-party/qemu`](third-party/qemu/README.md) 的补丁 | 逐字节一致 |

以上比较的前提是两侧进程可用的处理器数相同。moresampler 的输出随处理器数而变，在 Windows 上同样如此：同一台 Windows 机器限定 8 个处理器与使用 16 个处理器时，渲染结果不同。加载器按主机的处理器数运行，结果与在同一台机器上运行 Windows 版相同。原因见 [docs/claude/20260929-render-comparison.md](docs/claude/20260929-render-comparison.md) 第 7.3 节。

详见 [docs/Status.md](docs/Status.md)。

## 支持的程序

| 程序 | 说明 |
|---|---|
| moresampler 0.8.4（`moresampler.exe`） | resampler 模式、wavtool 模式与频率表生成 |
| UTAU 自带的 resampler（`resampler.exe`，与 UTAU 0.4.18 一同发布） | resampler 模式 |

两个程序都不属于本项目，也不随本项目分发。moresampler 的许可证允许原样再分发，但未经作者许可不得作为其他软件的一部分分发；`resampler.exe` 随 UTAU 发布。请另行取得程序，把可执行文件的路径传给加载器。加载器不修改可执行文件，也不在内存中打补丁。

## 构建

加载器是静态链接的 32 位 x86 Linux 程序。需要：

- CMake 3.19 或更高版本、Ninja，以及支持 32 位的 GCC（`gcc-multilib`）。没有管理员权限时，`scripts/fetch-i386-overlay.sh` 把所需的 32 位库解包到一个目录，配置时以 `MORE_I386_OVERLAY` 传入。
- 32 位的 [qmsetup](https://github.com/stdware/qmsetup)，以及测试所需的 Boost.Test，由 `scripts/build-i386-deps.sh` 构建。

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/linux-i386.cmake \
    -Dqmsetup_DIR=<prefix>/lib/cmake/qmsetup
cmake --build build
```

## 用法

```sh
moreloader [选项] <moresampler.exe 或 resampler.exe 的路径> <该程序的参数...>
```

例如以 UTAU 的 13 个参数渲染一个音符：

```sh
moreloader /opt/utau/resampler.exe /voice/a.wav /tmp/out.wav C4 100 "" 0 500 0 0 100 0 '!120' 'AA#5#'
moreloader /opt/moresampler/moresampler.exe /voice/a.wav /tmp/out.wav C4 100 "" 0 500 0 0 100 0 '!120' 'AA#5#'
```

参数中以 `/` 开头、且在主机上存在或位于已有目录中的，以 Wine 的方式作为 `Z:` 盘上的路径交给程序；其余参数原样传递，因此以 `/` 开头的音高曲线不受影响。

| 选项 | 作用 |
|---|---|
| `--trace-imports` | 报告每一次导入函数的调用 |
| `--trace-stubs` | 报告只部分实现的调用 |
| `--debug-strings` | 报告传给 `OutputDebugStringA` 的字符串 |
| `--check-heap` | 报告 msvcrt 堆中越过块末尾的写入 |

## 目录

| 目录 | 内容 |
|---|---|
| `moreloader` | 加载器的库、驱动程序与测试 |
| `cmake`、`scripts` | 32 位工具链文件，以及准备构建环境与分析的脚本 |
| `third-party` | 外部依赖的配置与 QEMU 的补丁 |
| `docs` | 设计文档与工作日志 |

## 许可证

MIT 许可证，见 [LICENSE](LICENSE)。`third-party/qemu` 中的补丁按 QEMU 的许可证（GPL-2.0）分发。
