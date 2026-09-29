#include "CRTExports_p.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>

#include <moreloader/Runtime/Process.h>
#include <moreloader/Support/CodePage.h>
#include <moreloader/Support/Diagnostics.h>
#include <moreloader/Support/PathMapping.h>

#include "GuestHeap_p.h"
#include "Random.h"
#include "Sort.h"
#include "StringFunctions.h"

namespace more::loader::msvcrt {

    namespace {

        // struct lconv of msvcrt: ten pointers followed by eight chars.
        struct MsvcrtLconv {
            const char *decimal_point;
            const char *thousands_sep;
            const char *grouping;
            const char *int_curr_symbol;
            const char *currency_symbol;
            const char *mon_decimal_point;
            const char *mon_thousands_sep;
            const char *mon_grouping;
            const char *positive_sign;
            const char *negative_sign;
            char int_frac_digits;
            char frac_digits;
            char p_cs_precedes;
            char p_sep_by_space;
            char n_cs_precedes;
            char n_sep_by_space;
            char p_sign_posn;
            char n_sign_posn;
        };
        static_assert(sizeof(MsvcrtLconv) == 48);

        // struct _stat of msvcrt.dll, with 32-bit times and a 32-bit size.
        struct MsvcrtStat32 {
            std::uint32_t st_dev;
            std::uint16_t st_ino;
            std::uint16_t st_mode;
            std::int16_t st_nlink;
            std::int16_t st_uid;
            std::int16_t st_gid;
            std::uint32_t st_rdev;
            std::int32_t st_size;
            // st_atime, st_mtime and st_ctime, which glibc defines as macros.
            std::int32_t accessTime;
            std::int32_t modificationTime;
            std::int32_t creationTime;
        };
        static_assert(sizeof(MsvcrtStat32) == 36);
        static_assert(offsetof(MsvcrtStat32, st_rdev) == 16);

        enum StatMode : std::uint16_t {
            ModeDirectory = 0x4000,
            ModeRegular = 0x8000,
            ModeRead = 0x0100,
            ModeWrite = 0x0080,
            ModeExecute = 0x0040,
        };

        // Signal numbers of msvcrt and the handler values SIG_DFL and SIG_IGN.
        constexpr std::int32_t signalAbort = 22;
        constexpr std::int32_t signalCount = 23;
        constexpr std::uint32_t signalDefault = 0;
        constexpr std::uint32_t signalIgnore = 1;
        constexpr std::uint32_t signalError = std::uint32_t(-1);

        // The drive number of Z: in st_dev.
        constexpr std::uint32_t driveZ = 25;

        std::mutex s_signalMutex;
        std::uint32_t s_signalHandlers[signalCount] = {};

        thread_local Random t_random;

        char *MORE_CDECL msvcrt_getenv(const char *name) {
            return std::getenv(name);
        }

        /// Runs no command, because the loader has no command processor. A null command asks
        /// whether a command processor exists.
        std::int32_t MORE_CDECL msvcrt_system(const char *command) {
            if (!command) {
                return 0;
            }
            if (isDiagnosticEnabled(DiagnosticCategory::Stubs)) {
                diagnostic("system(\"%s\") is not run", command);
            }
            threadErrno() = ErrnoNoEntry;
            return -1;
        }

        /// Reports the C locale for every query and every request of "C" or "". Windows would
        /// switch to the locale of the user for "", which only the Lua interpreter of
        /// moresampler could request.
        char *MORE_CDECL msvcrt_setlocale(std::int32_t category, const char *locale) {
            static char c[] = "C";
            if (!locale || std::strcmp(locale, "C") == 0 || locale[0] == 0) {
                return c;
            }
            return nullptr;
        }

        MsvcrtLconv *MORE_CDECL msvcrt_localeconv() {
            static MsvcrtLconv conventions = {".", "", "", "", "", "", "", "", "", "",
                                              127, 127, 127, 127, 127, 127, 127, 127};
            return &conventions;
        }

        char *MORE_CDECL msvcrt_strerror(std::int32_t error) {
            return const_cast<char *>(errorMessage(error));
        }

        std::int32_t *MORE_CDECL msvcrt__errno() {
            return &threadErrno();
        }

        std::uint32_t MORE_CDECL msvcrt_signal(std::int32_t number, std::uint32_t handler) {
            if (number < 0 || number >= signalCount) {
                threadErrno() = ErrnoInvalid;
                return signalError;
            }
            std::lock_guard<std::mutex> lock(s_signalMutex);
            std::uint32_t previous = s_signalHandlers[number];
            s_signalHandlers[number] = handler;
            return previous;
        }

        [[noreturn]] void abortProcess() {
            static const char message[] =
                "\nThis application has requested the Runtime to terminate it in an unusual way.\n"
                "Please contact the application's support team for more information.\n";
            ::write(STDERR_FILENO, message, sizeof message - 1);
            process().terminate(3);
        }

        /// Calls the handler of the signal, resetting it to the default first, as msvcrt does.
        /// The default action of every signal ends the process with exit code 3.
        std::int32_t MORE_CDECL msvcrt_raise(std::int32_t number) {
            if (number < 0 || number >= signalCount) {
                threadErrno() = ErrnoInvalid;
                return -1;
            }
            std::uint32_t handler;
            {
                std::lock_guard<std::mutex> lock(s_signalMutex);
                handler = s_signalHandlers[number];
                if (handler != signalIgnore) {
                    s_signalHandlers[number] = signalDefault;
                }
            }
            if (handler == signalIgnore) {
                return 0;
            }
            if (handler == signalDefault) {
                if (number == signalAbort) {
                    abortProcess();
                }
                process().terminate(3);
            }
            reinterpret_cast<void (*)(std::int32_t)>(handler)(number);
            return 0;
        }

