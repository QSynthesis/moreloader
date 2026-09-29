#!/bin/bash
# Builds the 32-bit dependencies of moreloader and installs them into one prefix:
#   - qmsetup, from a local clone, with its tool qmcorecmd linked statically so that it runs at
#     build time without the 32-bit dynamic loader.
#   - Boost.Test 1.83 as a static library, for the automatic tests.
#
# Usage: scripts/build-i386-deps.sh <qmsetup source directory> [<prefix>]
# The default prefix is ~/.local/opt/moreloader-i386. The overlay of fetch-i386-overlay.sh is used
# if MORE_I386_OVERLAY is set.
#
# Afterwards pass -Dqmsetup_DIR=<prefix>/lib/cmake/qmsetup and
# -DBoost_DIR=<prefix>/lib/cmake/Boost-1.83.0 to CMake.

set -euo pipefail

qmsetup_source="$(realpath "$1")"
prefix="${2:-$HOME/.local/opt/moreloader-i386}"
work="$HOME/.cache/moreloader-src"
toolchain="$(realpath "$(dirname "$0")/../cmake/toolchains/linux-i386.cmake")"
overlay="${MORE_I386_OVERLAY:-}"
boost_version=1.83.0
boost_dir="boost_${boost_version//./_}"

mkdir -p "$work"

# ----------------------------------
# qmsetup
# ----------------------------------
rm -rf "$work/qmsetup-build"
cmake -S "$qmsetup_source" -B "$work/qmsetup-build" -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE="$toolchain" -DMORE_I386_OVERLAY="$overlay" \
    -DCMAKE_EXE_LINKER_FLAGS=-static -DCMAKE_INSTALL_PREFIX="$prefix"
cmake --build "$work/qmsetup-build"
cmake --install "$work/qmsetup-build"

# ----------------------------------
# Boost.Test
# ----------------------------------
cd "$work"
if [ ! -d "$boost_dir" ]; then
    curl -sSL -o "$boost_dir.tar.gz" \
        "https://archives.boost.io/release/$boost_version/source/$boost_dir.tar.gz"
    tar xzf "$boost_dir.tar.gz"
fi
cd "$boost_dir"
[ -x b2 ] || ./bootstrap.sh --with-libraries=test

# b2 compiles with -m32 for address-model=32. The overlay flags mirror those of the toolchain file.
flags="-fno-pie"
linkflags=""
if [ -n "$overlay" ]; then
    major="$(gcc -dumpversion)"
    flags="$flags -isystem $overlay/usr/include/x86_64-linux-gnu/c++/$major/32"
    flags="$flags -idirafter /usr/include/x86_64-linux-gnu"
    flags="$flags -idirafter $overlay/usr/include/x86_64-linux-gnu"
    flags="$flags -idirafter $overlay/usr/include-multilib"
    linkflags="-B$overlay/usr/lib32 -L$overlay/usr/lib32"
fi

# runtime-link keeps its default. The CMake package of Boost rejects a static-runtime variant
# unless every consumer sets Boost_USE_STATIC_RUNTIME, and on Linux the setting does not change
# the contents of a static library.
./b2 -j"$(nproc)" address-model=32 architecture=x86 link=static \
    variant=release threading=multi cxxflags="$flags" cflags="$flags" linkflags="$linkflags" \
    --with-test --prefix="$prefix" install

echo "$prefix"
