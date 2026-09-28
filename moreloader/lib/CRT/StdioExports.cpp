#include "CRTExports_p.h"

#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <memory>

#include <moreloader/Runtime/KernelObjects.h>
#include <moreloader/Support/CodePage.h>
#include <moreloader/Support/PathMapping.h>

namespace more::loader::msvcrt {

    MsvcrtFILE msvcrt__iob[iobCount] = {};
    int msvcrt__fmode = 0;

    namespace {

        MsvcrtFILE s_extraFiles[streamCount - iobCount] = {};
        Stream s_streams[streamCount];
        std::mutex s_allocationMutex;

        // The substitute character, which ends the input of a text stream on Windows.
        constexpr int substituteCharacter = 0x1A;

        /// A mode string of fopen, parsed.
        struct OpenMode {
            char base = 'r';
            bool update = false;
            bool binary = false;
        };

        // Parses a mode of fopen or _wfopen with the rules measured for msvcrt.dll: the first
        // character is r, w or a; then +, b, t, c, n, N, S, R, T and D are accepted. In a
        // narrow mode a w after an initial r is ignored, which makes "rw" open for reading,
        // while _wfopen rejects the same mode with EINVAL. Any other character is rejected.
        template <class Char>
        std::optional<OpenMode> parseMode(const Char *mode, bool wide) {
            OpenMode result;
            if (*mode != Char('r') && *mode != Char('w') && *mode != Char('a')) {
                return std::nullopt;
            }
            result.base = char(*mode);
            bool explicitMode = false;
            for (const Char *p = mode + 1; *p; ++p) {
                switch (*p) {
                    case Char('+'):
                        if (result.update) {
                            return std::nullopt;
                        }
                        result.update = true;
                        break;
                    case Char('b'):
                    case Char('t'):
                        if (explicitMode) {
                            return std::nullopt;
                        }
                        explicitMode = true;
                        result.binary = *p == Char('b');
                        break;
                    case Char('c'):
                    case Char('n'):
                    case Char('N'):
                    case Char('S'):
                    case Char('R'):
                    case Char('T'):
                    case Char('D'):
                        break;
                    case Char('w'):
                        if (wide || result.base != 'r') {
                            return std::nullopt;
                        }
                        break;
                    default:
                        return std::nullopt;
                }
            }
            if (!explicitMode) {
                result.binary = (msvcrt__fmode & textModeBinary) != 0;
            }
            return result;
        }

        const char *hostModeOf(const OpenMode &mode) {
            switch (mode.base) {
                case 'r':
                    return mode.update ? "r+b" : "rb";
                case 'w':
                    return mode.update ? "w+b" : "wb";
                default:
                    return mode.update ? "a+b" : "ab";
            }
        }

        std::optional<int> allocateIndex() {
            std::lock_guard<std::mutex> lock(s_allocationMutex);
            for (int i = 3; i < streamCount; ++i) {
                if (!s_streams[i].open) {
                    s_streams[i].open = true;
                    return i;
                }
            }
            return std::nullopt;
        }

        // Opens the host file of \a guestPath into the stream \a index, which is reserved.
        bool openInto(int index, const std::string &guestPath, const OpenMode &mode) {
            std::string hostPath = hostPathFromGuest(guestPath);
            std::FILE *host = std::fopen(hostPath.c_str(), hostModeOf(mode));
            if (!host) {
                setErrnoFromHost(errno);
                return false;
            }
            Stream &stream = s_streams[index];
            stream.host = host;
            stream.open = true;
            stream.text = !mode.binary;
            stream.standard = false;
            stream.readable = mode.base == 'r' || mode.update;
            stream.writable = mode.base != 'r' || mode.update;
            stream.endOfFile = false;
            stream.error = false;
            stream.controlZ = false;
            stream.osHandle = 0;
            MsvcrtFILE *file = fileAt(index);
            std::memset(file, 0, sizeof *file);
            file->_file = index;
            updateFlags(index);
            return true;
        }

