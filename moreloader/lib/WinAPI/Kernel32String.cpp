#include "WinAPI_p.h"

#include <cstring>
#include <string>

#include <moreloader/Runtime/GuestLayout.h>
#include <moreloader/Runtime/GuestThread.h>
#include <moreloader/Support/CharacterType.h>
#include <moreloader/Support/CodePage.h>
#include <moreloader/Support/Diagnostics.h>

namespace more::loader::winapi {

    namespace {

        enum CodePageIdentifier : DWORD {
            CodePageACP = 0,
            CodePageOEMCP = 1,
            CodePageThreadACP = 3,
            CodePageUTF8 = 65001,
        };

        constexpr DWORD multiByteErrorInvalidChars = 0x8;
        constexpr DWORD wideCharErrorInvalidChars = 0x80;

        constexpr DWORD characterType1 = 1;
        constexpr DWORD mapLowerCase = 0x100;
        constexpr DWORD mapUpperCase = 0x200;
        constexpr DWORD normIgnoreCase = 0x1;
        constexpr std::int32_t compareEqual = 2;

        // The guest treats the ANSI and OEM code pages as UTF-8. Other code pages are reported
        // and converted as UTF-8 as well.
        void checkCodePage(DWORD codePage) {
            if (codePage != CodePageACP && codePage != CodePageOEMCP &&
                codePage != CodePageThreadACP && codePage != CodePageUTF8 &&
                isDiagnosticEnabled(DiagnosticCategory::Stubs)) {
                diagnostic("code page %u is converted as UTF-8", codePage);
            }
        }

        /// Converts as \c MultiByteToWideChar does. A length of -1 includes the terminator. With
        /// an output length of zero the required length is returned. An insufficient buffer
        /// yields 0 and \c ERROR_INSUFFICIENT_BUFFER, as measured.
        std::int32_t MORE_WINAPI kernel32_MultiByteToWideChar(DWORD codePage, DWORD flags,
                                                              const char *input,
                                                              std::int32_t inputLength,
                                                              char16_t *output,
                                                              std::int32_t outputLength) {
            checkCodePage(codePage);
            if (!input || inputLength == 0 || outputLength < 0) {
                setLastError(ErrorInvalidParameter);
                return 0;
            }
            std::size_t length =
                inputLength < 0 ? std::strlen(input) + 1 : std::size_t(inputLength);
            auto result = multiByteToWide(std::string_view(input, length));
            if (result.invalid && (flags & multiByteErrorInvalidChars)) {
                setLastError(ErrorNoUnicodeTranslation);
                return 0;
            }
            std::size_t required = result.text.size();
            if (outputLength == 0) {
                return std::int32_t(required);
            }
            if (required > std::size_t(outputLength)) {
                // Windows fills the buffer before it fails, even with half of a surrogate pair,
                // as measured.
                std::memcpy(output, result.text.data(),
                            std::size_t(outputLength) * sizeof(char16_t));
                setLastError(ErrorInsufficientBuffer);
                return 0;
            }
            std::memcpy(output, result.text.data(), required * sizeof(char16_t));
            return std::int32_t(required);
        }

        std::int32_t MORE_WINAPI kernel32_WideCharToMultiByte(DWORD codePage, DWORD flags,
                                                              const char16_t *input,
                                                              std::int32_t inputLength,
                                                              char *output,
                                                              std::int32_t outputLength,
                                                              const char *defaultChar,
                                                              BOOL *usedDefaultChar) {
            checkCodePage(codePage);
            if (!input || inputLength == 0 || outputLength < 0 ||
                (codePage == CodePageUTF8 && (defaultChar || usedDefaultChar))) {
                setLastError(ErrorInvalidParameter);
                return 0;
            }
            std::size_t length = 0;
            if (inputLength < 0) {
                while (input[length] != 0) {
                    ++length;
                }
                ++length;
            } else {
                length = std::size_t(inputLength);
            }
            auto result = wideToMultiByte(std::u16string_view(input, length));
            if (result.invalid && (flags & wideCharErrorInvalidChars)) {
                setLastError(ErrorNoUnicodeTranslation);
                return 0;
            }
            if (usedDefaultChar) {
                *usedDefaultChar = FALSE;
            }
            std::size_t required = result.text.size();
            if (outputLength == 0) {
                return std::int32_t(required);
            }
            if (required > std::size_t(outputLength)) {
                // Windows writes the whole characters that fit before it fails, as measured.
                std::memcpy(output, result.text.data(),
                            wholeCharacterPrefix(result.text, std::size_t(outputLength)));
                setLastError(ErrorInsufficientBuffer);
                return 0;
            }
            std::memcpy(output, result.text.data(), required);
            return std::int32_t(required);
        }

