#ifndef MORELOADER_CRT_CRTEXPORTS_P_H
#define MORELOADER_CRT_CRTEXPORTS_P_H

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <mutex>
#include <optional>
#include <string>

#include <moreloader/Runtime/ExportRegistry.h>
#include <moreloader/Support/MoreLoaderSupportGlobal.h>

namespace more::loader::msvcrt {

    /// errno values of msvcrt that the wrappers set. The numbers from 1 to 34 agree with Linux,
    /// but the names are distinct from the macros of the host.
    enum ErrorNumber : int {
        ErrnoNoEntry = 2,
        ErrnoBadFile = 9,
        ErrnoAgain = 11,
        ErrnoNoMemory = 12,
        ErrnoAccess = 13,
        ErrnoInvalid = 22,
        ErrnoTooManyFiles = 24,
        ErrnoRange = 34,
    };

    /// Returns the errno variable of the calling thread, whose address \c _errno returns.
    int &threadErrno();

    /// Sets the errno of the calling thread from the host \c errno value \a hostError.
    void setErrnoFromHost(int hostError);

    /// \c FILE of msvcrt, whose layout the guest relies on through \c _iob.
    struct MsvcrtFILE {
        std::uint32_t _ptr;
        std::int32_t _cnt;
        std::uint32_t _base;
        std::int32_t _flag;
        std::int32_t _file;
        std::int32_t _charbuf;
        std::int32_t _bufsiz;
        std::uint32_t _tmpfname;
    };
    static_assert(sizeof(MsvcrtFILE) == 32);

    /// Flags of \c MsvcrtFILE::_flag.
    enum StreamFlag : std::int32_t {
        StreamRead = 0x1,
        StreamWrite = 0x2,
        StreamEndOfFile = 0x10,
        StreamError = 0x20,
        StreamReadWrite = 0x80,
    };

    /// Number of streams, which is the default of msvcrt. The first 20 are \c _iob.
    constexpr int streamCount = 512;
    constexpr int iobCount = 20;

    /// The state of one stream of the guest, backed by a stream of the host that is opened in
    /// binary mode. The translation of text mode is performed here.
    ///
    /// The standard streams are not translated, so that the output of the guest follows the
    /// conventions of the host terminal. Files opened by the guest are translated as msvcrt
    /// translates them.
    struct Stream {
        std::recursive_mutex mutex;
        std::FILE *host = nullptr;
        bool open = false;
        bool text = true;
        bool standard = false;
        bool readable = false;
        bool writable = false;
        bool endOfFile = false;
        bool error = false;

        // A byte 0x1A ends the input of a text stream until the next seek.
        bool controlZ = false;

        // The handle that _get_osfhandle returned, 0 if none.
        std::uint32_t osHandle = 0;

        /// Reads one character, translating CR LF to LF in text mode.
        ///
        /// \return the character, or EOF at the end or on an error, which set the flags
        int getChar();

        /// Writes one character, translating LF to CR LF in text mode.
        ///
        /// \return whether the character was written
        bool putChar(int c);

        /// Writes \a size bytes with translation.
        ///
        /// \return the number of bytes of \a data written
        std::size_t write(const char *data, std::size_t size);
    };

    /// Returns the address of the \c FILE with index \a index.
    MsvcrtFILE *fileAt(int index);

    /// Returns the index of the \c FILE at \a file, or \c std::nullopt if \a file is not a
    /// stream of the guest.
    std::optional<int> indexOf(const MsvcrtFILE *file);

    /// Returns the stream with index \a index.
    Stream &streamAt(int index);

    /// Returns the open stream of \a file, or \c nullptr, in which case errno is set to
    /// \c EINVAL.
    Stream *openStream(MsvcrtFILE *file);

    /// Copies the flags of the stream with index \a index into its \c FILE.
    void updateFlags(int index);

    /// Flushes every open stream, as the exit of msvcrt does.
    void flushAllStreams();

    /// Connects the standard streams to those of the host.
    void initializeStandardStreams();

    /// Writes \a text to the stream of \a file with the locking and the translation of the
    /// printf family.
    ///
    /// \return whether all bytes were written
    bool writeToFile(MsvcrtFILE *file, const std::string &text);

    /// The default translation mode, the data export \c _fmode. \c _O_BINARY selects binary.
    extern int msvcrt__fmode;
    constexpr int textModeBinary = 0x8000;

    /// \c _iob.
    extern MsvcrtFILE msvcrt__iob[iobCount];

    // Registration of the wrappers of each source file.
    void registerStartupExports(ExportRegistry &registry);
    void registerStdioExports(ExportRegistry &registry);
    void registerFormatExports(ExportRegistry &registry);
    void registerStringExports(ExportRegistry &registry);
    void registerWideStringExports(ExportRegistry &registry);
    void registerMathExports(ExportRegistry &registry);
    void registerTimeExports(ExportRegistry &registry);
    void registerEnvironmentExports(ExportRegistry &registry);
    void registerThreadExports(ExportRegistry &registry);
    void registerSetJmpExports(ExportRegistry &registry);

    /// Initializes the data exports that depend on the process, called before guest code runs.
    void initializeStartupData();

}

#endif // MORELOADER_CRT_CRTEXPORTS_P_H
