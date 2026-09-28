#!/bin/bash
# Extracts the 32-bit C library and libgcc of Debian or Ubuntu into a directory, for systems on
# which gcc-multilib cannot be installed. No administrator rights are required: the package lists
# are downloaded into a private state directory, and the packages are unpacked with dpkg-deb.
#
# Usage: scripts/fetch-i386-overlay.sh [<directory>]
# The default directory is ~/.local/share/moreloader-i386. Pass the directory to CMake as
# MORE_I386_OVERLAY together with cmake/toolchains/linux-i386.cmake.
#
# The loader is linked statically, therefore the overlay is required only for building.

set -euo pipefail

overlay="${1:-$HOME/.local/share/moreloader-i386}"
state="$HOME/.cache/moreloader-apt"
major="$(gcc -dumpversion)"

mkdir -p "$state/lists/partial" "$state/archives/partial" "$state/debs"
opts=(-o "Dir::State=$state" -o "Dir::State::Lists=$state/lists" -o "Dir::Cache=$state"
      -o "Dir::Cache::Archives=$state/archives" -o "Dir::State::status=/var/lib/dpkg/status"
      -o "Debug::NoLocking=1")

apt-get "${opts[@]}" update >/dev/null

cd "$state/debs"
rm -f ./*.deb
apt-get "${opts[@]}" download libc6-dev-i386 libc6-i386 "lib32gcc-$major-dev" lib32gcc-s1 \
    "lib32stdc++-$major-dev" lib32stdc++6

rm -rf "$overlay"
mkdir -p "$overlay"
for deb in ./*.deb; do
    dpkg-deb -x "$deb" "$overlay"
done

# The linker scripts libc.so and libm.so name their members by absolute paths such as
# /lib32/libc.so.6, which exist only if the packages are installed. The paths are redirected into
# the overlay. A dynamically linked program built this way still requires /lib/ld-linux.so.2 at
# run time, therefore programs that are run on the build machine are linked statically.
for script in "$overlay"/usr/lib32/*.so; do
    if grep -q "GNU ld script" "$script" 2>/dev/null; then
        sed -i -e "s# /usr/lib32/# $overlay/usr/lib32/#g" -e "s# /lib32/# $overlay/usr/lib32/#g" \
            -e "s# /lib/ld-linux.so.2# $overlay/usr/lib32/ld-linux.so.2#g" "$script"
    fi
done

# gcc-multilib provides the directory asm for -m32, pointing to the biarch kernel headers.
mkdir -p "$overlay/usr/include-multilib"
ln -sfn /usr/include/x86_64-linux-gnu/asm "$overlay/usr/include-multilib/asm"

echo "$overlay"
