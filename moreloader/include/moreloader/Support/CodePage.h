#ifndef MORELOADER_SUPPORT_CODEPAGE_H
#define MORELOADER_SUPPORT_CODEPAGE_H

#include <string>
#include <string_view>

namespace more::loader {

    /// Result of a conversion that reports invalid input.
    template <class String>
    struct Conversion {
        String text;

        /// Whether the input contained an invalid sequence, which \a text replaces with U+FFFD.
        bool invalid = false;
    };

    /// Converts UTF-8 to UTF-16 as \c MultiByteToWideChar does for \c CP_UTF8.
    ///
    /// The guest treats every narrow string as UTF-8, including those that Windows would
    /// interpret in the ANSI code page. The substitution of invalid sequences differs from the
    /// maximal subparts of the Unicode Standard: if the second byte is a continuation byte
    /// outside the range that the lead byte permits, which rejects overlong forms, surrogates
    /// and code points above U+10FFFF, the lead byte and that byte are replaced together by one
    /// U+FFFD.
    ///
    /// \sa moreloader/tests/auto/data/msvcrt/utf8.txt
    Conversion<std::u16string> multiByteToWide(std::string_view text);

    /// Converts UTF-16 to UTF-8 as \c WideCharToMultiByte does for \c CP_UTF8. Each unpaired
    /// surrogate is replaced with U+FFFD.
    Conversion<std::string> wideToMultiByte(std::u16string_view text);

    /// Returns the length of the longest prefix of the UTF-8 text \a text that has at most
    /// \a limit bytes and ends at a character boundary.
    ///
    /// \c WideCharToMultiByte fills a buffer that is too small with this prefix before it
    /// fails, as measured. The C runtime of Visual C++ 6 relies on the prefix when it builds its
    /// case mapping tables for UTF-8.
    ///
    /// \sa moreloader/tests/auto/data/msvcrt/nls.txt
    std::size_t wholeCharacterPrefix(std::string_view text, std::size_t limit);

}

#endif // MORELOADER_SUPPORT_CODEPAGE_H
