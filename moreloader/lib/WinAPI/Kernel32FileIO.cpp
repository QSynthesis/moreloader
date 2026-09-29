#include "WinAPI_p.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <mutex>
#include <string>

#include <moreloader/Runtime/GuestThread.h>
#include <moreloader/Runtime/KernelObjects.h>
#include <moreloader/Support/Diagnostics.h>
#include <moreloader/Support/PathMapping.h>

namespace more::loader::winapi {

    namespace {

        enum AccessRight : DWORD {
            GenericWrite = 0x40000000,
            GenericRead = 0x80000000,
        };

        enum CreationDisposition : DWORD {
            CreateNew = 1,
            CreateAlways = 2,
            OpenExisting = 3,
            OpenAlways = 4,
            TruncateExisting = 5,
        };

        enum FileFlag : DWORD {
            FileAttributeReadOnly = 0x1,
            FileFlagDeleteOnClose = 0x04000000,
        };

        enum FileType : DWORD {
            FileTypeUnknown = 0,
            FileTypeDisk = 1,
            FileTypeChar = 2,
            FileTypePipe = 3,
        };

        enum MoveMethod : DWORD {
            FileBegin = 0,
            FileCurrent = 1,
            FileEnd = 2,
        };

        // The arguments of GetStdHandle.
        enum StandardDevice : DWORD {
            StdInputHandle = 0xFFFFFFF6,  // -10
            StdOutputHandle = 0xFFFFFFF5, // -11
            StdErrorHandle = 0xFFFFFFF4,  // -12
        };

        constexpr DWORD invalidSetFilePointer = 0xFFFFFFFF;

        std::mutex s_standardMutex;
        HANDLE s_standardHandles[3] = {};

        std::shared_ptr<FileObject> fileOf(HANDLE handle) {
            return handleTable().get<FileObject>(handle);
        }

        // -----------------------------------------------------------------------------------

        /// Opens a file by its narrow name, which is UTF-8 as the ANSI code page of the guest.
        ///
        /// Share modes are not enforced, because Linux has no mandatory sharing. A file that
        /// \c CREATE_ALWAYS or \c OPEN_ALWAYS finds existing is opened with the last error
        /// \c ERROR_ALREADY_EXISTS, otherwise the last error is 0, as Windows documents.
        HANDLE MORE_WINAPI kernel32_CreateFileA(const char *name, DWORD access, DWORD shareMode,
                                                void *security, DWORD disposition, DWORD flags,
                                                HANDLE templateFile) {
            if (!name) {
                setLastError(ErrorInvalidParameter);
                return INVALID_HANDLE_VALUE;
            }
            int mode = O_CLOEXEC;
            if ((access & GenericRead) && (access & GenericWrite)) {
                mode |= O_RDWR;
            } else if (access & GenericWrite) {
                mode |= O_WRONLY;
            } else {
                mode |= O_RDONLY;
            }
            switch (disposition) {
                case CreateNew:
                    mode |= O_CREAT | O_EXCL;
                    break;
                case CreateAlways:
                    mode |= O_CREAT | O_TRUNC;
                    break;
                case OpenExisting:
                    break;
                case OpenAlways:
                    mode |= O_CREAT;
                    break;
                case TruncateExisting:
                    mode |= O_TRUNC;
                    break;
                default:
                    setLastError(ErrorInvalidParameter);
                    return INVALID_HANDLE_VALUE;
            }
            if (flags & FileFlagDeleteOnClose) {
                diagnostic("CreateFileA(\"%s\"): FILE_FLAG_DELETE_ON_CLOSE is not supported", name);
                setLastError(ErrorNotSupported);
                return INVALID_HANDLE_VALUE;
            }

            std::string path = hostPathFromGuest(name);
            bool existed = false;
            if (disposition == CreateAlways || disposition == OpenAlways) {
                struct stat info;
                existed = ::stat(path.c_str(), &info) == 0;
            }
            // A file created with FILE_ATTRIBUTE_READONLY cannot be opened for writing later.
            mode_t permissions = (flags & FileAttributeReadOnly) ? 0444 : 0666;
            int descriptor = ::open(path.c_str(), mode, permissions);
            if (descriptor < 0) {
                setLastError(systemErrorOf(errno));
                return INVALID_HANDLE_VALUE;
            }
            // Windows opens a directory only with FILE_FLAG_BACKUP_SEMANTICS, which the guest
            // does not use.
            struct stat info;
            if (::fstat(descriptor, &info) == 0 && S_ISDIR(info.st_mode)) {
                ::close(descriptor);
                setLastError(ErrorAccessDenied);
                return INVALID_HANDLE_VALUE;
            }
            setLastError(existed ? ErrorAlreadyExists : ErrorSuccess);
            return handleTable().insert(std::make_shared<FileObject>(descriptor, true));
        }