        void release(int index) {
            std::lock_guard<std::mutex> lock(s_allocationMutex);
            Stream &stream = s_streams[index];
            stream.open = false;
            stream.host = nullptr;
            if (stream.osHandle) {
                handleTable().close(stream.osHandle);
                stream.osHandle = 0;
            }
            std::memset(fileAt(index), 0, sizeof(MsvcrtFILE));
        }

        MsvcrtFILE *openFile(const std::string &guestPath, const std::optional<OpenMode> &mode) {
            if (!mode) {
                threadErrno() = ErrnoInvalid;
                return nullptr;
            }
            auto index = allocateIndex();
            if (!index) {
                threadErrno() = ErrnoTooManyFiles;
                return nullptr;
            }
            if (!openInto(*index, guestPath, *mode)) {
                release(*index);
                return nullptr;
            }
            return fileAt(*index);
        }

        // -----------------------------------------------------------------------------------

        MsvcrtFILE *MORE_CDECL msvcrt_fopen(const char *path, const char *mode) {
            if (!path || !mode) {
                threadErrno() = ErrnoInvalid;
                return nullptr;
            }
            return openFile(path, parseMode(mode, false));
        }

        /// Opens a file by a wide path. Measured: the mode "rw", which moresampler passes at
        /// 0x40d5b5, is rejected with EINVAL, whereas fopen accepts it.
        MsvcrtFILE *MORE_CDECL msvcrt__wfopen(const char16_t *path, const char16_t *mode) {
            if (!path || !mode) {
                threadErrno() = ErrnoInvalid;
                return nullptr;
            }
            return openFile(wideToMultiByte(path).text, parseMode(mode, true));
        }

        MsvcrtFILE *MORE_CDECL msvcrt_freopen(const char *path, const char *mode,
                                              MsvcrtFILE *file) {
            auto index = indexOf(file);
            if (!index || !path || !mode) {
                threadErrno() = ErrnoInvalid;
                return nullptr;
            }
            auto parsed = parseMode(mode, false);
            if (!parsed) {
                threadErrno() = ErrnoInvalid;
                return nullptr;
            }
            Stream &stream = s_streams[*index];
            std::lock_guard<std::recursive_mutex> lock(stream.mutex);
            if (stream.open && stream.host) {
                std::fflush(stream.host);
                if (!stream.standard) {
                    std::fclose(stream.host);
                }
            }
            stream.open = true;
            if (!openInto(*index, path, *parsed)) {
                release(*index);
                return nullptr;
            }
            return file;
        }

        std::int32_t MORE_CDECL msvcrt_fclose(MsvcrtFILE *file) {
            auto index = indexOf(file);
            if (!index || !s_streams[*index].open) {
                threadErrno() = ErrnoInvalid;
                return EOF;
            }
            Stream &stream = s_streams[*index];
            int result;
            {
                std::lock_guard<std::recursive_mutex> lock(stream.mutex);
                result = stream.standard ? std::fflush(stream.host) : std::fclose(stream.host);
            }
            release(*index);
            return result == 0 ? 0 : EOF;
        }

        std::int32_t MORE_CDECL msvcrt_fflush(MsvcrtFILE *file) {
            if (!file) {
                flushAllStreams();
                return 0;
            }
            Stream *stream = openStream(file);
            if (!stream) {
                return EOF;
            }
            std::lock_guard<std::recursive_mutex> lock(stream->mutex);
            return std::fflush(stream->host) == 0 ? 0 : EOF;
        }

        std::int32_t MORE_CDECL msvcrt_feof(MsvcrtFILE *file) {
            Stream *stream = openStream(file);
            return stream && stream->endOfFile ? StreamEndOfFile : 0;
        }

        std::int32_t MORE_CDECL msvcrt_ferror(MsvcrtFILE *file) {
            Stream *stream = openStream(file);
            return stream && stream->error ? StreamError : 0;
        }

