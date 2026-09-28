#include "CRTExports_p.h"

#include <cstdarg>
#include <cstring>

#include "Format.h"

namespace more::loader::msvcrt {

    namespace {

        // Writes the result of a narrow format to \a file.
        std::int32_t printTo(MsvcrtFILE *file, const FormatResult<char> &result) {
            bool ok = writeToFile(file, result.text);
            if (result.failed || !ok) {
                return -1;
            }
            return std::int32_t(result.text.size());
        }

        // Writes the result of a wide format to \a file. msvcrt converts each character with
        // wctomb in the C locale, which fails for characters above U+00FF.
        std::int32_t printWideTo(MsvcrtFILE *file, const FormatResult<char16_t> &result) {
            std::string narrow;
            bool failed = result.failed;
            for (char16_t c : result.text) {
                if (c > 0xFF) {
                    failed = true;
                    break;
                }
                narrow.push_back(char(c));
            }
            bool ok = writeToFile(file, narrow);
            if (failed || !ok) {
                return -1;
            }
            return std::int32_t(result.text.size());
        }

        // Every variadic wrapper reads its arguments through the va_list of the host, which on
        // i386 is a pointer to the arguments on the stack, as it is for the guest.
        std::int32_t MORE_CDECL msvcrt_printf(const char *format, ...) {
            va_list list;
            va_start(list, format);
            GuestArguments arguments(list);
            auto result = formatNarrow(format, arguments);
            va_end(list);
            return printTo(&msvcrt__iob[1], result);
        }

        std::int32_t MORE_CDECL msvcrt_vprintf(const char *format, const std::uint8_t *list) {
            GuestArguments arguments(list);
            return printTo(&msvcrt__iob[1], formatNarrow(format, arguments));
        }

        std::int32_t MORE_CDECL msvcrt_fprintf(MsvcrtFILE *file, const char *format, ...) {
            va_list list;
            va_start(list, format);
            GuestArguments arguments(list);
            auto result = formatNarrow(format, arguments);
            va_end(list);
            return printTo(file, result);
        }

        std::int32_t MORE_CDECL msvcrt_vfprintf(MsvcrtFILE *file, const char *format,
                                               const std::uint8_t *list) {
            GuestArguments arguments(list);
            return printTo(file, formatNarrow(format, arguments));
        }

        std::int32_t MORE_CDECL msvcrt_sprintf(char *buffer, const char *format, ...) {
            va_list list;
            va_start(list, format);
            GuestArguments arguments(list);
            auto result = formatNarrow(format, arguments);
            va_end(list);
            std::memcpy(buffer, result.text.data(), result.text.size());
            buffer[result.text.size()] = 0;
            return result.failed ? -1 : std::int32_t(result.text.size());
        }

        std::int32_t MORE_CDECL msvcrt__vsnwprintf(char16_t *buffer, std::uint32_t count,
                                                  const char16_t *format,
                                                  const std::uint8_t *list) {
            GuestArguments arguments(list);
            return storeTruncated(formatWide(format, arguments), buffer, count);
        }

        std::int32_t MORE_CDECL msvcrt__snwprintf(char16_t *buffer, std::uint32_t count,
                                                 const char16_t *format, ...) {
            va_list list;
            va_start(list, format);
            GuestArguments arguments(list);
            auto result = formatWide(format, arguments);
            va_end(list);
            return storeTruncated(result, buffer, count);
        }

        std::int32_t MORE_CDECL msvcrt_fwprintf(MsvcrtFILE *file, const char16_t *format, ...) {
            va_list list;
            va_start(list, format);
            GuestArguments arguments(list);
            auto result = formatWide(format, arguments);
            va_end(list);
            return printWideTo(file, result);
        }

    }

    void registerFormatExports(ExportRegistry &registry) {
        MORE_REGISTER(registry, msvcrt, printf);
        MORE_REGISTER(registry, msvcrt, vprintf);
        MORE_REGISTER(registry, msvcrt, fprintf);
        MORE_REGISTER(registry, msvcrt, vfprintf);
        MORE_REGISTER(registry, msvcrt, sprintf);
        MORE_REGISTER(registry, msvcrt, _vsnwprintf);
        MORE_REGISTER(registry, msvcrt, _snwprintf);
        MORE_REGISTER(registry, msvcrt, fwprintf);
    }

}