        /// Reads up to \a count bytes. The end of a file is a successful read of 0 bytes.
        BOOL MORE_WINAPI kernel32_ReadFile(HANDLE handle, void *buffer, DWORD count, DWORD *read,
                                           OVERLAPPED32 *overlapped) {
            auto file = fileOf(handle);
            if (read) {
                *read = 0;
            }
            if (!file) {
                setLastError(ErrorInvalidHandle);
                return FALSE;
            }
            if (overlapped) {
                diagnostic("ReadFile with an OVERLAPPED structure is not supported");
                setLastError(ErrorNotSupported);
                return FALSE;
            }
            ssize_t result;
            do {
                result = ::read(file->descriptor(), buffer, count);
            } while (result < 0 && errno == EINTR);
            if (result < 0) {
                setLastError(systemErrorOf(errno));
                return FALSE;
            }
            if (read) {
                *read = DWORD(result);
            }
            return TRUE;
        }

        /// Writes all \a count bytes unless an error occurs.
        BOOL MORE_WINAPI kernel32_WriteFile(HANDLE handle, const void *buffer, DWORD count,
                                            DWORD *written, OVERLAPPED32 *overlapped) {
            auto file = fileOf(handle);
            if (written) {
                *written = 0;
            }
            if (!file) {
                setLastError(ErrorInvalidHandle);
                return FALSE;
            }
            if (overlapped) {
                diagnostic("WriteFile with an OVERLAPPED structure is not supported");
                setLastError(ErrorNotSupported);
                return FALSE;
            }
            auto bytes = static_cast<const char *>(buffer);
            DWORD done = 0;
            while (done < count) {
                ssize_t result = ::write(file->descriptor(), bytes + done, count - done);
                if (result < 0) {
                    if (errno == EINTR) {
                        continue;
                    }
                    if (written) {
                        *written = done;
                    }
                    setLastError(systemErrorOf(errno));
                    return FALSE;
                }
                done += DWORD(result);
            }
            if (written) {
                *written = done;
            }
            return TRUE;
        }

        /// Moves the file pointer. Without \a distanceHigh the distance is a signed 32-bit value
        /// and the result must fit in 32 bits. A position before the start of the file fails
        /// with \c ERROR_NEGATIVE_SEEK.
        DWORD MORE_WINAPI kernel32_SetFilePointer(HANDLE handle, std::int32_t distance,
                                                  std::int32_t *distanceHigh, DWORD method) {
            auto file = fileOf(handle);
            if (!file) {
                setLastError(ErrorInvalidHandle);
                return invalidSetFilePointer;
            }
            std::int64_t offset =
                distanceHigh ? std::int64_t((std::uint64_t(std::uint32_t(*distanceHigh)) << 32) |
                                            std::uint32_t(distance))
                             : std::int64_t(distance);
            int whence;
            switch (method) {
                case FileBegin:
                    whence = SEEK_SET;
                    break;
                case FileCurrent:
                    whence = SEEK_CUR;
                    break;
                case FileEnd:
                    whence = SEEK_END;
                    break;
                default:
                    setLastError(ErrorInvalidParameter);
                    return invalidSetFilePointer;
            }
            off64_t current = ::lseek64(file->descriptor(), 0, SEEK_CUR);
            off64_t end = ::lseek64(file->descriptor(), 0, SEEK_END);
            if (current < 0 || end < 0) {
                setLastError(systemErrorOf(errno));
                return invalidSetFilePointer;
            }
            std::int64_t base = whence == SEEK_SET ? 0 : (whence == SEEK_CUR ? current : end);
            std::int64_t target = base + offset;
            if (target < 0 || (!distanceHigh && target > 0xFFFFFFFEll)) {
                ::lseek64(file->descriptor(), current, SEEK_SET);
                setLastError(target < 0 ? ErrorNegativeSeek : ErrorInvalidParameter);
                return invalidSetFilePointer;
            }
            if (::lseek64(file->descriptor(), target, SEEK_SET) < 0) {
                setLastError(systemErrorOf(errno));
                return invalidSetFilePointer;
            }
            if (distanceHigh) {
                *distanceHigh = std::int32_t(std::uint64_t(target) >> 32);
                // A low part of 0xFFFFFFFF is a valid result, which the last error of 0
                // distinguishes from a failure.
                setLastError(ErrorSuccess);
            }
            return DWORD(std::uint64_t(target));
        }

