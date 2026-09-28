project(MoreLoaderImage
    VERSION ${MORELOADER_VERSION}
    LANGUAGES CXX
)

file(GLOB_RECURSE _src CONFIGURE_DEPENDS *.cpp *.h ../../include/moreloader/Image/*.h)

moreloader_add_library(${PROJECT_NAME} STATIC
    SOURCES ${_src}
    LINKS MoreLoaderSupport
    LINKS_PRIVATE stdcorelib::stdcorelib
    INCLUDE_PRIVATE ../../include/moreloader/Image/** **
)
