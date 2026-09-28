#include "PathMapping.h"

namespace more::loader {

    static bool isSeparator(char c) {
        return c == '\\' || c == '/';
    }

    static char toLowerASCII(char c) {
        return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
    }

    // Whether the final component of the path is the device nul. Windows resolves a device name
    // in any directory, therefore only the final component is examined. A name with an
    // extension, such as nul.txt, is a device before Windows 11 and a file since, and is treated
    // as a file.
    static bool isNulDevice(std::string_view path) {
        size_t start = path.find_last_of("\\/");
        std::string_view stem = start == std::string_view::npos ? path : path.substr(start + 1);
        // Trailing spaces are ignored in device names.
        while (!stem.empty() && stem.back() == ' ') {
            stem.remove_suffix(1);
        }
        if (stem.size() != 3) {
            return false;
        }
        return toLowerASCII(stem[0]) == 'n' && toLowerASCII(stem[1]) == 'u' &&
               toLowerASCII(stem[2]) == 'l';
    }

    std::string guestPathFromHost(std::string_view hostPath) {
        std::string out;
        if (!hostPath.empty() && hostPath[0] == '/') {
            out = "Z:";
        }
        for (char c : hostPath) {
            out.push_back(c == '/' ? '\\' : c);
        }
        if (out == "Z:") {
            out.push_back('\\');
        }
        return out;
    }

    std::string hostPathFromGuest(std::string_view guestPath) {
        if (isNulDevice(guestPath)) {
            return "/dev/null";
        }

        std::string_view path = guestPath;
        if (path.size() >= 4 && isSeparator(path[0]) && isSeparator(path[1]) &&
            (path[2] == '?' || path[2] == '.') && isSeparator(path[3])) {
            path.remove_prefix(4);
        }

        std::string out;
        if (path.size() >= 2 && path[1] == ':') {
            char drive = toLowerASCII(path[0]);
            path.remove_prefix(2);
            if (drive == 'z') {
                // Z:relative is relative to the current directory of drive Z:, which is the
                // current directory of the process.
                if (!path.empty() && isSeparator(path[0])) {
                    out = "/";
                    path.remove_prefix(1);
                } else if (path.empty()) {
                    return "/";
                }
            } else {
                out = std::string(1, drive) + ":/";
                if (!path.empty() && isSeparator(path[0])) {
                    path.remove_prefix(1);
                }
            }
        } else if (!path.empty() && isSeparator(path[0])) {
            out = "/";
            path.remove_prefix(1);
        }

        for (char c : path) {
            out.push_back(c == '\\' ? '/' : c);
        }
        return out;
    }

    std::string guestArgumentFromHost(std::string_view argument) {
        if (!argument.empty() && argument[0] == '/') {
            return guestPathFromHost(argument);
        }
        return std::string(argument);
    }

}
