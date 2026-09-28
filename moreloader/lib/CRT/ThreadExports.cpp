#include "CRTExports_p.h"

#include <moreloader/Runtime/GuestThread.h>
#include <moreloader/Runtime/KernelObjects.h>
#include <moreloader/Support/Diagnostics.h>

namespace more::loader::msvcrt {

    namespace {

        constexpr std::uint32_t createSuspended = 0x4;

        /// Creates a guest thread. winpthreads creates every thread suspended and resumes it
        /// with \c ResumeThread.
        std::uint32_t MORE_CDECL msvcrt__beginthreadex(void *security, std::uint32_t stackSize,
                                                      GuestThreadStart start, void *parameter,
                                                      std::uint32_t flags,
                                                      std::uint32_t *threadID) {
            if (!start) {
                threadErrno() = ErrnoInvalid;
                return 0;
            }
            auto thread =
                GuestThread::create(start, parameter, stackSize, (flags & createSuspended) != 0);
            if (!thread) {
                threadErrno() = ErrnoAgain;
                return 0;
            }
            if (threadID) {
                *threadID = (*thread)->threadID();
            }
            return handleTable().insert(*thread);
        }

        void MORE_CDECL msvcrt__endthreadex(std::uint32_t exitCode) {
            GuestThread *thread = GuestThread::current();
            if (!thread) {
                fatal("_endthreadex called outside a guest thread");
            }
            thread->exit(exitCode);
        }

    }

    void registerThreadExports(ExportRegistry &registry) {
        MORE_REGISTER(registry, msvcrt, _beginthreadex);
        MORE_REGISTER(registry, msvcrt, _endthreadex);
    }

}
