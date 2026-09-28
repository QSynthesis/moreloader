# The external packages this repository builds against, each pointed at by its own <name>_DIR.
#
#     -Dstdcorelib_DIR=<prefix>/lib/cmake/stdcorelib
#     -DBoost_DIR=<prefix>/lib/cmake/Boost-1.83.0         (tests only)
#
# qmsetup is found by the root CMakeLists.txt before this file. Every package must be a 32-bit
# build, produced by scripts/build-i386-deps.sh. stdcorelib is developed alongside this repository
# and is not taken from a package manager.
#
# Included from the root rather than added as a subdirectory, so that the imported targets are in
# scope for every module.

if(NOT stdcorelib_DIR)
    message(FATAL_ERROR
        "stdcorelib_DIR is not set. Build a 32-bit stdcorelib with scripts/build-i386-deps.sh "
        "and pass -Dstdcorelib_DIR=<prefix>/lib/cmake/stdcorelib.")
endif()

find_package(stdcorelib CONFIG REQUIRED)

find_package(Threads REQUIRED)
