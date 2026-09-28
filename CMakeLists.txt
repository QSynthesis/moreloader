cmake_minimum_required(VERSION 3.19)

project(moreloader
    VERSION 0.0.1.0
    LANGUAGES C CXX
    DESCRIPTION "Minimal PE loader that runs the 32-bit moresampler on Linux"
    HOMEPAGE_URL "https://github.com/QSynthesis/moreloader"
)

# ----------------------------------
# Build Options
# ----------------------------------
option(MORE_BUILD_TESTS "Build test cases" OFF)
option(MORE_INSTALL "Install the project" ON)
option(MORE_STATIC "Link the loader statically, so that no 32-bit libraries are required at run time" ON)

# ----------------------------------
# Platform Check
# ----------------------------------
# The guest is 32-bit x86 code executed directly by the processor, and the loader shares its
# address space. Any other target is a configuration error rather than a missing feature.
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux" OR NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR
        "moreloader must be built as a 32-bit x86 Linux program. "
        "Configure with -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/linux-i386.cmake.")
endif()

# ----------------------------------
# CMake Settings
# ----------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# The image of moresampler occupies 0x400000 to 0x4ed000 and has no relocations. A position
# independent executable may be mapped over that range, whereas the default base of a non-PIE
# i386 executable, 0x8048000, lies above it.
set(CMAKE_POSITION_INDEPENDENT_CODE OFF)
add_compile_options(-fno-pie)
add_link_options(-no-pie)

if(MORE_STATIC)
    add_link_options(-static)
endif()

if(MORE_INSTALL)
    include(GNUInstallDirs)
    include(CMakePackageConfigHelpers)
endif()

# ----------------------------------
# Project Variables
# ----------------------------------
set(MORE_VERSION ${PROJECT_VERSION})
set(MORE_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

# ----------------------------------
# Find basic dependencies
# ----------------------------------
find_package(qmsetup REQUIRED)

qm_import(Filesystem)

if(NOT DEFINED QMSETUP_BUILD_DIR)
    set(QMSETUP_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/out")
endif()

qm_init_directories()

# ----------------------------------
# Find third-party dependencies
# ----------------------------------
# Included rather than added as a subdirectory, so that its imported targets are visible to every
# module.
include(${CMAKE_CURRENT_SOURCE_DIR}/third-party/Dependencies.cmake)

# ----------------------------------
# Tests
# ----------------------------------
if(MORE_BUILD_TESTS)
    enable_testing()
endif()

# ----------------------------------
# Add source modules
# ----------------------------------
add_subdirectory(moreloader)
