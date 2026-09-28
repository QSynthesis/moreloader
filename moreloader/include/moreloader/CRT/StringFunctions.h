#ifndef MORELOADER_CRT_STRINGFUNCTIONS_H
#define MORELOADER_CRT_STRINGFUNCTIONS_H

#include <cstddef>
#include <cstdint>
#include <string>

namespace more::loader::msvcrt {

    /// Returns the message of \c strerror of msvcrt for the msvcrt error number \a error. Numbers
    /// outside the table yield \c Unknown \c error.
    const char *errorMessage(int error);

    /// Maps the host \c errno value \a hostError to the msvcrt error number. The numbers from 1
    /// to 34 agree. Unmapped values yield \c EINVAL of msvcrt.
    int errorNumber(int hostError);

    /// Fields of \c struct \c tm, which have the same layout in msvcrt and glibc.
    struct TimeFields {
        int second;
        int minute;
        int hour;
        int dayOfMonth;
        int month;
        int yearSince1900;
        int dayOfWeek;
        int dayOfYear;
        int daylightSaving;
    };

    /// Returns the text of \c asctime of msvcrt, which pads the day of the month with a zero
    /// where glibc pads it with a space.
    std::string asctime(const TimeFields &time);

    /// Returns the text of \c _ultoa of msvcrt, in lowercase digits.
    std::string ultoa(std::uint32_t value, int radix);

    /// Compares as \c _strnicmp of msvcrt in the C locale: the letters A to Z are folded to
    /// lowercase before comparison, other bytes are compared as unsigned values, and the result
    /// is -1, 0 or 1.
    int compareInsensitive(const char *a, const char *b, std::size_t count);

}

#endif // MORELOADER_CRT_STRINGFUNCTIONS_H
