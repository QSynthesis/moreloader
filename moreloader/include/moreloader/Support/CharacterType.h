#ifndef MORELOADER_SUPPORT_CHARACTERTYPE_H
#define MORELOADER_SUPPORT_CHARACTERTYPE_H

#include <cstdint>
#include <optional>
#include <string_view>

namespace more::loader {

    /// Flags of the character type \c CT_CTYPE1 that \c GetStringTypeW reports.
    enum CharacterType1 : std::uint16_t {
        C1Upper = 0x1,
        C1Lower = 0x2,
        C1Digit = 0x4,
        C1Space = 0x8,
        C1Punct = 0x10,
        C1Cntrl = 0x20,
        C1Blank = 0x40,
        C1XDigit = 0x80,
        C1Alpha = 0x100,
        C1Defined = 0x200,
    };

    /// Returns the \c CT_CTYPE1 type of \a unit as \c GetStringTypeW reports it, or
    /// \c std::nullopt if \a unit lies outside the measured range.
    ///
    /// The measured range is U+0000 to U+00FF and U+FFFD. These are the code units that the C
    /// runtime of Visual C++ 6 classifies when it builds its tables for the code page UTF-8,
    /// because it converts the bytes 0 to 255 and every byte from 0x80 becomes U+FFFD.
    ///
    /// \sa moreloader/tests/auto/data/msvcrt/nls.txt
    std::optional<std::uint16_t> characterType1Of(char16_t unit);

    /// Returns the mapping of \a unit by \c LCMapStringW with \c LCMAP_LOWERCASE, or
    /// \c std::nullopt outside the range of characterType1Of().
    std::optional<char16_t> lowerCaseOf(char16_t unit);

    /// Returns the mapping of \a unit by \c LCMapStringW with \c LCMAP_UPPERCASE, or
    /// \c std::nullopt outside the range of characterType1Of().
    std::optional<char16_t> upperCaseOf(char16_t unit);

    /// Compares \a a and \a b as \c CompareStringW does with \c NORM_IGNORECASE.
    ///
    /// Two strings compare equal if they are equal after mapping ASCII letters to one case,
    /// because Windows ignores no ASCII unit and ranks all single ASCII units apart except the
    /// two cases of a letter, as measured. The order of unequal strings follows the measured
    /// order of single units position by position. This order is an inference that holds for the
    /// primary weights of the default sort. It is applied only if neither string contains a
    /// control unit, an apostrophe or a hyphen, which Windows weights separately.
    ///
    /// \return -1, 0 or 1, or \c std::nullopt if a string contains a unit other than U+0001 to
    ///         U+007F, or if the strings are unequal and the order is not determined
    ///
    /// \sa moreloader/tests/auto/data/msvcrt/nls.txt
    std::optional<int> compareIgnoringCase(std::u16string_view a, std::u16string_view b);

}

#endif // MORELOADER_SUPPORT_CHARACTERTYPE_H
