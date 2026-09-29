#include "WinAPI_p.h"

#include <sched.h>
#include <time.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>

#include <moreloader/Runtime/GuestThread.h>
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

        // Returns the offset of UTC from the local time of the host in minutes, the Bias of
        // Windows, including daylight saving time if it is in effect now.
        std::int32_t currentBias() {
            time_t now = ::time(nullptr);
            struct tm local;
            if (!::localtime_r(&now, &local)) {
                return 0;
            }
            return std::int32_t(-local.tm_gmtoff / 60);
        }

        // Returns the days since 1970-01-01 of a date of the proleptic Gregorian calendar, and
        // the reverse, after the algorithms of Howard Hinnant for civil dates.
        void civilFromDays(std::int64_t days, std::int64_t &year, unsigned &month, unsigned &day) {
            days += 719468;
            std::int64_t era = (days >= 0 ? days : days - 146096) / 146097;
            unsigned dayOfEra = unsigned(days - era * 146097);
            unsigned yearOfEra =
                (dayOfEra - dayOfEra / 1460 + dayOfEra / 36524 - dayOfEra / 146096) / 365;
            year = std::int64_t(yearOfEra) + era * 400;
            unsigned dayOfYear = dayOfEra - (365 * yearOfEra + yearOfEra / 4 - yearOfEra / 100);
            unsigned monthIndex = (5 * dayOfYear + 2) / 153;
            day = dayOfYear - (153 * monthIndex + 2) / 5 + 1;
            month = monthIndex < 10 ? monthIndex + 3 : monthIndex - 9;
            year += month <= 2;
        }

        // -----------------------------------------------------------------------------------

        /// Converts a file time to the calendar fields in the same time base. Values of 2^63 and
        /// above fail with \c ERROR_INVALID_PARAMETER, as Windows documents.
        BOOL MORE_WINAPI kernel32_FileTimeToSystemTime(const FILETIME32 *fileTime,
                                                       SYSTEMTIME32 *systemTime) {
            std::uint64_t value =
                (std::uint64_t(fileTime->dwHighDateTime) << 32) | fileTime->dwLowDateTime;
            if (value >> 63) {
                setLastError(ErrorInvalidParameter);
                return FALSE;
            }
            std::uint64_t milliseconds = value / 10000;
            std::int64_t days =
                std::int64_t(milliseconds / 86400000) - std::int64_t(fileTimeEpochOffset / 86400);
            std::uint64_t millisecondOfDay = milliseconds % 86400000;
            std::int64_t year;
            unsigned month;
            unsigned day;
            civilFromDays(days, year, month, day);
            systemTime->wYear = std::uint16_t(year);
            systemTime->wMonth = std::uint16_t(month);
            // 1970-01-01 was a Thursday, day 4 of a week that begins on Sunday.
            systemTime->wDayOfWeek = std::uint16_t(((days % 7) + 7 + 4) % 7);
            systemTime->wDay = std::uint16_t(day);
            systemTime->wHour = std::uint16_t(millisecondOfDay / 3600000);
            systemTime->wMinute = std::uint16_t(millisecondOfDay / 60000 % 60);
            systemTime->wSecond = std::uint16_t(millisecondOfDay / 1000 % 60);
            systemTime->wMilliseconds = std::uint16_t(millisecondOfDay % 1000);
            return TRUE;
        }

        /// Converts a UTC file time to local time with the bias that is in effect now, as
        /// Windows does regardless of the date of the file time.
        BOOL MORE_WINAPI kernel32_FileTimeToLocalFileTime(const FILETIME32 *fileTime,
                                                          FILETIME32 *localFileTime) {
            std::uint64_t value =
                (std::uint64_t(fileTime->dwHighDateTime) << 32) | fileTime->dwLowDateTime;
            value -= std::uint64_t(std::int64_t(currentBias()) * 60 * 10000000);
            localFileTime->dwLowDateTime = std::uint32_t(value);
            localFileTime->dwHighDateTime = std::uint32_t(value >> 32);
            return TRUE;
        }

        /// Describes the time zone of the host by the bias that is in effect now, without
        /// transition dates. The C runtime of Visual C++ 6 then applies no daylight saving time,
        /// and its conversions agree with FileTimeToLocalFileTime.
        ///
        /// \return \c TIME_ZONE_ID_UNKNOWN (0)
        DWORD MORE_WINAPI kernel32_GetTimeZoneInformation(TIME_ZONE_INFORMATION32 *information) {
            std::memset(information, 0, sizeof *information);
            information->Bias = currentBias();
            return 0;
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
        MORE_REGISTER(registry, kernel32, FileTimeToSystemTime);
        MORE_REGISTER(registry, kernel32, FileTimeToLocalFileTime);
        MORE_REGISTER(registry, kernel32, GetTimeZoneInformation);
    }

}
