#include "WinAPI_p.h"

#include <cstring>
#include <string>

#include <moreloader/Runtime/GuestThread.h>
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
                setLastError(ErrorInsufficientBuffer);
                return 0;
            }
            std::memcpy(output, result.text.data(), required);
            return std::int32_t(required);
        }

        /// Reports no lead bytes, because the narrow code page of the guest is UTF-8, which has
        /// no double-byte characters.
        BOOL MORE_WINAPI kernel32_IsDBCSLeadByteEx(DWORD codePage, std::uint8_t byte) {
            return FALSE;
        }

    }

    void registerKernel32String(ExportRegistry &registry) {
        MORE_REGISTER(registry, kernel32, MultiByteToWideChar);
        MORE_REGISTER(registry, kernel32, WideCharToMultiByte);
        MORE_REGISTER(registry, kernel32, IsDBCSLeadByteEx);
    }

}
