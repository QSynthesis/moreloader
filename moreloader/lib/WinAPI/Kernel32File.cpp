#include "WinAPI_p.h"

#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <string>
#include <vector>

#include <moreloader/Runtime/GuestLayout.h>
#include <moreloader/Runtime/GuestThread.h>
#include <moreloader/Runtime/KernelObjects.h>
#include <moreloader/Support/CodePage.h>
#include <moreloader/Support/Diagnostics.h>
#include <moreloader/Support/PathMapping.h>

namespace more::loader::winapi {

    namespace {

        enum FileAttribute : DWORD {
            FileAttributeReadOnly = 0x1,
            FileAttributeDirectory = 0x10,
            FileAttributeArchive = 0x20,
        };

        enum LockFlag : DWORD {
            LockFileFailImmediately = 0x1,
            LockFileExclusiveLock = 0x2,
        };

        enum DriveType : DWORD {
            DriveNoRootDir = 1,
            DriveFixed = 3,
        };

        constexpr std::uint64_t fileTimeEpochOffset = 11644473600ull;

        char16_t upcase(char16_t c) {
            return c >= u'a' && c <= u'z' ? char16_t(c - u'a' + u'A') : c;
        }

        // Matches \a name against the pattern \a pattern with the wildcards * and ?, without
        // regard to the letter case of ASCII letters, as FindFirstFileW does on NTFS.
        bool matches(std::u16string_view pattern, std::u16string_view name) {
            if (pattern == u"*.*") {
                pattern = u"*";
            }
            std::size_t p = 0;
            std::size_t n = 0;
            std::size_t starPattern = std::u16string_view::npos;
            std::size_t starName = 0;
            while (n < name.size()) {
                if (p < pattern.size() &&
                    (pattern[p] == u'?' || upcase(pattern[p]) == upcase(name[n]))) {
                    ++p;
                    ++n;
                } else if (p < pattern.size() && pattern[p] == u'*') {
                    starPattern = p++;
                    starName = n;
                } else if (starPattern != std::u16string_view::npos) {
                    p = starPattern + 1;
                    n = ++starName;
                } else {
                    return false;
                }
            }
            while (p < pattern.size() && pattern[p] == u'*') {
                ++p;
            }
            return p == pattern.size();
        }

        // NTFS returns names in the order of their uppercase forms, after . and ..
        bool ntfsOrder(const FindObject::Entry &a, const FindObject::Entry &b) {
            auto rank = [](const std::u16string &name) {
                return name == u"." ? 0 : (name == u".." ? 1 : 2);
            };
            if (rank(a.name) != rank(b.name)) {
                return rank(a.name) < rank(b.name);
            }
            std::u16string x = a.name;
            std::u16string y = b.name;
            std::transform(x.begin(), x.end(), x.begin(), upcase);
            std::transform(y.begin(), y.end(), y.begin(), upcase);
            return x < y;
        }

        FILETIME32 fileTimeOf(const timespec &time) {
            std::uint64_t value = (std::uint64_t(time.tv_sec) + fileTimeEpochOffset) * 10000000ull +
                                  std::uint64_t(time.tv_nsec) / 100;
            return FILETIME32{std::uint32_t(value), std::uint32_t(value >> 32)};
        }

        bool describe(const FindObject::Entry &entry, WIN32_FIND_DATAW32 *data) {
            struct stat info;
            if (::stat(entry.hostPath.c_str(), &info) != 0) {
                return false;
            }
            std::memset(data, 0, sizeof *data);
            if (S_ISDIR(info.st_mode)) {
                data->dwFileAttributes = FileAttributeDirectory;
            } else {
                data->dwFileAttributes = FileAttributeArchive;
                data->nFileSizeHigh = std::uint32_t(std::uint64_t(info.st_size) >> 32);
                data->nFileSizeLow = std::uint32_t(info.st_size);
            }
            if (::access(entry.hostPath.c_str(), W_OK) != 0) {
                data->dwFileAttributes |= FileAttributeReadOnly;
            }
            // Linux keeps no creation time in struct stat. The modification time stands in.
            data->ftCreationTime = fileTimeOf(info.st_mtim);
            data->ftLastAccessTime = fileTimeOf(info.st_atim);
            data->ftLastWriteTime = fileTimeOf(info.st_mtim);
            std::size_t length = std::min<std::size_t>(entry.name.size(), 259);
            std::memcpy(data->cFileName, entry.name.data(), length * sizeof(char16_t));
            return true;
        }

        std::shared_ptr<FileObject> fileOf(HANDLE handle) {
            return handleTable().get<FileObject>(handle);
        }

