#include "WinAPI_p.h"

#include <sched.h>
#include <unistd.h>

#include <atomic>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

#include <moreloader/Runtime/GuestLayout.h>
#include <moreloader/Runtime/GuestThread.h>
#include <moreloader/Runtime/KernelObjects.h>
#include <moreloader/Runtime/Process.h>
#include <moreloader/Support/CodePage.h>
#include <moreloader/Support/Diagnostics.h>

namespace more::loader::winapi {

    namespace {

        // Module handles of the emulated libraries. They are addresses that no mapping occupies,
        // because the guest only passes them back to GetProcAddress.
        constexpr std::uint32_t libraryHandleBase = 0x7E000000;
        constexpr std::uint32_t libraryHandleStride = 0x10000;

        // The exception that the Visual C++ debugger interface raises to name a thread.
        constexpr DWORD threadNameException = 0x406D1388;

        constexpr std::int32_t exceptionContinueSearch = 0;

        std::atomic<std::uint32_t> s_unhandledExceptionFilter{0};

        std::mutex s_vectoredMutex;
        std::vector<std::uint32_t *> s_vectoredHandlers;

        std::string lowercase(std::string s) {
            for (char &c : s) {
                if (c >= 'A' && c <= 'Z') {
                    c = char(c - 'A' + 'a');
                }
            }
            return s;
        }

        std::string fileNameOf(const std::string &path) {
            size_t slash = path.find_last_of("\\/");
            return slash == std::string::npos ? path : path.substr(slash + 1);
        }

        std::string withoutExtension(const std::string &name) {
            size_t dot = name.find_last_of('.');
            return dot == std::string::npos ? name : name.substr(0, dot);
        }

        // Returns the module handle of \a name, or 0 if the module is not loaded.
        std::uint32_t moduleHandleOf(const std::string &name) {
            Process &p = process();
            std::string requested = lowercase(fileNameOf(name));
            std::string image = lowercase(fileNameOf(wideToMultiByte(p.modulePath()).text));
            if (requested == image || requested == withoutExtension(image)) {
                return p.image().base();
            }
            auto libraries = p.registry().libraries();
            std::string normalized = ExportRegistry::normalizeLibrary(requested);
            for (std::size_t i = 0; i < libraries.size(); ++i) {
                if (libraries[i] == normalized) {
                    return libraryHandleBase + std::uint32_t(i) * libraryHandleStride;
                }
            }
            return 0;
        }

        std::uint32_t moduleHandleForName(const std::string *name) {
            if (!name) {
                return process().image().base();
            }
            std::uint32_t handle = moduleHandleOf(*name);
            if (!handle) {
                setLastError(ErrorModuleNotFound);
            }
            return handle;
        }

        // -----------------------------------------------------------------------------------

        char16_t *MORE_WINAPI kernel32_GetCommandLineW() {
            return const_cast<char16_t *>(process().commandLine().c_str());
        }

        /// Copies the guest path of the executable. A buffer that is too small receives the
        /// truncated path with a terminator, and the function returns \a size and sets
        /// \c ERROR_INSUFFICIENT_BUFFER, as Windows Vista and later do.
        DWORD MORE_WINAPI kernel32_GetModuleFileNameW(HANDLE module, char16_t *buffer, DWORD size) {
            Process &p = process();
            if (module != 0 && module != p.image().base()) {
                setLastError(ErrorModuleNotFound);
                return 0;
            }
            const std::u16string &path = p.modulePath();
            if (size == 0) {
                setLastError(ErrorInsufficientBuffer);
                return 0;
            }
            if (path.size() < size) {
                std::memcpy(buffer, path.c_str(), (path.size() + 1) * sizeof(char16_t));
                setLastError(ErrorSuccess);
                return DWORD(path.size());
            }
            std::memcpy(buffer, path.c_str(), (size - 1) * sizeof(char16_t));
            buffer[size - 1] = 0;
            setLastError(ErrorInsufficientBuffer);
            return size;
        }

        HANDLE MORE_WINAPI kernel32_GetModuleHandleA(const char *name) {
            if (!name) {
                return moduleHandleForName(nullptr);
            }
            std::string narrow(name);
            return moduleHandleForName(&narrow);
        }

        HANDLE MORE_WINAPI kernel32_GetModuleHandleW(const char16_t *name) {
            if (!name) {
                return moduleHandleForName(nullptr);
            }
            std::string narrow = wideToMultiByte(name).text;
            return moduleHandleForName(&narrow);
        }

        /// Returns an export of an emulated library. The image itself exports nothing, and
        /// lookups by ordinal are not supported.
        void *MORE_WINAPI kernel32_GetProcAddress(HANDLE module, const char *name) {
            Process &p = process();
            if (reinterpret_cast<std::uintptr_t>(name) < 0x10000) {
                setLastError(ErrorProcedureNotFound);
                return nullptr;
            }
            if (module >= libraryHandleBase) {
                std::uint32_t index = (module - libraryHandleBase) / libraryHandleStride;
                auto libraries = p.registry().libraries();
                if (index < libraries.size()) {
                    if (auto found = p.registry().find(libraries[index], name)) {
                        return found->address;
                    }
                }
            }
            if (isDiagnosticEnabled(DiagnosticCategory::Stubs)) {
                diagnostic("GetProcAddress(0x%08x, \"%s\") returns NULL", module, name);
            }
            setLastError(ErrorProcedureNotFound);
            return nullptr;
        }

