# The external packages this repository builds against, each pointed at by its own <name>_DIR.
#
#     -DBoost_DIR=<prefix>/lib/cmake/Boost-1.83.0         (tests only)
#
# qmsetup is found by the root CMakeLists.txt before this file. Every package must be a 32-bit
# build, produced by scripts/build-i386-deps.sh.
#
# Included from the root rather than added as a subdirectory, so that the imported targets are in
# scope for every module.

find_package(Threads REQUIRED)
