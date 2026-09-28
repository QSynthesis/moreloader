#include "CRTExports_p.h"

#include <time.h>

#include <algorithm>
#include <cstring>

#include "StringFunctions.h"

namespace more::loader::msvcrt {

    namespace {

        // The time_t of msvcrt.dll has 32 bits. struct tm has the same nine int fields in
        // msvcrt and glibc.
        using Time32 = std::int32_t;

        thread_local TimeFields t_gmtime;
        thread_local char t_asctime[32];

        std::int64_t monotonicMilliseconds() {
            timespec now;
            ::clock_gettime(CLOCK_MONOTONIC, &now);
            return std::int64_t(now.tv_sec) * 1000 + now.tv_nsec / 1000000;
        }

        // clock of msvcrt measures from the start of the process.
        const std::int64_t s_processStart = monotonicMilliseconds();

        Time32 MORE_CDECL msvcrt_time(Time32 *result) {
            Time32 now = Time32(::time(nullptr));
            if (result) {
                *result = now;
            }
            return now;
        }

        TimeFields *MORE_CDECL msvcrt_gmtime(const Time32 *time) {
            if (!time || *time < 0) {
                threadErrno() = ErrnoInvalid;
                return nullptr;
            }
            time_t value = *time;
            struct tm fields;
            if (!::gmtime_r(&value, &fields)) {
                threadErrno() = ErrnoInvalid;
                return nullptr;
            }
            t_gmtime = {fields.tm_sec,  fields.tm_min,  fields.tm_hour,
                        fields.tm_mday, fields.tm_mon,  fields.tm_year,
                        fields.tm_wday, fields.tm_yday, fields.tm_isdst};
            return &t_gmtime;
        }

        char *MORE_CDECL msvcrt_asctime(const TimeFields *time) {
            std::string text = asctime(*time);
            std::memcpy(t_asctime, text.c_str(), std::min(text.size() + 1, sizeof t_asctime));
            t_asctime[sizeof t_asctime - 1] = 0;
            return t_asctime;
        }

        /// Returns milliseconds since the start of the process, as \c CLOCKS_PER_SEC is 1000 in
        /// msvcrt.
        std::int32_t MORE_CDECL msvcrt_clock() {
            return std::int32_t(monotonicMilliseconds() - s_processStart);
        }

    }

    void registerTimeExports(ExportRegistry &registry) {
        MORE_REGISTER(registry, msvcrt, time);
        MORE_REGISTER(registry, msvcrt, gmtime);
        MORE_REGISTER(registry, msvcrt, asctime);
        MORE_REGISTER(registry, msvcrt, clock);
    }

}
