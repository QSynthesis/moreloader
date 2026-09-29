#include "CRTExports_p.h"

#include <cstdlib>
#include <cstring>

#include "GuestHeap_p.h"
#include "Numbers.h"

namespace more::loader::msvcrt {

    namespace {

        // The guest's wchar_t has 16 bits, whereas the host's has 32, therefore none of these
        // functions may be forwarded to the wcs functions of the host.

        std::size_t length(const char16_t *s) {
            std::size_t n = 0;
            while (s[n]) {
                ++n;
            }
            return n;
        }

        int compareUnits(const char16_t *a, const char16_t *b, std::size_t n) {
            for (std::size_t i = 0; i < n; ++i) {
                if (a[i] != b[i]) {
                    return a[i] < b[i] ? -1 : 1;
                }
                if (a[i] == 0) {
                    break;
                }
            }
            return 0;
        }

        std::uint32_t MORE_CDECL msvcrt_wcslen(const char16_t *s) {
            return std::uint32_t(length(s));
        }

        char16_t *MORE_CDECL msvcrt_wcscpy(char16_t *d, const char16_t *s) {
            std::memcpy(d, s, (length(s) + 1) * sizeof(char16_t));
            return d;
        }

        char16_t *MORE_CDECL msvcrt_wcscat(char16_t *d, const char16_t *s) {
            std::memcpy(d + length(d), s, (length(s) + 1) * sizeof(char16_t));
            return d;
        }

        /// Copies at most \a n characters and pads the remainder of \a n with zeros.
        char16_t *MORE_CDECL msvcrt_wcsncpy(char16_t *d, const char16_t *s, std::uint32_t n) {
            std::uint32_t i = 0;
            for (; i < n && s[i]; ++i) {
                d[i] = s[i];
            }
            for (; i < n; ++i) {
                d[i] = 0;
            }
            return d;
        }

        std::int32_t MORE_CDECL msvcrt_wcscmp(const char16_t *a, const char16_t *b) {
            return compareUnits(a, b, std::size_t(-1));
        }

        std::int32_t MORE_CDECL msvcrt_wcsncmp(const char16_t *a, const char16_t *b,
                                              std::uint32_t n) {
            return compareUnits(a, b, n);
        }

        char16_t *MORE_CDECL msvcrt__wcsdup(const char16_t *s) {
            if (!s) {
                return nullptr;
            }
            std::size_t size = (length(s) + 1) * sizeof(char16_t);
            auto p = static_cast<char16_t *>(guestAllocate(size, false, MORE_RETURN_ADDRESS()));
            if (!p) {
                threadErrno() = ErrnoNoMemory;
                return nullptr;
            }
            return static_cast<char16_t *>(std::memcpy(p, s, size));
        }

        std::int32_t MORE_CDECL msvcrt_wcstol(const char16_t *text, char16_t **end,
                                             std::int32_t base) {
            auto result = strtol(text, base);
            if (end) {
                *end = const_cast<char16_t *>(text) + result.consumed;
            }
            if (result.overflow) {
                threadErrno() = ErrnoRange;
            }
            return std::int32_t(result.value);
        }

    }

    void registerWideStringExports(ExportRegistry &registry) {
        MORE_REGISTER(registry, msvcrt, wcslen);
        MORE_REGISTER(registry, msvcrt, wcscpy);
        MORE_REGISTER(registry, msvcrt, wcscat);
        MORE_REGISTER(registry, msvcrt, wcsncpy);
        MORE_REGISTER(registry, msvcrt, wcscmp);
        MORE_REGISTER(registry, msvcrt, wcsncmp);
        MORE_REGISTER(registry, msvcrt, _wcsdup);
        MORE_REGISTER(registry, msvcrt, wcstol);
    }

}
