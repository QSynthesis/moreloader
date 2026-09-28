# Toolchain for building the loader as a 32-bit x86 Linux program with the native GCC.
#
# The loader must be an i386 process, because the guest is 32-bit code executed directly by the
# processor. Distributions provide the 32-bit C library and libgcc through gcc-multilib. If that
# package cannot be installed, scripts/fetch-i386-overlay.sh extracts the same files into a
# directory without administrator rights, and MORE_I386_OVERLAY names that directory.

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR i686)

set(CMAKE_C_COMPILER gcc)
set(CMAKE_CXX_COMPILER g++)

# The overlay path must reach the nested projects of try_compile(), which read the toolchain file
# again with an empty cache.
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES MORE_I386_OVERLAY)
if(NOT MORE_I386_OVERLAY AND DEFINED ENV{MORE_I386_OVERLAY})
    set(MORE_I386_OVERLAY "$ENV{MORE_I386_OVERLAY}")
endif()

set(_flags "-m32")

if(MORE_I386_OVERLAY)
    # The multiarch directory of a compiler without multilib support is i386-linux-gnu for -m32,
    # while Debian and Ubuntu install the biarch headers under x86_64-linux-gnu. Both header
    # directories are therefore appended after the standard directories, so that the
    # #include_next chains of libstdc++ remain intact.
    execute_process(COMMAND ${CMAKE_CXX_COMPILER} -dumpversion
        OUTPUT_VARIABLE _gcc_major OUTPUT_STRIP_TRAILING_WHITESPACE)
    set(_gcc32 "${MORE_I386_OVERLAY}/usr/lib/gcc/x86_64-linux-gnu/${_gcc_major}/32")
    string(APPEND _flags
        " -isystem ${MORE_I386_OVERLAY}/usr/include/x86_64-linux-gnu/c++/${_gcc_major}/32"
        " -idirafter /usr/include/x86_64-linux-gnu"
        " -idirafter ${MORE_I386_OVERLAY}/usr/include/x86_64-linux-gnu"
        " -idirafter ${MORE_I386_OVERLAY}/usr/include-multilib"
        " -B${MORE_I386_OVERLAY}/usr/lib32 -B${_gcc32}"
        " -L${MORE_I386_OVERLAY}/usr/lib32 -L${_gcc32}"
    )
endif()

set(CMAKE_C_FLAGS_INIT "${_flags}")
set(CMAKE_CXX_FLAGS_INIT "${_flags}")
set(CMAKE_ASM_FLAGS_INIT "${_flags}")
