#ifndef MORELOADER_WINAPI_WINAPIEXPORTS_H
#define MORELOADER_WINAPI_WINAPIEXPORTS_H

#include <moreloader/Runtime/ExportRegistry.h>

namespace more::loader {

    /// Adds the emulated functions of kernel32, shell32, shlwapi and user32 to \a registry.
    void registerWinAPIExports(ExportRegistry &registry);

}

#endif // MORELOADER_WINAPI_WINAPIEXPORTS_H
