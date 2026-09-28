#include "CRTExports.h"
#include "CRTExports_p.h"

#include <moreloader/Runtime/Process.h>

#include "StringFunctions.h"

namespace more::loader::msvcrt {

    namespace {

        thread_local int t_errno = 0;

        void startupHook() {
            initializeStandardStreams();
            initializeStartupData();
        }

    }

    int &threadErrno() {
        return t_errno;
    }

    void setErrnoFromHost(int hostError) {
        t_errno = errorNumber(hostError);
    }

}

namespace more::loader {

    void registerCRTExports(ExportRegistry &registry) {
        using namespace msvcrt;
        registerStartupExports(registry);
        registerStdioExports(registry);
        registerFormatExports(registry);
        registerStringExports(registry);
        registerWideStringExports(registry);
        registerMathExports(registry);
        registerTimeExports(registry);
        registerEnvironmentExports(registry);
        registerThreadExports(registry);
        registerSetJmpExports(registry);
        process().addStartupHook(startupHook);
    }

}