        std::int32_t MORE_CDECL msvcrt_fseek(MsvcrtFILE *file, std::int32_t offset,
                                            std::int32_t origin) {
            Stream *stream = openStream(file);
            if (!stream || origin < 0 || origin > 2) {
                threadErrno() = ErrnoInvalid;
                return -1;
            }
            std::lock_guard<std::recursive_mutex> lock(stream->mutex);
            if (std::fseek(stream->host, offset, origin) != 0) {
                setErrnoFromHost(errno);
                return -1;
            }
            stream->endOfFile = false;
            stream->controlZ = false;
            updateFlags(*indexOf(file));
            return 0;
        }

        /// Returns the position of the host stream. In text mode msvcrt returns a position that
        /// differs from the byte offset after CR characters (measured), which is not reproduced.
        std::int32_t MORE_CDECL msvcrt_ftell(MsvcrtFILE *file) {
            Stream *stream = openStream(file);
            if (!stream) {
                return -1;
            }
            std::lock_guard<std::recursive_mutex> lock(stream->mutex);
            long position = std::ftell(stream->host);
            if (position < 0) {
                setErrnoFromHost(errno);
                return -1;
            }
            return std::int32_t(position);
        }

        std::int32_t MORE_CDECL msvcrt_fgetc(MsvcrtFILE *file) {
            Stream *stream = openStream(file);
            if (!stream) {
                return EOF;
            }
            std::lock_guard<std::recursive_mutex> lock(stream->mutex);
            int c = stream->getChar();
            updateFlags(*indexOf(file));
            return c;
        }

        std::int32_t MORE_CDECL msvcrt_getc(MsvcrtFILE *file) {
            return msvcrt_fgetc(file);
        }

        std::int32_t MORE_CDECL msvcrt_getchar() {
            return msvcrt_fgetc(&msvcrt__iob[0]);
        }

        char *MORE_CDECL msvcrt_fgets(char *buffer, std::int32_t size, MsvcrtFILE *file) {
            Stream *stream = openStream(file);
            if (!stream || !buffer || size <= 0) {
                return nullptr;
            }
            std::lock_guard<std::recursive_mutex> lock(stream->mutex);
            std::int32_t n = 0;
            while (n < size - 1) {
                int c = stream->getChar();
                if (c == EOF) {
                    break;
                }
                buffer[n++] = char(c);
                if (c == '\n') {
                    break;
                }
            }
            updateFlags(*indexOf(file));
            if (n == 0) {
                return nullptr;
            }
            buffer[n] = 0;
            return buffer;
        }

        std::uint32_t MORE_CDECL msvcrt_fread(void *buffer, std::uint32_t size,
                                             std::uint32_t count, MsvcrtFILE *file) {
            Stream *stream = openStream(file);
            if (!stream || size == 0 || count == 0) {
                return 0;
            }
            std::lock_guard<std::recursive_mutex> lock(stream->mutex);
            std::size_t total = std::size_t(size) * count;
            std::size_t read = 0;
            auto out = static_cast<char *>(buffer);
            if (!stream->text || stream->standard) {
                if (!stream->readable) {
                    stream->error = true;
                } else {
                    read = std::fread(out, 1, total, stream->host);
                    if (read < total) {
                        if (std::ferror(stream->host)) {
                            stream->error = true;
                        } else {
                            stream->endOfFile = true;
                        }
                    }
                }
            } else {
                while (read < total) {
                    int c = stream->getChar();
                    if (c == EOF) {
                        break;
                    }
                    out[read++] = char(c);
                }
            }
            updateFlags(*indexOf(file));
            return std::uint32_t(read / size);
        }

        std::uint32_t MORE_CDECL msvcrt_fwrite(const void *buffer, std::uint32_t size,
                                              std::uint32_t count, MsvcrtFILE *file) {
            Stream *stream = openStream(file);
            if (!stream || size == 0 || count == 0) {
                return 0;
            }
            std::lock_guard<std::recursive_mutex> lock(stream->mutex);
            std::size_t written =
                stream->write(static_cast<const char *>(buffer), std::size_t(size) * count);
            updateFlags(*indexOf(file));
            return std::uint32_t(written / size);
        }

