#ifndef MORELOADER_CRT_NUMBERS_H
#define MORELOADER_CRT_NUMBERS_H

#include <cstddef>
#include <cstdint>

namespace more::loader::msvcrt {

    /// Result of an integer conversion of the \c strtol family.
    struct IntegerParse {
        std::uint32_t value = 0;

        /// Number of characters consumed. Zero if no digit was read, including for \c 0x
        /// without hexadecimal digits, where glibc consumes the \c 0.
        std::size_t consumed = 0;

        /// Whether the value was out of range, for which msvcrt sets \c errno to \c ERANGE.
        bool overflow = false;
    };

    /// Converts \a text as \c strtol of msvcrt does. Out-of-range values are clamped to
    /// \c LONG_MIN and \c LONG_MAX.
    IntegerParse strtol(const char *text, int base);
    IntegerParse strtol(const char16_t *text, int base);

    /// Converts \a text as \c strtoul of msvcrt does. An out-of-range value becomes
    /// \c ULONG_MAX, and a leading minus sign negates the result even then, which yields 1.
    ///
    /// \sa moreloader/tests/auto/data/msvcrt/strtol.txt
    IntegerParse strtoul(const char *text, int base);

    /// Returns the bits of the double that \c atof of msvcrt returns for \a text.
    ///
    /// The syntax is that of msvcrt: optional white space and sign, decimal digits with an
    /// optional decimal point, and an optional exponent. Hexadecimal numbers, infinities and
    /// NaNs are not recognized. The digits are converted with correct rounding.
    ///
    /// \note Measured deviations of msvcrt that are not reproduced: a value given with more
    ///       than 17 significant digits that lies within a few units of the 21st digit of a
    ///       halfway point, and the rounding of values near half the smallest denormal. See the
    ///       exclusions in test_Numbers.cpp.
    std::uint64_t atof(const char *text);

}

#endif // MORELOADER_CRT_NUMBERS_H