        /// Classifies UTF-16 code units with \c CT_CTYPE1 as measured on Windows. Other
        /// information types and code units outside the measured range fail with
        /// \c ERROR_NOT_SUPPORTED and a diagnostic.
        ///
        /// The C runtime of Visual C++ 6 calls this function when it builds the tables of its
        /// multibyte functions at startup, and first with an empty string and a count of 1 to
        /// detect whether the function is available.
        BOOL MORE_WINAPI kernel32_GetStringTypeW(DWORD infoType, const char16_t *input,
                                                 std::int32_t length, std::uint16_t *types) {
            if (!input || !types || length == 0) {
                setLastError(ErrorInvalidParameter);
                return FALSE;
            }
            if (infoType != characterType1) {
                diagnostic("GetStringTypeW with the information type %u is not supported",
                           infoType);
                setLastError(ErrorNotSupported);
                return FALSE;
            }
            std::size_t count =
                length < 0 ? std::char_traits<char16_t>::length(input) + 1 : std::size_t(length);
            for (std::size_t i = 0; i < count; ++i) {
                auto type = characterType1Of(input[i]);
                if (!type) {
                    diagnostic("GetStringTypeW: the type of U+%04X is not known",
                               unsigned(input[i]));
                    setLastError(ErrorNotSupported);
                    return FALSE;
                }
                types[i] = *type;
            }
            return TRUE;
        }

        /// Maps UTF-16 code units to lower or upper case as measured on Windows. The locale is
        /// ignored, because the measured mappings do not depend on it. Other mappings and code
        /// units outside the measured range fail with \c ERROR_NOT_SUPPORTED and a diagnostic.
        ///
        /// \return the number of code units written, or required if \a outputLength is 0
        std::int32_t MORE_WINAPI kernel32_LCMapStringW(DWORD locale, DWORD flags,
                                                       const char16_t *input, std::int32_t length,
                                                       char16_t *output,
                                                       std::int32_t outputLength) {
            if (!input || length == 0 || outputLength < 0 || (outputLength > 0 && !output)) {
                setLastError(ErrorInvalidParameter);
                return 0;
            }
            if (flags != mapLowerCase && flags != mapUpperCase) {
                diagnostic("LCMapStringW with the flags 0x%x is not supported", flags);
                setLastError(ErrorNotSupported);
                return 0;
            }
            std::size_t count =
                length < 0 ? std::char_traits<char16_t>::length(input) + 1 : std::size_t(length);
            std::u16string mapped;
            for (std::size_t i = 0; i < count; ++i) {
                auto unit = flags == mapLowerCase ? lowerCaseOf(input[i]) : upperCaseOf(input[i]);
                if (!unit) {
                    diagnostic("LCMapStringW: the case mapping of U+%04X is not known",
                               unsigned(input[i]));
                    setLastError(ErrorNotSupported);
                    return 0;
                }
                mapped.push_back(*unit);
            }
            if (outputLength == 0) {
                return std::int32_t(mapped.size());
            }
            if (mapped.size() > std::size_t(outputLength)) {
                setLastError(ErrorInsufficientBuffer);
                return 0;
            }
            std::memcpy(output, mapped.data(), mapped.size() * sizeof(char16_t));
            return std::int32_t(mapped.size());
        }

        /// Reports no lead bytes, because the narrow code page of the guest is UTF-8, which has
        /// no double-byte characters.
        BOOL MORE_WINAPI kernel32_IsDBCSLeadByteEx(DWORD codePage, std::uint8_t byte) {
            return FALSE;
        }

