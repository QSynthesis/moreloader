#include "CRTExports_p.h"

#include <cstdlib>
#include <cstring>

#include "CharacterClass.h"
#include "GuestHeap_p.h"
#include "Numbers.h"
#include "StringFunctions.h"

namespace more::loader::msvcrt {

    namespace {

        // Memory. The guest frees with free what these return, and every other block that the
        // guest frees comes from the same heap.

        void *MORE_CDECL msvcrt_malloc(std::uint32_t size) {
            void *p = guestAllocate(size, false, MORE_RETURN_ADDRESS());
            if (!p) {
                threadErrno() = ErrnoNoMemory;
            }
            return p;
        }

        void *MORE_CDECL msvcrt_calloc(std::uint32_t count, std::uint32_t size) {
            std::uint64_t total = std::uint64_t(count) * size;
            void *p = total > 0xFFFFFFFFu
                          ? nullptr
                          : guestAllocate(std::size_t(total), true, MORE_RETURN_ADDRESS());
            if (!p) {
                threadErrno() = ErrnoNoMemory;
            }
            return p;
        }

        void *MORE_CDECL msvcrt_realloc(void *pointer, std::uint32_t size) {
            if (size == 0) {
                // msvcrt frees the block and returns null.
                guestFree(pointer, MORE_RETURN_ADDRESS());
                return nullptr;
            }
            void *p = guestReallocate(pointer, size, MORE_RETURN_ADDRESS());
            if (!p) {
                threadErrno() = ErrnoNoMemory;
            }
            return p;
        }

        void MORE_CDECL msvcrt_free(void *pointer) {
            guestFree(pointer, MORE_RETURN_ADDRESS());
        }

        // Byte strings. The C library of the host has the same semantics for these.

        void *MORE_CDECL msvcrt_memchr(const void *s, std::int32_t c, std::uint32_t n) {
            return const_cast<void *>(std::memchr(s, c, n));
        }

        std::int32_t MORE_CDECL msvcrt_memcmp(const void *a, const void *b, std::uint32_t n) {
            return std::memcmp(a, b, n);
        }

        void *MORE_CDECL msvcrt_memcpy(void *d, const void *s, std::uint32_t n) {
            return std::memcpy(d, s, n);
        }

        void *MORE_CDECL msvcrt_memmove(void *d, const void *s, std::uint32_t n) {
            return std::memmove(d, s, n);
        }

        void *MORE_CDECL msvcrt_memset(void *d, std::int32_t c, std::uint32_t n) {
            return std::memset(d, c, n);
        }

        char *MORE_CDECL msvcrt_strchr(const char *s, std::int32_t c) {
            return const_cast<char *>(std::strchr(s, c));
        }

        std::int32_t MORE_CDECL msvcrt_strcmp(const char *a, const char *b) {
            return std::strcmp(a, b);
        }

        /// Compares as \c strcmp, because the collation of the C locale is the byte order.
        std::int32_t MORE_CDECL msvcrt_strcoll(const char *a, const char *b) {
            return std::strcmp(a, b);
        }

        char *MORE_CDECL msvcrt_strcpy(char *d, const char *s) {
            return std::strcpy(d, s);
        }

        std::uint32_t MORE_CDECL msvcrt_strlen(const char *s) {
            return std::uint32_t(std::strlen(s));
        }

        std::int32_t MORE_CDECL msvcrt_strncmp(const char *a, const char *b, std::uint32_t n) {
            return std::strncmp(a, b, n);
        }

        char *MORE_CDECL msvcrt_strncpy(char *d, const char *s, std::uint32_t n) {
            return std::strncpy(d, s, n);
        }

        char *MORE_CDECL msvcrt_strpbrk(const char *s, const char *accept) {
            return const_cast<char *>(std::strpbrk(s, accept));
        }

        std::uint32_t MORE_CDECL msvcrt_strspn(const char *s, const char *accept) {
            return std::uint32_t(std::strspn(s, accept));
        }

        char *MORE_CDECL msvcrt_strstr(const char *s, const char *needle) {
            return const_cast<char *>(std::strstr(s, needle));
        }

        char *MORE_CDECL msvcrt__strdup(const char *s) {
            if (!s) {
                return nullptr;
            }
            std::size_t size = std::strlen(s) + 1;
            auto p = static_cast<char *>(guestAllocate(size, false, MORE_RETURN_ADDRESS()));
            if (!p) {
                threadErrno() = ErrnoNoMemory;
                return nullptr;
            }
            return static_cast<char *>(std::memcpy(p, s, size));
        }

        std::int32_t MORE_CDECL msvcrt__stricmp(const char *a, const char *b) {
            return compareInsensitive(a, b, std::size_t(-1));
        }

        std::int32_t MORE_CDECL msvcrt__strnicmp(const char *a, const char *b, std::uint32_t n) {
            return compareInsensitive(a, b, n);
        }

        char *MORE_CDECL msvcrt__ultoa(std::uint32_t value, char *buffer, std::int32_t radix) {
            std::string text = ultoa(value, radix);
            std::memcpy(buffer, text.c_str(), text.size() + 1);
            return buffer;
        }

        /// Counts bytes. The narrow code page of the guest is treated as a single-byte code
        /// page, consistent with IsDBCSLeadByteEx. msvcrt counts double-byte characters of the
        /// ANSI code page of the measuring machine instead (strings.txt).
        std::uint32_t MORE_CDECL msvcrt__mbslen(const unsigned char *s) {
            return std::uint32_t(std::strlen(reinterpret_cast<const char *>(s)));
        }

