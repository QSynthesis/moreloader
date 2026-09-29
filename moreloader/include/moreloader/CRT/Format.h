#ifndef MORELOADER_CRT_FORMAT_H
#define MORELOADER_CRT_FORMAT_H

#include <cstdint>
#include <string>

namespace more::loader::msvcrt {

    /// Reads the variable arguments of a guest call.
    ///
    /// The arguments are laid out as the i386 calling convention places them on the stack: each
    /// integer and pointer in 4 bytes, each 64-bit integer and double in 8 bytes, without further
    /// alignment. A \c va_list of the guest is a pointer to this area, and so is a \c va_list of
    /// the host on i386 Linux.
    class GuestArguments {
    public:
        inline explicit GuestArguments(const void *area)
            : m_next(static_cast<const std::uint8_t *>(area)) {
        }

        std::uint32_t nextUInt32();

        /// Returns the next 8 bytes, which are also the bits of a double argument.
        ///
        /// \note There is deliberately no function that returns a \c double. Loading a
        ///       signaling NaN into an x87 register converts it into a quiet NaN.
        std::uint64_t nextUInt64();

        /// Returns the address of the next unread argument.
        inline const void *position() const {
            return m_next;
        }

    private:
        const std::uint8_t *m_next;
    };

    /// Result of a formatting call.
    template <class Char>
    struct FormatResult {
        /// The characters produced, including those produced before a failure.
        std::basic_string<Char> text;

        /// Whether the call failed, in which case msvcrt returns -1. A failure occurs if a wide
        /// string argument of a narrow format contains a character above U+00FF, which the C
        /// locale cannot represent, and for \c %n, which msvcrt rejects.
        bool failed = false;
    };

    /// Formats \a format with \a arguments as \c vsprintf of msvcrt.dll does.
    ///
    /// The differences from glibc that affect the output are reproduced. Exponents have at least
    /// three digits. Floating-point conversions round the 17 significant digits of the value half
    /// up rather than rounding the exact binary value. Infinities and NaNs are written as
    /// \c 1.#INF, \c 1.#QNAN, \c 1.#SNAN and \c 1.#IND and rounded as digit strings. The
    /// conversion \c %p writes eight uppercase digits. The length modifiers \c I, \c I32, \c I64
    /// and \c w are accepted. The length modifier \c L denotes \c double. Wide arguments are
    /// converted in the C locale.
    ///
    /// \sa moreloader/tests/auto/data/msvcrt/printf.txt
    FormatResult<char> formatNarrow(const char *format, GuestArguments &arguments);

    /// Formats \a format with \a arguments as \c _vsnwprintf of msvcrt.dll does. In a wide
    /// format \c %s and \c %c denote wide arguments and \c %S and \c %C narrow arguments, and
    /// narrow arguments are widened byte by byte, as the C locale does.
    FormatResult<char16_t> formatWide(const char16_t *format, GuestArguments &arguments);

    /// Stores \a result into \a buffer of \a count elements as \c _vsnprintf and \c _vsnwprintf
    /// of msvcrt do.
    ///
    /// \return the length of the text if it fits, with a terminator only if there is room for
    ///         one, or -1 if the text is longer than \a count or the formatting failed, in which
    ///         case the first \a count elements are stored without a terminator
    ///
    /// \sa moreloader/tests/auto/data/msvcrt/snprintf.txt
    int storeTruncated(const FormatResult<char> &result, char *buffer, std::size_t count);
    int storeTruncated(const FormatResult<char16_t> &result, char16_t *buffer, std::size_t count);

}

#endif // MORELOADER_CRT_FORMAT_H
