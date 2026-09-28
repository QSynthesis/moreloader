# 2026-09-28 导出包装与第一次运行

## 1. 做了什么

- `MoreLoaderWinAPI`（原名 Win32，作者要求改名）：kernel32 的进程与模块、内存、时间、同步、线程、文件与字符串函数，shell32 的 `CommandLineToArgvW`，shlwapi 的 `PathIsDirectoryW`，user32 的 `MessageBoxW`。包装函数位于命名空间 `more::loader::winapi`，以 `MORE_REGISTER` 注册，该宏按 `<dll>_<导出名>` 取包装函数，保证命名与导入名一致。
- `MoreLoaderCRT` 的导出包装：启动与退出、stdio（自建 `_iob` 与流表，文本模式的换行转换，按实测规则解析打开模式）、printf 族、字符串与宽字符串、数学、时间、环境与信号、`_wstat`、`qsort` 与 `rand`、线程、以汇编实现的 `_setjmp3` 与 `longjmp`。数据导出 `_iob`、`_fmode`、`_acmdln`、`__initenv`、`__mb_cur_max` 以 `MORE_REGISTER_DATA` 注册。
- `Process` 增加启动钩子：CRT 在客体代码运行之前初始化标准流与 `_acmdln`。
- 驱动 `moreloader [--trace-imports] [--trace-stubs] [--debug-strings] <exe> [参数…]`。

## 2. 第一次运行

在 WSL 中不带参数运行 `work/moresampler/moresampler.exe`（实测）：

- 187 个导入全部解析，没有未实现的导入被调用。
- 调用序列：MinGW 运行库的初始化（`InitializeCriticalSection`、`AddVectoredExceptionHandler`、安全 cookie 所用的时间与标识）、`_initterm`、`__set_app_type`、`__getmainargs`、winpthreads 的初始化（`CreateSemaphoreA`、`GetProcessAffinityMask` 与一系列 `getenv`）、读取 `moreconfig.txt`（`fgets` 与 `MultiByteToWideChar`）、打印横幅、`time`/`gmtime`/`asctime`、`vprintf`，最后在 `getchar` 处等待按键。
- 标准输入给一个换行后输出横幅与「Moresampler only works as a backend for UTAU. Please run Moresampler from the host software.」，退出码 0。
- MinGW 运行库以 `GetProcAddress` 在 msvcrt 中查找 `___lc_codepage_func` 与 `__lc_codepage`，二者都返回 NULL。运行库在找不到时使用默认值，暂不提供。

## 3. 问题与处理

- glibc 把 `st_atime`、`st_mtime`、`st_ctime` 定义为宏，msvcrt 的 `struct _stat` 的这三个字段因此改名为 `accessTime`、`modificationTime`、`creationTime`。
- `Stream::getChar` 中 0x1A 的常量与成员 `controlZ` 同名，成员遮蔽了常量，改名为 `substituteCharacter`。

## 4. 下一步

1. 准备测试数据：从 Extra-Jinkela 复制数个 wav 与 `oto.ini` 到 `work/voice/`，写 `moreconfig.txt`（`resampler-compatibility on`、`multithread-synthesis off`、44100 Hz 16 位）。
2. 在 Windows 上以相同参数原生运行，得到参考输出。
3. 在 moreloader 上运行频率表生成、渲染与 wavtool 模式，逐字节比较。
