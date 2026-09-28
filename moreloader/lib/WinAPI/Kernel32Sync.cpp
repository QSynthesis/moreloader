#include "WinAPI_p.h"

#include <mutex>
#include <vector>

#include <moreloader/Runtime/GuestLayout.h>
#include <moreloader/Runtime/GuestThread.h>
#include <moreloader/Runtime/KernelObjects.h>
#include <moreloader/Support/Diagnostics.h>

namespace more::loader::winapi {

    namespace {

        constexpr DWORD duplicateCloseSource = 1;

        // The lock of a critical section is a host recursive mutex, whose address is stored in
        // LockSemaphore. A critical section that the guest did not initialize receives its lock
        // on first use, which Windows does not do but which costs nothing.
        std::mutex s_initializationMutex;

        //
        // The guest initializes each critical section before any thread uses it, therefore the
        // field is read without synchronization once it is set.
        std::recursive_mutex *lockOf(CRITICAL_SECTION32 *section) {
            if (!section->LockSemaphore) {
                std::lock_guard<std::mutex> lock(s_initializationMutex);
                if (!section->LockSemaphore) {
                    auto mutex = new std::recursive_mutex;
                    section->LockSemaphore =
                        static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(mutex));
                }
            }
            return reinterpret_cast<std::recursive_mutex *>(
                static_cast<std::uintptr_t>(section->LockSemaphore));
        }

        std::uint32_t currentThreadID() {
            GuestThread *thread = GuestThread::current();
            return thread ? thread->threadID() : 0;
        }

        // Returns the object of a handle for waiting, including the pseudo handle of the
        // current thread.
        std::shared_ptr<WaitableObject> waitableOf(HANDLE handle) {
            if (handle == currentThreadHandle) {
                GuestThread *thread = GuestThread::current();
                return thread ? thread->object() : nullptr;
            }
            return handleTable().get<WaitableObject>(handle);
        }

        // -----------------------------------------------------------------------------------