        std::int32_t MORE_CDECL msvcrt_fputc(std::int32_t c, MsvcrtFILE *file) {
            Stream *stream = openStream(file);
            if (!stream) {
                return EOF;
            }
            std::lock_guard<std::recursive_mutex> lock(stream->mutex);
            bool ok = stream->putChar(c & 0xFF);
            updateFlags(*indexOf(file));
            return ok ? (c & 0xFF) : EOF;
        }

        std::int32_t MORE_CDECL msvcrt_putchar(std::int32_t c) {
            return msvcrt_fputc(c, &msvcrt__iob[1]);
        }

        std::int32_t MORE_CDECL msvcrt_fputs(const char *text, MsvcrtFILE *file) {
            return writeToFile(file, text) ? 0 : EOF;
        }

        std::int32_t MORE_CDECL msvcrt_puts(const char *text) {
            return writeToFile(&msvcrt__iob[1], std::string(text) + "\n") ? 0 : EOF;
        }

        std::int32_t MORE_CDECL msvcrt__fileno(MsvcrtFILE *file) {
            auto index = indexOf(file);
            if (!index) {
                threadErrno() = ErrnoInvalid;
                return -1;
            }
            return *index;
        }

        /// Returns a handle to the host file descriptor of the stream \a descriptor, which
        /// \c LockFileEx accepts.
        std::int32_t MORE_CDECL msvcrt__get_osfhandle(std::int32_t descriptor) {
            if (descriptor < 0 || descriptor >= streamCount || !s_streams[descriptor].open) {
                threadErrno() = ErrnoBadFile;
                return -1;
            }
            Stream &stream = s_streams[descriptor];
            std::lock_guard<std::recursive_mutex> lock(stream.mutex);
            if (!stream.osHandle) {
                stream.osHandle =
                    handleTable().insert(std::make_shared<FileObject>(::fileno(stream.host)));
            }
            return std::int32_t(stream.osHandle);
        }

    }

    // ---------------------------------------------------------------------------------------
    // Stream
    // ---------------------------------------------------------------------------------------

    int Stream::getChar() {
        if (!readable) {
            error = true;
            return EOF;
        }
        if (controlZ) {
            return EOF;
        }
        int c = std::fgetc(host);
        if (c == EOF) {
            if (std::ferror(host)) {
                error = true;
            } else {
                endOfFile = true;
            }
            return EOF;
        }
        if (text && !standard) {
            if (c == substituteCharacter) {
                this->controlZ = true;
                endOfFile = true;
                return EOF;
            }
            if (c == '\r') {
                int next = std::fgetc(host);
                if (next == '\n') {
                    return '\n';
                }
                if (next != EOF) {
                    std::ungetc(next, host);
                }
                return '\r';
            }
        }
        return c;
    }

    bool Stream::putChar(int c) {
        if (!writable) {
            error = true;
            return false;
        }
        if (text && !standard && c == '\n' && std::fputc('\r', host) == EOF) {
            error = true;
            return false;
        }
        if (std::fputc(c, host) == EOF) {
            error = true;
            return false;
        }
        return true;
    }

    std::size_t Stream::write(const char *data, std::size_t size) {
        if (!writable) {
            error = true;
            return 0;
        }
        if (!text || standard) {
            std::size_t written = std::fwrite(data, 1, size, host);
            if (written < size) {
                error = true;
            }
            return written;
        }
        for (std::size_t i = 0; i < size; ++i) {
            if (!putChar(static_cast<unsigned char>(data[i]))) {
                return i;
            }
        }
        return size;
    }

    // ---------------------------------------------------------------------------------------
    // Stream table
    // ---------------------------------------------------------------------------------------

    MsvcrtFILE *fileAt(int index) {
        return index < iobCount ? &msvcrt__iob[index] : &s_extraFiles[index - iobCount];
    }

