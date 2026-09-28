#include "WinAPI_p.h"

#include <moreloader/Runtime/GuestLayout.h>
#include <moreloader/Runtime/GuestThread.h>
#include <moreloader/Runtime/KernelObjects.h>
#include <moreloader/Runtime/Process.h>
#include <moreloader/Support/Diagnostics.h>

namespace more::loader::winapi {

    namespace {

        constexpr DWORD tlsOutOfIndexes = 0xFFFFFFFF;
        constexpr std::int32_t threadPriorityNormal = 0;

        std::shared_ptr<ThreadObject> threadOf(HANDLE handle) {
            if (handle == currentThreadHandle) {
                GuestThread *thread = GuestThread::current();
                return thread ? thread->object() : nullptr;
            }
            return handleTable().get<ThreadObject>(handle);
        }

        void reportUnsupported(const char *function) {
            if (isDiagnosticEnabled(DiagnosticCategory::Stubs)) {
                diagnostic("%s is not supported and fails", function);
            }
        }

        // -----------------------------------------------------------------------------------

        HANDLE MORE_WINAPI kernel32_GetCurrentThread() {
            return currentThreadHandle;
        }

        DWORD MORE_WINAPI kernel32_GetCurrentThreadId() {
            GuestThread *thread = GuestThread::current();
            return thread ? thread->threadID() : 0;
        }

        std::int32_t MORE_WINAPI kernel32_GetThreadPriority(HANDLE handle) {
            return threadPriorityNormal;
        }

        /// Accepts every priority without effect. The host scheduler is not changed.
        BOOL MORE_WINAPI kernel32_SetThreadPriority(HANDLE handle, std::int32_t priority) {
            return TRUE;
        }

        /// Decrements the suspend count of a thread created suspended. winpthreads creates each
        /// thread suspended and resumes it after setting its priority.
        DWORD MORE_WINAPI kernel32_ResumeThread(HANDLE handle) {
            auto thread = threadOf(handle);
            if (!thread) {
                setLastError(ErrorInvalidHandle);
                return DWORD(-1);
            }
            return thread->resume();
        }

        /// Suspending a running thread is not supported. winpthreads calls it only to deliver
        /// cancellation, which moresampler does not use.
        DWORD MORE_WINAPI kernel32_SuspendThread(HANDLE handle) {
            reportUnsupported("SuspendThread");
            setLastError(ErrorNotSupported);
            return DWORD(-1);
        }

        BOOL MORE_WINAPI kernel32_GetThreadContext(HANDLE handle, void *context) {
            reportUnsupported("GetThreadContext");
            setLastError(ErrorNotSupported);
            return FALSE;
        }

        BOOL MORE_WINAPI kernel32_SetThreadContext(HANDLE handle, const void *context) {
            reportUnsupported("SetThreadContext");
            setLastError(ErrorNotSupported);
            return FALSE;
        }

        /// Allocates one of the 64 slots in the TEB. Expansion slots are not provided.
        DWORD MORE_WINAPI kernel32_TlsAlloc() {
            auto slot = process().allocateTLSSlot();
            if (!slot) {
                setLastError(ErrorNoMoreItems);
                return tlsOutOfIndexes;
            }
            return *slot;
        }

        /// Returns the value of the slot and sets the last error to \c ERROR_SUCCESS, as Windows
        /// does, so that a caller can distinguish a stored zero from a failure.
        std::uint32_t MORE_WINAPI kernel32_TlsGetValue(DWORD slot) {
            GuestThread *thread = GuestThread::current();
            if (!thread || slot >= tlsMinimumAvailable) {
                setLastError(ErrorInvalidParameter);
                return 0;
            }
            setLastError(ErrorSuccess);
            return thread->teb()->TlsSlots[slot];
        }

        BOOL MORE_WINAPI kernel32_TlsSetValue(DWORD slot, std::uint32_t value) {
            GuestThread *thread = GuestThread::current();
            if (!thread || slot >= tlsMinimumAvailable) {
                setLastError(ErrorInvalidParameter);
                return FALSE;
            }
            thread->teb()->TlsSlots[slot] = value;
            return TRUE;
        }

    }

    void registerKernel32Thread(ExportRegistry &registry) {
        MORE_REGISTER(registry, kernel32, GetCurrentThread);
        MORE_REGISTER(registry, kernel32, GetCurrentThreadId);
        MORE_REGISTER(registry, kernel32, GetThreadPriority);
        MORE_REGISTER(registry, kernel32, SetThreadPriority);
        MORE_REGISTER(registry, kernel32, ResumeThread);
        MORE_REGISTER(registry, kernel32, SuspendThread);
        MORE_REGISTER(registry, kernel32, GetThreadContext);
        MORE_REGISTER(registry, kernel32, SetThreadContext);
        MORE_REGISTER(registry, kernel32, TlsAlloc);
        MORE_REGISTER(registry, kernel32, TlsGetValue);
        MORE_REGISTER(registry, kernel32, TlsSetValue);
    }

}