        /// Returns UTF-8 as the ANSI code page, which Windows 10 1903 and later also permit.
        /// File names and command lines of the host are UTF-8.
        DWORD MORE_WINAPI kernel32_GetACP() {
            return CodePageUTF8;
        }

        DWORD MORE_WINAPI kernel32_GetOEMCP() {
            return CodePageUTF8;
        }

        /// Describes UTF-8 as Windows does: at most 4 bytes per character, \c ? as the default
        /// character and no lead byte ranges. Other code pages are not available.
        BOOL MORE_WINAPI kernel32_GetCPInfo(DWORD codePage, CPINFO32 *info) {
            if (codePage != CodePageACP && codePage != CodePageOEMCP &&
                codePage != CodePageThreadACP && codePage != CodePageUTF8) {
                if (isDiagnosticEnabled(DiagnosticCategory::Stubs)) {
                    diagnostic("GetCPInfo(%u) fails", codePage);
                }
                setLastError(ErrorInvalidParameter);
                return FALSE;
            }
            std::memset(info, 0, sizeof *info);
            info->MaxCharSize = 4;
            info->DefaultChar[0] = '?';
            return TRUE;
        }

        /// Compares two strings as measured on Windows. The locale is ignored, because the
        /// measured results agree for the locales 0 and 0x804. Identical strings compare equal
        /// with any flags. Otherwise only \c NORM_IGNORECASE is supported.
        ///
        /// The C runtime of Visual C++ 6 first compares two strings that consist of U+0000 with
        /// no flags to detect whether the function is available. Its getenv compares the names
        /// of variables with \c NORM_IGNORECASE and only tests for equality. If the order of two
        /// unequal strings is not determined, the function fails with \c ERROR_NOT_SUPPORTED,
        /// which that caller treats as unequal.
        ///
        /// \return \c CSTR_LESS_THAN (1), \c CSTR_EQUAL (2), \c CSTR_GREATER_THAN (3) or 0
        std::int32_t MORE_WINAPI kernel32_CompareStringW(DWORD locale, DWORD flags,
                                                         const char16_t *first,
                                                         std::int32_t firstLength,
                                                         const char16_t *second,
                                                         std::int32_t secondLength) {
            if (!first || !second) {
                setLastError(ErrorInvalidParameter);
                return 0;
            }
            auto view = [](const char16_t *text, std::int32_t length) {
                return length < 0 ? std::u16string_view(text)
                                  : std::u16string_view(text, std::size_t(length));
            };
            std::u16string_view a = view(first, firstLength);
            std::u16string_view b = view(second, secondLength);
            if (a == b) {
                return compareEqual;
            }
            if (flags != normIgnoreCase) {
                diagnostic("CompareStringW with the flags 0x%x is not supported", flags);
                setLastError(ErrorNotSupported);
                return 0;
            }
            auto result = compareIgnoringCase(a, b);
            if (!result) {
                if (isDiagnosticEnabled(DiagnosticCategory::Stubs)) {
                    diagnostic("CompareStringW: the order of two strings is not known");
                }
                setLastError(ErrorNotSupported);
                return 0;
            }
            return *result + compareEqual;
        }

        char *MORE_WINAPI kernel32_lstrcpyA(char *destination, const char *source) {
            return std::strcpy(destination, source);
        }
    }

    void registerKernel32String(ExportRegistry &registry) {
        MORE_REGISTER(registry, kernel32, MultiByteToWideChar);
        MORE_REGISTER(registry, kernel32, WideCharToMultiByte);
        MORE_REGISTER(registry, kernel32, IsDBCSLeadByteEx);
        MORE_REGISTER(registry, kernel32, GetStringTypeW);
        MORE_REGISTER(registry, kernel32, LCMapStringW);
        MORE_REGISTER(registry, kernel32, CompareStringW);
        MORE_REGISTER(registry, kernel32, GetACP);
        MORE_REGISTER(registry, kernel32, GetOEMCP);
        MORE_REGISTER(registry, kernel32, GetCPInfo);
        MORE_REGISTER(registry, kernel32, lstrcpyA);
    }

}