        void MORE_CDECL msvcrt_abort() {
            msvcrt_raise(signalAbort);
            abortProcess();
        }

        /// Returns the current directory of the host in its guest form.
        char16_t *MORE_CDECL msvcrt__wgetcwd(char16_t *buffer, std::int32_t size) {
            char host[4096];
            if (!::getcwd(host, sizeof host)) {
                setErrnoFromHost(errno);
                return nullptr;
            }
            std::u16string guest = multiByteToWide(guestPathFromHost(host)).text;
            std::size_t needed = guest.size() + 1;
            if (!buffer) {
                std::size_t allocation = std::max<std::size_t>(needed, std::size_t(std::max(size, 0)));
                buffer = static_cast<char16_t *>(
                    guestAllocate(allocation * sizeof(char16_t), false, MORE_RETURN_ADDRESS()));
                if (!buffer) {
                    threadErrno() = ErrnoNoMemory;
                    return nullptr;
                }
            } else if (size < 0 || std::size_t(size) < needed) {
                threadErrno() = ErrnoRange;
                return nullptr;
            }
            std::memcpy(buffer, guest.c_str(), needed * sizeof(char16_t));
            return buffer;
        }

        bool hasExecutableExtension(const std::string &path) {
            std::size_t dot = path.find_last_of('.');
            if (dot == std::string::npos || path.find_first_of("/\\", dot) != std::string::npos) {
                return false;
            }
            std::string extension = path.substr(dot + 1);
            for (char &c : extension) {
                if (c >= 'A' && c <= 'Z') {
                    c = char(c - 'A' + 'a');
                }
            }
            return extension == "exe" || extension == "com" || extension == "bat" ||
                   extension == "cmd";
        }

        /// Describes a file in the \c struct \c _stat of msvcrt. The mode is derived from the
        /// attributes as msvcrt derives it: read permission always, write permission unless
        /// read-only, execute permission for directories and executable extensions, each
        /// replicated to the group and other bits. The creation time stands in for st_ctime
        /// where the host records it, and the modification time otherwise.
        std::int32_t MORE_CDECL msvcrt__wstat(const char16_t *path, MsvcrtStat32 *result) {
            if (!path || !result) {
                threadErrno() = ErrnoInvalid;
                return -1;
            }
            std::string guest = wideToMultiByte(path).text;
            std::string host = hostPathFromGuest(guest);
            struct statx info;
            if (::statx(AT_FDCWD, host.c_str(), 0, STATX_BASIC_STATS | STATX_BTIME, &info) != 0) {
                setErrnoFromHost(errno);
                return -1;
            }
            std::uint16_t mode = ModeRead;
            if (S_ISDIR(info.stx_mode)) {
                mode |= ModeDirectory | ModeExecute;
            } else {
                mode |= ModeRegular;
                if (hasExecutableExtension(guest)) {
                    mode |= ModeExecute;
                }
            }
            if (::access(host.c_str(), W_OK) == 0) {
                mode |= ModeWrite;
            }
            mode |= (mode & 0700) >> 3;
            mode |= (mode & 0700) >> 6;

            std::memset(result, 0, sizeof *result);
            result->st_dev = driveZ;
            result->st_rdev = driveZ;
            result->st_mode = mode;
            result->st_nlink = 1;
            result->st_size = std::int32_t(info.stx_size);
            result->accessTime = std::int32_t(info.stx_atime.tv_sec);
            result->modificationTime = std::int32_t(info.stx_mtime.tv_sec);
            result->creationTime = std::int32_t((info.stx_mask & STATX_BTIME) ? info.stx_btime.tv_sec
                                                                          : info.stx_mtime.tv_sec);
            return 0;
        }

        void MORE_CDECL msvcrt_qsort(void *base, std::uint32_t count, std::uint32_t size,
                                    Comparator compare) {
            quickSort(base, count, size, compare);
        }

        std::int32_t MORE_CDECL msvcrt_rand() {
            return t_random.next();
        }

        void MORE_CDECL msvcrt_srand(std::uint32_t seed) {
            t_random.seed(seed);
        }

    }

    void registerEnvironmentExports(ExportRegistry &registry) {
        MORE_REGISTER(registry, msvcrt, getenv);
        MORE_REGISTER(registry, msvcrt, system);
        MORE_REGISTER(registry, msvcrt, setlocale);
        MORE_REGISTER(registry, msvcrt, localeconv);
        MORE_REGISTER(registry, msvcrt, strerror);
        MORE_REGISTER(registry, msvcrt, _errno);
        MORE_REGISTER(registry, msvcrt, signal);
        MORE_REGISTER(registry, msvcrt, raise);
        MORE_REGISTER(registry, msvcrt, abort);
        MORE_REGISTER(registry, msvcrt, _wgetcwd);
        MORE_REGISTER(registry, msvcrt, _wstat);
        MORE_REGISTER(registry, msvcrt, qsort);
        MORE_REGISTER(registry, msvcrt, rand);
        MORE_REGISTER(registry, msvcrt, srand);
    }

}
