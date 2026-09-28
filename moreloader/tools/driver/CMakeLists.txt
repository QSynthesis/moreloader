project(moreloader
    DESCRIPTION "Runs the 32-bit moresampler on Linux"
    LANGUAGES CXX
)

file(GLOB_RECURSE _src CONFIGURE_DEPENDS "*.cpp")

moreloader_add_executable(${PROJECT_NAME} CONSOLE
    SOURCES ${_src}
    LINKS MoreLoaderSupport Threads::Threads
)
