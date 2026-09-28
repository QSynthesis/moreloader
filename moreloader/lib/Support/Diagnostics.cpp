#include "Diagnostics.h"

#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace more::loader {

    static std::atomic<unsigned> s_enabledCategories{0};

    static unsigned categoryBit(DiagnosticCategory category) {
        return 1u << static_cast<unsigned>(category);
    }

    void setDiagnosticEnabled(DiagnosticCategory category, bool enabled) {
        if (enabled) {
            s_enabledCategories.fetch_or(categoryBit(category));
        } else {
            s_enabledCategories.fetch_and(~categoryBit(category));
        }
    }

    bool isDiagnosticEnabled(DiagnosticCategory category) {
        return (s_enabledCategories.load(std::memory_order_relaxed) & categoryBit(category)) != 0;
    }

    static void writeLine(const char *format, va_list args) {
        char buffer[4096];
        static const char prefix[] = "moreloader: ";
        size_t length = sizeof prefix - 1;
        std::memcpy(buffer, prefix, length);

        int n = std::vsnprintf(buffer + length, sizeof buffer - length - 1, format, args);
        if (n < 0) {
            n = 0;
        }
        length += std::min<size_t>(size_t(n), sizeof buffer - length - 2);
        buffer[length++] = '\n';

        const char *p = buffer;
        while (length > 0) {
            ssize_t written = ::write(STDERR_FILENO, p, length);
            if (written <= 0) {
                break;
            }
            p += written;
            length -= size_t(written);
        }
    }

    void diagnostic(const char *format, ...) {
        va_list args;
        va_start(args, format);
        writeLine(format, args);
        va_end(args);
    }

    void fatal(const char *format, ...) {
        va_list args;
        va_start(args, format);
        writeLine(format, args);
        va_end(args);
        ::_exit(255);
    }

}
