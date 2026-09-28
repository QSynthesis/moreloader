project(test_auto LANGUAGES CXX)

find_package(Boost CONFIG COMPONENTS unit_test_framework)

# Fatal rather than a warning and a return. A build with MORE_BUILD_TESTS=ON that silently omits
# the tests reports success without running a single case.
if(NOT Boost_FOUND)
    message(FATAL_ERROR
        "MORE_BUILD_TESTS is on but a 32-bit Boost.Test was not found. Build it with "
        "scripts/build-i386-deps.sh and pass -DBoost_DIR, or turn the tests off with "
        "-DMORE_BUILD_TESTS=OFF.")
endif()

# One directory per sub-library, mirroring include/moreloader. Each test file is named after the
# header it covers, so that the directory listing shows which headers are tested.
file(GLOB_RECURSE _src CONFIGURE_DEPENDS *.cpp)

moreloader_add_executable(${PROJECT_NAME} TEST CONSOLE
    SOURCES ${_src}
    LINKS Boost::unit_test_framework MoreLoaderSupport MoreLoaderCRT
)

# Golden data measured on Windows by moreloader/tests/probe, read from the source tree.
target_compile_definitions(${PROJECT_NAME} PRIVATE
    MORE_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data"
)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# One entry rather than one per suite, as in stdcorelib. ctest exits 0 if it finds no tests, so
# run it with --no-tests=error.
add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})
set_tests_properties(${PROJECT_NAME} PROPERTIES TIMEOUT 120)
