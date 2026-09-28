#include "WinAPI_p.h"

#include <sched.h>
#include <time.h>
#include <unistd.h>

#include <cerrno>

#include <moreloader/Runtime/GuestLayout.h>
#include <moreloader/Runtime/KernelObjects.h>

namespace more::loader::winapi {

    namespace {

        // Seconds from 1601-01-01, the epoch of FILETIME, to 1970-01-01.
        constexpr std::uint64_t fileTimeEpochOffset = 11644473600ull;

        std::uint64_t monotonicNanoseconds() {
            timespec now;
            ::clock_gettime(CLOCK_MONOTONIC, &now);
            return std::uint64_t(now.tv_sec) * 1000000000ull + std::uint64_t(now.tv_nsec);
        }

        void MORE_WINAPI kernel32_GetSystemTimeAsFileTime(FILETIME32 *time) {
            timespec now;
            ::clock_gettime(CLOCK_REALTIME, &now);
            std::uint64_t value = (std::uint64_t(now.tv_sec) + fileTimeEpochOffset) * 10000000ull +
                                  std::uint64_t(now.tv_nsec) / 100;
            time->dwLowDateTime = std::uint32_t(value);
            time->dwHighDateTime = std::uint32_t(value >> 32);
        }

        /// Returns a counter of 100-nanosecond units, the frequency that
        /// \c QueryPerformanceFrequency reports on current Windows. moresampler does not import
        /// \c QueryPerformanceFrequency.
        BOOL MORE_WINAPI kernel32_QueryPerformanceCounter(std::int64_t *counter) {
            *counter = std::int64_t(monotonicNanoseconds() / 100);
            return TRUE;
        }

        DWORD MORE_WINAPI kernel32_GetTickCount() {
            return DWORD(monotonicNanoseconds() / 1000000);
        }

        void MORE_WINAPI kernel32_Sleep(DWORD milliseconds) {
            if (milliseconds == 0) {
                ::sched_yield();
                return;
            }
            if (milliseconds == infiniteTimeout) {
                for (;;) {
                    ::pause();
                }
            }
            timespec duration{time_t(milliseconds / 1000), long(milliseconds % 1000) * 1000000L};
            while (::nanosleep(&duration, &duration) != 0 && errno == EINTR) {
            }
        }

    }

    void registerKernel32Time(ExportRegistry &registry) {
        MORE_REGISTER(registry, kernel32, GetSystemTimeAsFileTime);
        MORE_REGISTER(registry, kernel32, QueryPerformanceCounter);
        MORE_REGISTER(registry, kernel32, GetTickCount);
        MORE_REGISTER(registry, kernel32, Sleep);
    }

}
