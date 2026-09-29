#ifndef MORELOADER_SUPPORT_PATHMAPPING_H
#define MORELOADER_SUPPORT_PATHMAPPING_H

#include <optional>
#include <string>
#include <string_view>

namespace more::loader {

    /// Returns the guest form of the host path \a hostPath, in UTF-8.
    ///
    /// The mapping is that of Wine: the host root is drive \c Z:, so that \c /a/b becomes
    /// <tt>Z:\\a\\b</tt>. A relative path keeps its components, with each slash replaced by a
    /// backslash.
    std::string guestPathFromHost(std::string_view hostPath);

    /// Returns the host path to which the guest path \a guestPath refers, in UTF-8.
    ///
    /// Both separators are accepted. The drive \c Z: and a leading separator, which denotes the
    /// root of the current drive \c Z:, map to the host root. The prefixes <tt>\\\\?\\</tt> and
    /// <tt>\\\\.\\</tt> are removed. A final component \c nul, in any letter case, maps to
    /// \c /dev/null. A path on any other drive is returned with its
    /// separators replaced, and therefore does not exist on the host.
    std::string hostPathFromGuest(std::string_view guestPath);

    /// Returns the guest form of the command-line argument \a argument.
    ///
    /// An argument that begins with a slash is taken to be an absolute host path and is mapped
    /// by guestPathFromHost(). Any other argument is returned unchanged, because it may be a
    /// flag string of the resampler that contains slashes.
    std::string guestArgumentFromHost(std::string_view argument);

    /// Result of fullGuestPath().
    struct FullGuestPath {
        std::string path;

        /// Offset of the final component in \a path, or \c std::nullopt if \a path ends with a
        /// separator or names a device.
        std::optional<std::size_t> filePart;
    };

    /// Returns the absolute form of the guest path \a path as \c GetFullPathNameA does, with
    /// \a currentDirectory as the current directory in the form <tt>X:\\dir</tt>.
    ///
    /// Slashes become backslashes and repeated separators are merged. The components \c . and
    /// \c .. are resolved and do not ascend above the root. Trailing dots and spaces are removed
    /// from each component. A final separator is kept. A path that begins with a separator
    /// refers to the root of the current drive. A path on another drive without a separator
    /// after the colon refers to the root of that drive, because the loader keeps no current
    /// directory per drive. A UNC path keeps the server and share as its root. A final
    /// component \c nul becomes <tt>\\\\.\\nul</tt>.
    ///
    /// \sa moreloader/tests/auto/data/msvcrt/fullpath.txt
    FullGuestPath fullGuestPath(std::string_view path, std::string_view currentDirectory);

}

#endif // MORELOADER_SUPPORT_PATHMAPPING_H
