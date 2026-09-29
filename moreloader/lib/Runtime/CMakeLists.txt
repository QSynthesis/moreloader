project(MoreLoaderRuntime
    VERSION ${MORELOADER_VERSION}
    LANGUAGES CXX
)

file(GLOB_RECURSE _src CONFIGURE_DEPENDS *.cpp *.h ../../include/moreloader/Runtime/*.h)

moreloader_add_library(${PROJECT_NAME} STATIC
    SOURCES ${_src}
    LINKS MoreLoaderSupport MoreLoaderImage Threads::Threads
    INCLUDE_PRIVATE ../../include/moreloader/Runtime/** **
)