        void MORE_WINAPI kernel32_InitializeCriticalSection(CRITICAL_SECTION32 *section) {
            section->DebugInfo = 0;
            section->LockCount = -1;
            section->RecursionCount = 0;
            section->OwningThread = 0;
            section->SpinCount = 0;
            auto mutex = new std::recursive_mutex;
            section->LockSemaphore =
                static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(mutex));
        }

        void MORE_WINAPI kernel32_DeleteCriticalSection(CRITICAL_SECTION32 *section) {
            delete reinterpret_cast<std::recursive_mutex *>(
                static_cast<std::uintptr_t>(section->LockSemaphore));
            section->LockSemaphore = 0;
        }

        void MORE_WINAPI kernel32_EnterCriticalSection(CRITICAL_SECTION32 *section) {
            lockOf(section)->lock();
            section->OwningThread = currentThreadID();
            ++section->RecursionCount;
        }

        BOOL MORE_WINAPI kernel32_TryEnterCriticalSection(CRITICAL_SECTION32 *section) {
            if (!lockOf(section)->try_lock()) {
                return FALSE;
            }
            section->OwningThread = currentThreadID();
            ++section->RecursionCount;
            return TRUE;
        }

        void MORE_WINAPI kernel32_LeaveCriticalSection(CRITICAL_SECTION32 *section) {
            if (--section->RecursionCount == 0) {
                section->OwningThread = 0;
            }
            lockOf(section)->unlock();
        }

        HANDLE MORE_WINAPI kernel32_CreateEventA(void *attributes, BOOL manualReset,
                                                 BOOL initialState, const char *name) {
            if (name && isDiagnosticEnabled(DiagnosticCategory::Stubs)) {
                diagnostic("CreateEventA(\"%s\"): named events are created unnamed", name);
            }
            return handleTable().insert(
                std::make_shared<EventObject>(manualReset != 0, initialState != 0));
        }

        BOOL MORE_WINAPI kernel32_SetEvent(HANDLE handle) {
            auto event = handleTable().get<EventObject>(handle);
            if (!event) {
                setLastError(ErrorInvalidHandle);
                return FALSE;
            }
            event->set();
            return TRUE;
        }

        BOOL MORE_WINAPI kernel32_ResetEvent(HANDLE handle) {
            auto event = handleTable().get<EventObject>(handle);
            if (!event) {
                setLastError(ErrorInvalidHandle);
                return FALSE;
            }
            event->reset();
            return TRUE;
        }

        HANDLE createSemaphore(std::int32_t initial, std::int32_t maximum) {
            if (maximum <= 0 || initial < 0 || initial > maximum) {
                setLastError(ErrorInvalidParameter);
                return 0;
            }
            return handleTable().insert(std::make_shared<SemaphoreObject>(initial, maximum));
        }

        HANDLE MORE_WINAPI kernel32_CreateSemaphoreA(void *attributes, std::int32_t initial,
                                                     std::int32_t maximum, const char *name) {
            return createSemaphore(initial, maximum);
        }

        HANDLE MORE_WINAPI kernel32_CreateSemaphoreW(void *attributes, std::int32_t initial,
                                                     std::int32_t maximum, const char16_t *name) {
            return createSemaphore(initial, maximum);
        }

        BOOL MORE_WINAPI kernel32_ReleaseSemaphore(HANDLE handle, std::int32_t count,
                                                   std::int32_t *previous) {
            auto semaphore = handleTable().get<SemaphoreObject>(handle);
            if (!semaphore) {
                setLastError(ErrorInvalidHandle);
                return FALSE;
            }
            auto result = semaphore->release(count);
            if (!result) {
                setLastError(ErrorTooManyPosts);
                return FALSE;
            }
            if (previous) {
                *previous = *result;
            }
            return TRUE;
        }

        DWORD MORE_WINAPI kernel32_WaitForSingleObject(HANDLE handle, DWORD milliseconds) {
            auto object = waitableOf(handle);
            if (!object) {
                setLastError(ErrorInvalidHandle);
                return WaitFailed;
            }
            return waitForObjects({object}, false, milliseconds);
        }

        DWORD MORE_WINAPI kernel32_WaitForMultipleObjects(DWORD count, const HANDLE *handles,
                                                          BOOL waitAll, DWORD milliseconds) {
            if (count == 0 || count > 64) {
                setLastError(ErrorInvalidParameter);
                return WaitFailed;
            }
            std::vector<std::shared_ptr<WaitableObject>> objects;
            for (DWORD i = 0; i < count; ++i) {
                auto object = waitableOf(handles[i]);
                if (!object) {
                    setLastError(ErrorInvalidHandle);
                    return WaitFailed;
                }
                objects.push_back(std::move(object));
            }
            return waitForObjects(objects, waitAll != 0, milliseconds);
        }

        BOOL MORE_WINAPI kernel32_CloseHandle(HANDLE handle) {
            if (handle == currentProcessHandle || handle == currentThreadHandle) {
                return TRUE;
            }
            if (!handleTable().close(handle)) {
                setLastError(ErrorInvalidHandle);
                return FALSE;
            }
            return TRUE;
        }

        /// Duplicates a handle within the process. winpthreads converts the pseudo handle of the
        /// current thread into a real handle this way.
        BOOL MORE_WINAPI kernel32_DuplicateHandle(HANDLE sourceProcess, HANDLE source,
                                                  HANDLE targetProcess, HANDLE *target,
                                                  DWORD access, BOOL inherit, DWORD options) {
            if (sourceProcess != currentProcessHandle || targetProcess != currentProcessHandle) {
                setLastError(ErrorInvalidHandle);
                return FALSE;
            }
            std::shared_ptr<KernelObject> object;
            if (source == currentThreadHandle) {
                GuestThread *thread = GuestThread::current();
                object = thread ? thread->object() : nullptr;
            } else {
                object = handleTable().get(source);
            }
            if (!object) {
                setLastError(ErrorInvalidHandle);
                return FALSE;
            }
            if (target) {
                *target = handleTable().insert(object);
            }
            if (options & duplicateCloseSource) {
                handleTable().close(source);
            }
            return TRUE;
        }

        /// Reports no flags for a valid handle. winpthreads uses the call to validate handles.
        BOOL MORE_WINAPI kernel32_GetHandleInformation(HANDLE handle, DWORD *flags) {
            bool valid = handle == currentProcessHandle || handle == currentThreadHandle ||
                         handleTable().get(handle) != nullptr;
            if (!valid) {
                setLastError(ErrorInvalidHandle);
                return FALSE;
            }
            if (flags) {
                *flags = 0;
            }
            return TRUE;
        }

    }

    void registerKernel32Sync(ExportRegistry &registry) {
        MORE_REGISTER(registry, kernel32, InitializeCriticalSection);
        MORE_REGISTER(registry, kernel32, DeleteCriticalSection);
        MORE_REGISTER(registry, kernel32, EnterCriticalSection);
        MORE_REGISTER(registry, kernel32, TryEnterCriticalSection);
        MORE_REGISTER(registry, kernel32, LeaveCriticalSection);
        MORE_REGISTER(registry, kernel32, CreateEventA);
        MORE_REGISTER(registry, kernel32, SetEvent);
        MORE_REGISTER(registry, kernel32, ResetEvent);
        MORE_REGISTER(registry, kernel32, CreateSemaphoreA);
        MORE_REGISTER(registry, kernel32, CreateSemaphoreW);
        MORE_REGISTER(registry, kernel32, ReleaseSemaphore);
        MORE_REGISTER(registry, kernel32, WaitForSingleObject);
        MORE_REGISTER(registry, kernel32, WaitForMultipleObjects);
        MORE_REGISTER(registry, kernel32, CloseHandle);
        MORE_REGISTER(registry, kernel32, DuplicateHandle);
        MORE_REGISTER(registry, kernel32, GetHandleInformation);
    }

}
