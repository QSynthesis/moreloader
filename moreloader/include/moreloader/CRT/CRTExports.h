#ifndef MORELOADER_CRT_CRTEXPORTS_H
#define MORELOADER_CRT_CRTEXPORTS_H

#include <moreloader/Runtime/ExportRegistry.h>

namespace more::loader {

    /// Adds the emulated functions and variables of msvcrt to \a registry, and registers with
    /// the process the initialization of the variables that depend on the command line.
    void registerCRTExports(ExportRegistry &registry);

}

#endif // MORELOADER_CRT_CRTEXPORTS_H