        BOOL lockRange(HANDLE handle, short type, bool wait, DWORD lengthLow, DWORD lengthHigh,
                       const OVERLAPPED32 *overlapped) {
            auto file = fileOf(handle);
            if (!file || !overlapped) {
                setLastError(file ? ErrorInvalidParameter : ErrorInvalidHandle);
                return FALSE;
            }
            std::uint64_t length = (std::uint64_t(lengthHigh) << 32) | lengthLow;
            if (length == 0) {
                return TRUE;
            }
            // Locks of open file descriptions belong to the descriptor, as Windows locks belong
            // to the handle. Process-associated locks would not exclude threads of one process.
            struct flock lock{};
            lock.l_type = type;
            lock.l_whence = SEEK_SET;
            lock.l_start = off_t((std::uint64_t(overlapped->OffsetHigh) << 32) | overlapped->Offset);
            lock.l_len = off_t(length);
            lock.l_pid = 0;
            int command = type == F_UNLCK ? F_OFD_SETLK : (wait ? F_OFD_SETLKW : F_OFD_SETLK);
            while (::fcntl(file->lockDescriptor(), command, &lock) != 0) {
                if (errno == EINTR) {
                    continue;
                }
                if (isDiagnosticEnabled(DiagnosticCategory::Stubs)) {
                    diagnostic("fcntl lock type %d of descriptor %d failed: %s", int(type),
                               file->lockDescriptor(), std::strerror(errno));
                }
                setLastError(type == F_UNLCK ? ErrorNotLocked : ErrorLockViolation);
                return FALSE;
            }
            return TRUE;
        }

        // Returns the current directory of the process in its guest form.
        std::string currentGuestDirectory() {
            char host[4096];
            if (!::getcwd(host, sizeof host)) {
                return "Z:\\";
            }
            return guestPathFromHost(host);
        }

        // Converts the description of an entry for FindFirstFileA. The name is UTF-8, the ANSI
        // code page of the guest, and is truncated at a character boundary if it does not fit.
        void narrowFindData(const WIN32_FIND_DATAW32 &wide, WIN32_FIND_DATAA32 *narrow) {
            std::memset(narrow, 0, sizeof *narrow);
            narrow->dwFileAttributes = wide.dwFileAttributes;
            narrow->ftCreationTime = wide.ftCreationTime;
            narrow->ftLastAccessTime = wide.ftLastAccessTime;
            narrow->ftLastWriteTime = wide.ftLastWriteTime;
            narrow->nFileSizeHigh = wide.nFileSizeHigh;
            narrow->nFileSizeLow = wide.nFileSizeLow;
            std::string name = wideToMultiByte(wide.cFileName).text;
            std::memcpy(narrow->cFileName, name.data(),
                        wholeCharacterPrefix(name, sizeof narrow->cFileName - 1));
        }

        /// Enumerates a directory. The entries are collected at once and sorted in the order of
        /// NTFS, so that the guest sees the order that it sees on Windows.
        HANDLE findFirst(std::u16string guest, WIN32_FIND_DATAW32 *data) {
            std::size_t separator = guest.find_last_of(u"\\/");
            std::u16string directory =
                separator == std::u16string::npos ? u"." : guest.substr(0, separator + 1);
            std::u16string filePattern =
                separator == std::u16string::npos ? guest : guest.substr(separator + 1);

            std::string hostDirectory = hostPathFromGuest(wideToMultiByte(directory).text);
            if (hostDirectory.empty()) {
                hostDirectory = ".";
            }
            DIR *dir = ::opendir(hostDirectory.c_str());
            if (!dir) {
                setLastError(ErrorPathNotFound);
                return INVALID_HANDLE_VALUE;
            }
            std::vector<FindObject::Entry> entries;
            while (dirent *entry = ::readdir(dir)) {
                std::u16string name = multiByteToWide(entry->d_name).text;
                if (matches(filePattern, name)) {
                    std::string path = hostDirectory;
                    if (path.back() != '/') {
                        path.push_back('/');
                    }
                    entries.push_back({path + entry->d_name, name});
                }
            }
            ::closedir(dir);
            std::sort(entries.begin(), entries.end(), ntfsOrder);

            auto find = std::make_shared<FindObject>(std::move(entries));
            for (;;) {
                auto entry = find->next();
                if (!entry) {
                    setLastError(ErrorFileNotFound);
                    return INVALID_HANDLE_VALUE;
                }
                if (describe(*entry, data)) {
                    return handleTable().insert(find);
                }
            }
        }

        // -----------------------------------------------------------------------------------

        /// Enumerates a directory in the order of NTFS.
        HANDLE MORE_WINAPI kernel32_FindFirstFileW(const char16_t *pattern,
                                                   WIN32_FIND_DATAW32 *data) {
            return findFirst(pattern, data);
        }

        /// Enumerates a directory in the order of NTFS. The pattern is UTF-8, the ANSI code page
        /// of the guest. \c _stat of Visual C++ 6 describes a file this way.
        HANDLE MORE_WINAPI kernel32_FindFirstFileA(const char *pattern, WIN32_FIND_DATAA32 *data) {
            WIN32_FIND_DATAW32 wide;
            HANDLE handle = findFirst(multiByteToWide(pattern).text, &wide);
            if (handle != INVALID_HANDLE_VALUE) {
                narrowFindData(wide, data);
            }
            return handle;
        }

