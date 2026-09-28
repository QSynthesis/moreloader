#ifndef MORELOADER_WINAPI_WINAPI_P_H
#define MORELOADER_WINAPI_WINAPI_P_H

#include <cstdint>

#include <moreloader/Runtime/ExportRegistry.h>
#include <moreloader/Support/MoreLoaderSupportGlobal.h>

namespace more::loader::winapi {

    using BOOL = std::int32_t;
    using DWORD = std::uint32_t;
    using HANDLE = std::uint32_t;

    constexpr BOOL TRUE = 1;
    constexpr BOOL FALSE = 0;
    constexpr HANDLE INVALID_HANDLE_VALUE = 0xFFFFFFFF;

    /// System error codes that the wrappers report through \c SetLastError.
    enum SystemError : DWORD {
        ErrorSuccess = 0,
        ErrorFileNotFound = 2,
        ErrorPathNotFound = 3,
        ErrorAccessDenied = 5,
        ErrorInvalidHandle = 6,
        ErrorNotEnoughMemory = 8,
        ErrorNoMoreFiles = 18,
        ErrorLockViolation = 33,
        ErrorNotSupported = 50,
        ErrorInvalidParameter = 87,
        ErrorInsufficientBuffer = 122,
        ErrorModuleNotFound = 126,
        ErrorProcedureNotFound = 127,
        ErrorNotLocked = 158,
        ErrorNoMoreItems = 259,
        ErrorTooManyPosts = 298,
        ErrorNoUnicodeTranslation = 1113,
    };

    /// Returns the system error code that corresponds to the host \c errno value \a error.
    DWORD systemErrorOf(int error);

    // Registration of the wrappers of each source file.
    void registerKernel32Process(ExportRegistry &registry);
    void registerKernel32Memory(ExportRegistry &registry);
    void registerKernel32Time(ExportRegistry &registry);
    void registerKernel32Sync(ExportRegistry &registry);
    void registerKernel32Thread(ExportRegistry &registry);
    void registerKernel32File(ExportRegistry &registry);
    void registerKernel32String(ExportRegistry &registry);
    void registerShell32(ExportRegistry &registry);
    void registerShlwapi(ExportRegistry &registry);
    void registerUser32(ExportRegistry &registry);

}

#endif // MORELOADER_WINAPI_WINAPI_P_H