        /// Truncates or extends the file to the current file pointer.
        BOOL MORE_WINAPI kernel32_SetEndOfFile(HANDLE handle) {
            auto file = fileOf(handle);
            if (!file) {
                setLastError(ErrorInvalidHandle);
                return FALSE;
            }
            off64_t current = ::lseek64(file->descriptor(), 0, SEEK_CUR);
            if (current < 0 || ::ftruncate64(file->descriptor(), current) != 0) {
                setLastError(systemErrorOf(errno));
                return FALSE;
            }
            return TRUE;
        }

        /// Succeeds without further action, because WriteFile passes all data to the host
        /// kernel at once, which makes it visible to every reader as Windows does after the
        /// flush.
        BOOL MORE_WINAPI kernel32_FlushFileBuffers(HANDLE handle) {
            if (!fileOf(handle)) {
                setLastError(ErrorInvalidHandle);
                return FALSE;
            }
            return TRUE;
        }

        /// Returns handles of the host descriptors 0, 1 and 2. The handles are created on first
        /// use and are not closed by the process.
        HANDLE MORE_WINAPI kernel32_GetStdHandle(DWORD device) {
            int descriptor;
            switch (device) {
                case StdInputHandle:
                    descriptor = 0;
                    break;
                case StdOutputHandle:
                    descriptor = 1;
                    break;
                case StdErrorHandle:
                    descriptor = 2;
                    break;
                default:
                    setLastError(ErrorInvalidHandle);
                    return INVALID_HANDLE_VALUE;
            }
            std::lock_guard<std::mutex> lock(s_standardMutex);
            if (!s_standardHandles[descriptor]) {
                s_standardHandles[descriptor] =
                    handleTable().insert(std::make_shared<FileObject>(descriptor));
            }
            return s_standardHandles[descriptor];
        }

        /// Classifies a handle as Windows does: regular files are disk files, character
        /// devices including \c /dev/null are character files like the console and \c NUL,
        /// and pipes and sockets are pipes.
        DWORD MORE_WINAPI kernel32_GetFileType(HANDLE handle) {
            auto file = fileOf(handle);
            struct stat info;
            if (!file || ::fstat(file->descriptor(), &info) != 0) {
                setLastError(ErrorInvalidHandle);
                return FileTypeUnknown;
            }
            setLastError(ErrorSuccess);
            if (S_ISREG(info.st_mode)) {
                return FileTypeDisk;
            }
            if (S_ISCHR(info.st_mode)) {
                return FileTypeChar;
            }
            if (S_ISFIFO(info.st_mode) || S_ISSOCK(info.st_mode)) {
                return FileTypePipe;
            }
            return FileTypeUnknown;
        }

        /// Returns \a count. Windows NT has no limit on the number of handles, and the
        /// function has no effect there.
        DWORD MORE_WINAPI kernel32_SetHandleCount(DWORD count) {
            return count;
        }

    }

    void registerKernel32FileIO(ExportRegistry &registry) {
        MORE_REGISTER(registry, kernel32, CreateFileA);
        MORE_REGISTER(registry, kernel32, ReadFile);
        MORE_REGISTER(registry, kernel32, WriteFile);
        MORE_REGISTER(registry, kernel32, SetFilePointer);
        MORE_REGISTER(registry, kernel32, SetEndOfFile);
        MORE_REGISTER(registry, kernel32, FlushFileBuffers);
        MORE_REGISTER(registry, kernel32, GetStdHandle);
        MORE_REGISTER(registry, kernel32, GetFileType);
        MORE_REGISTER(registry, kernel32, SetHandleCount);
    }

}