        void MORE_WINAPI kernel32_GetStartupInfoA(STARTUPINFOA32 *info) {
            std::memset(info, 0, sizeof *info);
            info->cb = sizeof *info;
        }

        HANDLE MORE_WINAPI kernel32_GetCurrentProcess() {
            return currentProcessHandle;
        }

        DWORD MORE_WINAPI kernel32_GetCurrentProcessId() {
            return DWORD(::getpid());
        }

        /// Ends the process immediately. Streams of the C runtime are not flushed, as on
        /// Windows.
        BOOL MORE_WINAPI kernel32_TerminateProcess(HANDLE handle, DWORD exitCode) {
            if (handle != currentProcessHandle) {
                setLastError(ErrorInvalidHandle);
                return FALSE;
            }
            process().terminate(exitCode);
        }

        BOOL MORE_WINAPI kernel32_IsDebuggerPresent() {
            return FALSE;
        }

        void MORE_WINAPI kernel32_OutputDebugStringA(const char *text) {
            if (isDiagnosticEnabled(DiagnosticCategory::DebugStrings) && text) {
                diagnostic("OutputDebugString: %s", text);
            }
        }

        /// Records the filter. Structured exception handling is not emulated, therefore the
        /// filter is never called.
        std::uint32_t MORE_WINAPI kernel32_SetUnhandledExceptionFilter(std::uint32_t filter) {
            return s_unhandledExceptionFilter.exchange(filter);
        }

        std::int32_t MORE_WINAPI kernel32_UnhandledExceptionFilter(void *pointers) {
            return exceptionContinueSearch;
        }

        /// Records the handler and returns a token for its removal. Handlers are never called.
        void *MORE_WINAPI kernel32_AddVectoredExceptionHandler(DWORD first, std::uint32_t handler) {
            std::lock_guard<std::mutex> lock(s_vectoredMutex);
            auto token = new std::uint32_t(handler);
            s_vectoredHandlers.push_back(token);
            return token;
        }

        DWORD MORE_WINAPI kernel32_RemoveVectoredExceptionHandler(void *token) {
            std::lock_guard<std::mutex> lock(s_vectoredMutex);
            for (auto it = s_vectoredHandlers.begin(); it != s_vectoredHandlers.end(); ++it) {
                if (*it == token) {
                    delete *it;
                    s_vectoredHandlers.erase(it);
                    return 1;
                }
            }
            return 0;
        }

        /// Ignores the exception that names a thread for a debugger, which winpthreads raises.
        /// Any other exception is unhandled, because structured exception handling is not
        /// emulated, and ends the process.
        void MORE_WINAPI kernel32_RaiseException(DWORD code, DWORD flags, DWORD count,
                                                 const std::uint32_t *arguments) {
            if (code == threadNameException) {
                return;
            }
            fatal("unhandled exception 0x%08x raised by the guest", code);
        }

        /// Returns a mask of one bit per processor available to the host process, at most 32.
        /// moresampler derives the number of synthesis threads from it.
        BOOL MORE_WINAPI kernel32_GetProcessAffinityMask(HANDLE process, std::uint32_t *processMask,
                                                         std::uint32_t *systemMask) {
            cpu_set_t set;
            int count = 1;
            if (::sched_getaffinity(0, sizeof set, &set) == 0) {
                count = CPU_COUNT(&set);
            }
            if (count > 32) {
                count = 32;
            }
            std::uint32_t mask = count == 32 ? 0xFFFFFFFFu : ((1u << count) - 1);
            *processMask = mask;
            *systemMask = mask;
            return TRUE;
        }

        BOOL MORE_WINAPI kernel32_SetProcessAffinityMask(HANDLE process, std::uint32_t mask) {
            return TRUE;
        }

        DWORD MORE_WINAPI kernel32_GetLastError() {
            return lastError();
        }

        void MORE_WINAPI kernel32_SetLastError(DWORD error) {
            setLastError(error);
        }

    }

    void registerKernel32Process(ExportRegistry &registry) {
        MORE_REGISTER(registry, kernel32, GetCommandLineW);
        MORE_REGISTER(registry, kernel32, GetModuleFileNameW);
        MORE_REGISTER(registry, kernel32, GetModuleHandleA);
        MORE_REGISTER(registry, kernel32, GetModuleHandleW);
        MORE_REGISTER(registry, kernel32, GetProcAddress);
        MORE_REGISTER(registry, kernel32, GetStartupInfoA);
        MORE_REGISTER(registry, kernel32, GetCurrentProcess);
        MORE_REGISTER(registry, kernel32, GetCurrentProcessId);
        MORE_REGISTER(registry, kernel32, TerminateProcess);
        MORE_REGISTER(registry, kernel32, IsDebuggerPresent);
        MORE_REGISTER(registry, kernel32, OutputDebugStringA);
        MORE_REGISTER(registry, kernel32, SetUnhandledExceptionFilter);
        MORE_REGISTER(registry, kernel32, UnhandledExceptionFilter);
        MORE_REGISTER(registry, kernel32, AddVectoredExceptionHandler);
        MORE_REGISTER(registry, kernel32, RemoveVectoredExceptionHandler);
        MORE_REGISTER(registry, kernel32, RaiseException);
        MORE_REGISTER(registry, kernel32, GetProcessAffinityMask);
        MORE_REGISTER(registry, kernel32, SetProcessAffinityMask);
        MORE_REGISTER(registry, kernel32, GetLastError);
        MORE_REGISTER(registry, kernel32, SetLastError);
    }

}
