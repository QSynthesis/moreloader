#include "StringFunctions.h"

#include <cerrno>
#include <cstdio>

namespace more::loader::msvcrt {

    // Measured with strerror of msvcrt.dll for 0 to 45.
    static const char *const errorMessages[] = {
        "No error",
        "Operation not permitted",
        "No such file or directory",
        "No such process",
        "Interrupted function call",
        "Input/output error",
        "No such device or address",
        "Arg list too long",
        "Exec format error",
        "Bad file descriptor",
        "No child processes",
        "Resource temporarily unavailable",
        "Not enough space",
        "Permission denied",
        "Bad address",
        "Unknown error",
        "Resource device",
        "File exists",
        "Improper link",
        "No such device",
        "Not a directory",
        "Is a directory",
        "Invalid argument",
        "Too many open files in system",
        "Too many open files",
        "Inappropriate I/O control operation",
        "Unknown error",
        "File too large",
        "No space left on device",
        "Invalid seek",
        "Read-only file system",
        "Too many links",
        "Broken pipe",
        "Domain error",
        "Result too large",
        "Unknown error",
        "Resource deadlock avoided",
        "Unknown error",
        "Filename too long",
        "No locks available",
        "Function not implemented",
        "Directory not empty",
        "Illegal byte sequence",
    };

    const char *errorMessage(int error) {
        if (error < 0 || size_t(error) >= sizeof errorMessages / sizeof errorMessages[0]) {
            return "Unknown error";
        }
        return errorMessages[error];
    }

    int errorNumber(int hostError) {
        if (hostError >= 0 && hostError <= 34) {
            return hostError;
        }
        switch (hostError) {
            case EDEADLK:
                return 36;
            case ENAMETOOLONG:
                return 38;
            case ENOLCK:
                return 39;
            case ENOSYS:
                return 40;
            case ENOTEMPTY:
                return 41;
            case EILSEQ:
                return 42;
            default:
                return 22;
        }
    }

    std::string asctime(const TimeFields &time) {
        static const char days[] = "SunMonTueWedThuFriSat";
        static const char months[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
        char buffer[64];
        int day = time.dayOfWeek >= 0 && time.dayOfWeek < 7 ? time.dayOfWeek : 0;
        int month = time.month >= 0 && time.month < 12 ? time.month : 0;
        std::snprintf(buffer, sizeof buffer, "%.3s %.3s %02d %02d:%02d:%02d %d\n",
                      days + day * 3, months + month * 3, time.dayOfMonth, time.hour,
                      time.minute, time.second, 1900 + time.yearSince1900);
        return buffer;
    }

    std::string ultoa(std::uint32_t value, int radix) {
        if (radix < 2 || radix > 36) {
            return {};
        }
        std::string out;
        do {
            unsigned digit = value % unsigned(radix);
            out.insert(out.begin(), char(digit < 10 ? '0' + digit : 'a' + digit - 10));
            value /= unsigned(radix);
        } while (value != 0);
        return out;
    }

    int compareInsensitive(const char *a, const char *b, std::size_t count) {
        for (std::size_t i = 0; i < count; ++i) {
            unsigned x = static_cast<unsigned char>(a[i]);
            unsigned y = static_cast<unsigned char>(b[i]);
            if (x >= 'A' && x <= 'Z') {
                x += 'a' - 'A';
            }
            if (y >= 'A' && y <= 'Z') {
                y += 'a' - 'A';
            }
            if (x != y) {
                return x < y ? -1 : 1;
            }
            if (x == 0) {
                break;
            }
        }
        return 0;
    }

}
