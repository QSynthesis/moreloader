#ifndef MORELOADER_SUPPORT_COMMANDLINE_H
#define MORELOADER_SUPPORT_COMMANDLINE_H

#include <string>
#include <string_view>
#include <vector>

namespace more::loader {

    /// Splits \a commandLine into arguments by the rules of \c CommandLineToArgvW.
    ///
    /// The first argument is the program name, which ends at the next quotation mark if it
    /// begins with one and at the next space or tab otherwise, without any escape processing. In
    /// the remaining arguments, 2n backslashes followed by a quotation mark yield n backslashes
    /// and toggle quoting, 2n + 1 backslashes followed by a quotation mark yield n backslashes
    /// and a literal quotation mark, and backslashes elsewhere are literal. Within a quoted run,
    /// every third consecutive quotation mark is literal.
    ///
    /// \return the arguments, or an empty vector if \a commandLine is empty
    ///
    /// \note \c CommandLineToArgvW returns the path of the calling executable for an empty
    ///       command line. That case is left to the caller.
    std::vector<std::u16string> splitCommandLine(std::u16string_view commandLine);

    /// Joins \a arguments into one command line that splitCommandLine() splits into the same
    /// arguments.
    ///
    /// The program name is quoted if it contains a space or a tab. It cannot represent a
    /// quotation mark, which Windows does not permit in file names.
    std::u16string joinCommandLine(const std::vector<std::u16string> &arguments);

}

#endif // MORELOADER_SUPPORT_COMMANDLINE_H