    std::optional<int> indexOf(const MsvcrtFILE *file) {
        if (file >= msvcrt__iob && file < msvcrt__iob + iobCount) {
            return int(file - msvcrt__iob);
        }
        if (file >= s_extraFiles && file < s_extraFiles + (streamCount - iobCount)) {
            return iobCount + int(file - s_extraFiles);
        }
        return std::nullopt;
    }

    Stream &streamAt(int index) {
        return s_streams[index];
    }

    Stream *openStream(MsvcrtFILE *file) {
        auto index = indexOf(file);
        if (!index || !s_streams[*index].open) {
            threadErrno() = ErrnoInvalid;
            return nullptr;
        }
        return &s_streams[*index];
    }

    void updateFlags(int index) {
        Stream &stream = s_streams[index];
        std::int32_t flags = 0;
        if (stream.readable && stream.writable) {
            flags |= StreamReadWrite;
        } else if (stream.readable) {
            flags |= StreamRead;
        } else if (stream.writable) {
            flags |= StreamWrite;
        }
        if (stream.endOfFile) {
            flags |= StreamEndOfFile;
        }
        if (stream.error) {
            flags |= StreamError;
        }
        fileAt(index)->_flag = flags;
    }

    void flushAllStreams() {
        for (int i = 0; i < streamCount; ++i) {
            Stream &stream = s_streams[i];
            if (stream.open && stream.host) {
                std::lock_guard<std::recursive_mutex> lock(stream.mutex);
                std::fflush(stream.host);
            }
        }
    }

    void initializeStandardStreams() {
        std::FILE *hosts[3] = {stdin, stdout, stderr};
        for (int i = 0; i < 3; ++i) {
            Stream &stream = s_streams[i];
            stream.host = hosts[i];
            stream.open = true;
            stream.text = true;
            stream.standard = true;
            stream.readable = i == 0;
            stream.writable = i != 0;
            msvcrt__iob[i]._file = i;
            updateFlags(i);
        }
    }

    bool writeToFile(MsvcrtFILE *file, const std::string &text) {
        Stream *stream = openStream(file);
        if (!stream) {
            return false;
        }
        std::lock_guard<std::recursive_mutex> lock(stream->mutex);
        bool ok = stream->write(text.data(), text.size()) == text.size();
        updateFlags(*indexOf(file));
        return ok;
    }

    void registerStdioExports(ExportRegistry &registry) {
        MORE_REGISTER_DATA(registry, msvcrt, _iob);
        MORE_REGISTER_DATA(registry, msvcrt, _fmode);
        MORE_REGISTER(registry, msvcrt, fopen);
        MORE_REGISTER(registry, msvcrt, _wfopen);
        MORE_REGISTER(registry, msvcrt, freopen);
        MORE_REGISTER(registry, msvcrt, fclose);
        MORE_REGISTER(registry, msvcrt, fflush);
        MORE_REGISTER(registry, msvcrt, feof);
        MORE_REGISTER(registry, msvcrt, ferror);
        MORE_REGISTER(registry, msvcrt, fseek);
        MORE_REGISTER(registry, msvcrt, ftell);
        MORE_REGISTER(registry, msvcrt, fgetc);
        MORE_REGISTER(registry, msvcrt, getc);
        MORE_REGISTER(registry, msvcrt, getchar);
        MORE_REGISTER(registry, msvcrt, fgets);
        MORE_REGISTER(registry, msvcrt, fread);
        MORE_REGISTER(registry, msvcrt, fwrite);
        MORE_REGISTER(registry, msvcrt, fputc);
        MORE_REGISTER(registry, msvcrt, putchar);
        MORE_REGISTER(registry, msvcrt, fputs);
        MORE_REGISTER(registry, msvcrt, puts);
        MORE_REGISTER(registry, msvcrt, _fileno);
        MORE_REGISTER(registry, msvcrt, _get_osfhandle);
    }

}
