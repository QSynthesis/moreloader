#ifndef MORELOADER_SUPPORT_PATHMAPPING_H
#define MORELOADER_SUPPORT_PATHMAPPING_H

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

}

#endif // MORELOADER_SUPPORT_PATHMAPPING_H
