#ifndef MORELOADER_SUPPORT_DIAGNOSTICS_H
#define MORELOADER_SUPPORT_DIAGNOSTICS_H

#include <moreloader/Support/MoreLoaderSupportGlobal.h>

namespace more::loader {

    /// Categories of optional diagnostic output, selected on the command line of the driver.
    enum class DiagnosticCategory {
        /// Every call of an imported function, printed before the call.
        Imports,
        /// Calls of unimplemented or partially implemented semantics that returned a failure.
        Stubs,
        /// Strings passed to \c OutputDebugStringA.
        DebugStrings,
        /// Guard bytes after every block of the msvcrt heap, checked when the block is freed or
        /// resized. The category must be selected before the guest runs, because it changes the
        /// layout of the blocks.
        HeapCheck,
    };

    /// Enables or disables the output of \a category. All categories are disabled initially.
    void setDiagnosticEnabled(DiagnosticCategory category, bool enabled);

    /// Returns whether the output of \a category is enabled.
    bool isDiagnosticEnabled(DiagnosticCategory category);

    /// Writes a diagnostic line prefixed with \c moreloader: to the standard error of the host.
    ///
    /// The line is written with a single \c write call, therefore lines of concurrent threads do
    /// not interleave. Lines longer than 4095 bytes are truncated.
    void diagnostic(MORE_PRINTF_FORMAT_STRING const char *format, ...) MORE_PRINTF_FORMAT(1, 2);

    /// Writes a diagnostic line as diagnostic() does and terminates the process with exit code
    /// 255, without running any exit handler of the guest or the host.
    [[noreturn]] void fatal(MORE_PRINTF_FORMAT_STRING const char *format, ...)
        MORE_PRINTF_FORMAT(1, 2);

}

#endif // MORELOADER_SUPPORT_DIAGNOSTICS_H
