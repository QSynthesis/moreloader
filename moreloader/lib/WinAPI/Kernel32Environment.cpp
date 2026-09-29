#include "WinAPI_p.h"

#include <cstdlib>
#include <cstring>
#include <string>

#include <moreloader/Runtime/GuestThread.h>
#include <moreloader/Support/CodePage.h>

extern char **environ;

namespace more::loader::winapi {

    namespace {

        char upcase(char c) {
            return c >= 'a' && c <= 'z' ? char(c - 'a' + 'A') : c;
        }

        // Returns the value of the host environment variable \a name, whose letter case is
        // ignored as Windows ignores it, or nullptr if the variable is not set.
        const char *findVariable(const char *name) {
            std::size_t length = std::strlen(name);
            for (char **entry = environ; entry && *entry; ++entry) {
                const char *text = *entry;
                std::size_t i = 0;
                while (i < length && text[i] && upcase(text[i]) == upcase(name[i])) {
                    ++i;
                }
                if (i == length && text[i] == '=') {
                    return text + length + 1;
                }
            }
            return nullptr;
        }

        // Returns the environment block in UTF-8: each variable as NAME=value with a
        // terminator, followed by an empty string.
        std::string narrowBlock() {
            std::string block;
            for (char **entry = environ; entry && *entry; ++entry) {
                block.append(*entry);
                block.push_back('\0');
            }
            block.push_back('\0');
            return block;
        }

        // -----------------------------------------------------------------------------------

        /// Returns a copy of the environment block of the host in UTF-8, the ANSI code page of
        /// the guest. The copy is released by FreeEnvironmentStringsA.
        char *MORE_WINAPI kernel32_GetEnvironmentStrings() {
            std::string block = narrowBlock();
            auto copy = static_cast<char *>(std::malloc(block.size()));
            std::memcpy(copy, block.data(), block.size());
            return copy;
        }

        /// Returns a copy of the environment block of the host in UTF-16. The copy is released
        /// by FreeEnvironmentStringsW.
        char16_t *MORE_WINAPI kernel32_GetEnvironmentStringsW() {
            std::u16string block = multiByteToWide(narrowBlock()).text;
            auto copy = static_cast<char16_t *>(std::malloc(block.size() * sizeof(char16_t)));
            std::memcpy(copy, block.data(), block.size() * sizeof(char16_t));
            return copy;
        }

        BOOL MORE_WINAPI kernel32_FreeEnvironmentStringsA(char *block) {
            std::free(block);
            return TRUE;
        }

        BOOL MORE_WINAPI kernel32_FreeEnvironmentStringsW(char16_t *block) {
            std::free(block);
            return TRUE;
        }

        /// Copies the value of a variable of the host environment.
        ///
        /// \return the length of the value, or the size including the terminator that the value
        ///         requires if \a size is insufficient, or 0 with
        ///         \c ERROR_ENVVAR_NOT_FOUND if the variable is not set
        DWORD MORE_WINAPI kernel32_GetEnvironmentVariableA(const char *name, char *buffer,
                                                           DWORD size) {
            const char *value = name ? findVariable(name) : nullptr;
            if (!value) {
                setLastError(ErrorEnvironmentVariableNotFound);
                return 0;
            }
            std::size_t length = std::strlen(value);
            if (!buffer || length >= size) {
                return DWORD(length + 1);
            }
            std::memcpy(buffer, value, length + 1);
            return DWORD(length);
        }

    }

    void registerKernel32Environment(ExportRegistry &registry) {
        MORE_REGISTER(registry, kernel32, GetEnvironmentStrings);
        MORE_REGISTER(registry, kernel32, GetEnvironmentStringsW);
        MORE_REGISTER(registry, kernel32, FreeEnvironmentStringsA);
        MORE_REGISTER(registry, kernel32, FreeEnvironmentStringsW);
        MORE_REGISTER(registry, kernel32, GetEnvironmentVariableA);
    }

}