        BOOL MORE_WINAPI kernel32_FindClose(HANDLE handle) {
            if (!handleTable().get<FindObject>(handle) || !handleTable().close(handle)) {
                setLastError(ErrorInvalidHandle);
                return FALSE;
            }
            return TRUE;
        }

        /// Copies the current directory in its guest form, such as <tt>Z:\\home\\user</tt>.
        ///
        /// \return the length without the terminator, or the size including the terminator
        ///         if \a size is insufficient, as measured
        DWORD MORE_WINAPI kernel32_GetCurrentDirectoryA(DWORD size, char *buffer) {
            std::string guest = currentGuestDirectory();
            if (!buffer || size <= guest.size()) {
                return DWORD(guest.size() + 1);
            }
            std::memcpy(buffer, guest.c_str(), guest.size() + 1);
            return DWORD(guest.size());
        }

        /// Returns the absolute guest path of \a name as measured on Windows.
        ///
        /// \return the length without the terminator, or the size including the terminator
        ///         if \a size is insufficient, as measured
        DWORD MORE_WINAPI kernel32_GetFullPathNameA(const char *name, DWORD size, char *buffer,
                                                    char **filePart) {
            if (!name || !*name) {
                setLastError(ErrorInvalidName);
                return 0;
            }
            FullGuestPath full = fullGuestPath(name, currentGuestDirectory());
            if (!buffer || size <= full.path.size()) {
                return DWORD(full.path.size() + 1);
            }
            std::memcpy(buffer, full.path.c_str(), full.path.size() + 1);
            if (filePart) {
                *filePart = full.filePart ? buffer + *full.filePart : nullptr;
            }
            return DWORD(full.path.size());
        }

        /// Reports the drive \c Z:, which holds the host root, as a fixed drive and any other
        /// root as absent. \c _stat of Visual C++ 6 queries the root of a path that
        /// FindFirstFileA cannot describe.
        DWORD MORE_WINAPI kernel32_GetDriveTypeA(const char *root) {
            if (!root) {
                return DriveFixed;
            }
            std::string_view text(root);
            bool isDriveZ = text.size() == 3 && (text[0] == 'Z' || text[0] == 'z') &&
                            text[1] == ':' && (text[2] == '\\' || text[2] == '/');
            return isDriveZ ? DriveFixed : DriveNoRootDir;
        }

        BOOL MORE_WINAPI kernel32_FindNextFileW(HANDLE handle, WIN32_FIND_DATAW32 *data) {
            auto find = handleTable().get<FindObject>(handle);
            if (!find) {
                setLastError(ErrorInvalidHandle);
                return FALSE;
            }
            for (;;) {
                auto entry = find->next();
                if (!entry) {
                    setLastError(ErrorNoMoreFiles);
                    return FALSE;
                }
                if (describe(*entry, data)) {
                    return TRUE;
                }
            }
        }

        /// Locks a byte range of a file opened through msvcrt, whose handle comes from
        /// \c _get_osfhandle. moresampler locks \c desc.mrq, which several of its processes
        /// share.
        BOOL MORE_WINAPI kernel32_LockFileEx(HANDLE handle, DWORD flags, DWORD reserved,
                                             DWORD lengthLow, DWORD lengthHigh,
                                             OVERLAPPED32 *overlapped) {
            short type = (flags & LockFileExclusiveLock) ? F_WRLCK : F_RDLCK;
            return lockRange(handle, type, !(flags & LockFileFailImmediately), lengthLow,
                             lengthHigh, overlapped);
        }

        BOOL MORE_WINAPI kernel32_UnlockFileEx(HANDLE handle, DWORD reserved, DWORD lengthLow,
                                               DWORD lengthHigh, OVERLAPPED32 *overlapped) {
            return lockRange(handle, F_UNLCK, false, lengthLow, lengthHigh, overlapped);
        }

    }

    void registerKernel32File(ExportRegistry &registry) {
        MORE_REGISTER(registry, kernel32, FindFirstFileA);
        MORE_REGISTER(registry, kernel32, FindFirstFileW);
        MORE_REGISTER(registry, kernel32, FindNextFileW);
        MORE_REGISTER(registry, kernel32, FindClose);
        MORE_REGISTER(registry, kernel32, GetCurrentDirectoryA);
        MORE_REGISTER(registry, kernel32, GetFullPathNameA);
        MORE_REGISTER(registry, kernel32, GetDriveTypeA);
        MORE_REGISTER(registry, kernel32, LockFileEx);
        MORE_REGISTER(registry, kernel32, UnlockFileEx);
    }

}
