project(moreloaderkit
    VERSION ${MORE_VERSION}
    LANGUAGES C CXX
    DESCRIPTION "moreloader"
)

# ----------------------------------
# Project Variables
# ----------------------------------
set(MORELOADER_INSTALL ${MORE_INSTALL})
set(MORELOADER_VERSION ${PROJECT_VERSION})
set(MORELOADER_INSTALL_NAME moreloader)

include("conf.cmake")

moreloader_init_buildsystem(MORELOADER)

add_subdirectory(lib)

add_subdirectory(tools)

moreloader_finish_buildsystem()
