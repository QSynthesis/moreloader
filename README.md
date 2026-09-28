# moreloader

A minimal PE loader and Windows API layer that runs the unmodified 32-bit `moresampler.exe` 0.8.4 as a command-line program on Linux, without Wine. The output is intended to be byte-identical to that of a native run on Windows.

## Status

Early development. See [docs/Status.md](docs/Status.md).

## moresampler

moresampler is not part of this project and is not distributed with it. Its license permits redistribution of the original package, but not distribution as part of other software without the permission of its author. Obtain moresampler 0.8.4 separately and pass the path of `moresampler.exe` to the loader. The executable file is neither modified nor patched.

## Building

The loader is a 32-bit x86 Linux program, linked statically. Requirements:

- CMake 3.19 or later, Ninja, and GCC with 32-bit support (`gcc-multilib`). Without administrator rights, `scripts/fetch-i386-overlay.sh` extracts the required 32-bit libraries into a directory that is passed as `MORE_I386_OVERLAY`.
- 32-bit builds of [qmsetup](https://github.com/stdware/qmsetup) and, for the tests, Boost.Test, built by `scripts/build-i386-deps.sh`.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/linux-i386.cmake \
    -Dqmsetup_DIR=<prefix>/lib/cmake/qmsetup
cmake --build build
```

## Usage

```sh
moreloader <path to moresampler.exe> <arguments of moresampler...>
```

Absolute host paths among the arguments are presented to moresampler as paths on drive `Z:`, in the manner of Wine.

## Repository layout

| Directory | Content |
|---|---|
| `moreloader` | The libraries of the loader, the driver program and the tests |
| `cmake`, `scripts` | The 32-bit toolchain file and the scripts that prepare the build environment and the analysis |
| `docs` | Design documents and work logs, in Chinese |

## License

MIT License. See [LICENSE](LICENSE).