        // Conversions.

        std::int32_t MORE_CDECL msvcrt_strtol(const char *text, char **end, std::int32_t base) {
            auto result = strtol(text, base);
            if (end) {
                *end = const_cast<char *>(text) + result.consumed;
            }
            if (result.overflow) {
                threadErrno() = ErrnoRange;
            }
            return std::int32_t(result.value);
        }

        std::uint32_t MORE_CDECL msvcrt_strtoul(const char *text, char **end, std::int32_t base) {
            auto result = strtoul(text, base);
            if (end) {
                *end = const_cast<char *>(text) + result.consumed;
            }
            if (result.overflow) {
                threadErrno() = ErrnoRange;
            }
            return result.value;
        }

        /// Converts as \c strtol with base 10. Not measured separately.
        std::int32_t MORE_CDECL msvcrt_atoi(const char *text) {
            return std::int32_t(strtol(text, 10).value);
        }

        /// Returns the result of the conversion. The value is loaded from its bits, so that the
        /// register ST0 holds exactly the double, as it does after msvcrt.
        double MORE_CDECL msvcrt_atof(const char *text) {
            std::uint64_t bits = atof(text);
            double value;
            std::memcpy(&value, &bits, sizeof value);
            return value;
        }

        // Character classification of the C locale.

        std::int32_t MORE_CDECL msvcrt_isalnum(std::int32_t c) {
            return characterClass(c) & (ClassUpper | ClassLower | ClassDigit);
        }

        std::int32_t MORE_CDECL msvcrt_isalpha(std::int32_t c) {
            return characterClass(c) & (ClassUpper | ClassLower);
        }

        std::int32_t MORE_CDECL msvcrt_iscntrl(std::int32_t c) {
            return characterClass(c) & ClassControl;
        }

        std::int32_t MORE_CDECL msvcrt_isgraph(std::int32_t c) {
            return characterClass(c) & (ClassUpper | ClassLower | ClassDigit | ClassPunct);
        }

        std::int32_t MORE_CDECL msvcrt_islower(std::int32_t c) {
            return characterClass(c) & ClassLower;
        }

        std::int32_t MORE_CDECL msvcrt_ispunct(std::int32_t c) {
            return characterClass(c) & ClassPunct;
        }

        std::int32_t MORE_CDECL msvcrt_isspace(std::int32_t c) {
            return characterClass(c) & ClassSpace;
        }

        std::int32_t MORE_CDECL msvcrt_isupper(std::int32_t c) {
            return characterClass(c) & ClassUpper;
        }

        std::int32_t MORE_CDECL msvcrt_isxdigit(std::int32_t c) {
            return characterClass(c) & ClassHex;
        }

        std::int32_t MORE_CDECL msvcrt_tolower(std::int32_t c) {
            return toLower(c);
        }

        std::int32_t MORE_CDECL msvcrt_toupper(std::int32_t c) {
            return toUpper(c);
        }

    }

    void registerStringExports(ExportRegistry &registry) {
        MORE_REGISTER(registry, msvcrt, malloc);
        MORE_REGISTER(registry, msvcrt, calloc);
        MORE_REGISTER(registry, msvcrt, realloc);
        MORE_REGISTER(registry, msvcrt, free);
        MORE_REGISTER(registry, msvcrt, memchr);
        MORE_REGISTER(registry, msvcrt, memcmp);
        MORE_REGISTER(registry, msvcrt, memcpy);
        MORE_REGISTER(registry, msvcrt, memmove);
        MORE_REGISTER(registry, msvcrt, memset);
        MORE_REGISTER(registry, msvcrt, strchr);
        MORE_REGISTER(registry, msvcrt, strcmp);
        MORE_REGISTER(registry, msvcrt, strcoll);
        MORE_REGISTER(registry, msvcrt, strcpy);
        MORE_REGISTER(registry, msvcrt, strlen);
        MORE_REGISTER(registry, msvcrt, strncmp);
        MORE_REGISTER(registry, msvcrt, strncpy);
        MORE_REGISTER(registry, msvcrt, strpbrk);
        MORE_REGISTER(registry, msvcrt, strspn);
        MORE_REGISTER(registry, msvcrt, strstr);
        MORE_REGISTER(registry, msvcrt, _strdup);
        MORE_REGISTER(registry, msvcrt, _stricmp);
        MORE_REGISTER(registry, msvcrt, _strnicmp);
        MORE_REGISTER(registry, msvcrt, _ultoa);
        MORE_REGISTER(registry, msvcrt, _mbslen);
        MORE_REGISTER(registry, msvcrt, strtol);
        MORE_REGISTER(registry, msvcrt, strtoul);
        MORE_REGISTER(registry, msvcrt, atoi);
        MORE_REGISTER(registry, msvcrt, atof);
        MORE_REGISTER(registry, msvcrt, isalnum);
        MORE_REGISTER(registry, msvcrt, isalpha);
        MORE_REGISTER(registry, msvcrt, iscntrl);
        MORE_REGISTER(registry, msvcrt, isgraph);
        MORE_REGISTER(registry, msvcrt, islower);
        MORE_REGISTER(registry, msvcrt, ispunct);
        MORE_REGISTER(registry, msvcrt, isspace);
        MORE_REGISTER(registry, msvcrt, isupper);
        MORE_REGISTER(registry, msvcrt, isxdigit);
        MORE_REGISTER(registry, msvcrt, tolower);
        MORE_REGISTER(registry, msvcrt, toupper);
    }

}
