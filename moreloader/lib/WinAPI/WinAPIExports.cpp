#include "WinAPIExports.h"
#include "WinAPI_p.h"

#include <cerrno>

namespace more::loader::winapi {

    DWORD systemErrorOf(int error) {
        switch (error) {
            case ENOENT:
                return ErrorFileNotFound;
            case ENOTDIR:
                return ErrorPathNotFound;
            case EACCES:
            case EPERM:
            case EROFS:
                return ErrorAccessDenied;
            case EBADF:
                return ErrorInvalidHandle;
            case ENOMEM:
                return ErrorNotEnoughMemory;
            case EAGAIN:
            case EDEADLK:
                return ErrorLockViolation;
            default:
                return ErrorInvalidParameter;
        }
    }

}

namespace more::loader {

    void registerWinAPIExports(ExportRegistry &registry) {
        using namespace winapi;
        registerKernel32Process(registry);
        registerKernel32Memory(registry);
        registerKernel32Time(registry);
        registerKernel32Sync(registry);
        registerKernel32Thread(registry);
        registerKernel32File(registry);
        registerKernel32String(registry);
        registerShell32(registry);
        registerShlwapi(registry);
        registerUser32(registry);
    }

}
