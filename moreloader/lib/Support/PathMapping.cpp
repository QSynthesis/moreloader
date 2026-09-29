#include "PathMapping.h"

#include <vector>

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

    FullGuestPath fullGuestPath(std::string_view path, std::string_view currentDirectory) {
        if (isNulDevice(path)) {
            std::string_view name = path.substr(path.find_last_of("\\/") + 1);
            while (!name.empty() && name.back() == ' ') {
                name.remove_suffix(1);
            }
            return {"\\\\.\\" + std::string(name), std::nullopt};
        }

        // The root, which the components cannot ascend above, and the remainder of the path.
        std::string root;
        std::string_view rest;
        bool hasDrive = path.size() >= 2 && path[1] == ':';
        if (path.size() >= 2 && isSeparator(path[0]) && isSeparator(path[1])) {
            // \\server\share: the server and the share belong to the root.
            std::size_t server = path.find_first_of("\\/", 2);
            std::size_t share =
                server == std::string_view::npos ? server : path.find_first_of("\\/", server + 1);
            root = std::string(path.substr(0, share));
            rest = share == std::string_view::npos ? std::string_view() : path.substr(share);
        } else if (hasDrive && path.size() >= 3 && isSeparator(path[2])) {
            root = std::string(path.substr(0, 2));
            rest = path.substr(2);
        } else if (hasDrive && toLowerASCII(path[0]) != toLowerASCII(currentDirectory[0])) {
            root = std::string(path.substr(0, 2));
            rest = path.substr(2);
        } else if (!path.empty() && isSeparator(path[0])) {
            root = std::string(currentDirectory.substr(0, 2));
            rest = path;
        } else {
            // A relative path, or a path relative to the current directory of the current drive.
            root = std::string(currentDirectory.substr(0, 2));
            std::string_view relative = hasDrive ? path.substr(2) : path;
            return fullGuestPath(std::string(currentDirectory) + "\\" + std::string(relative),
                                 currentDirectory);
        }

        std::vector<std::string> components;
        std::size_t start = 0;
        while (start < rest.size()) {
            std::size_t end = rest.find_first_of("\\/", start);
            if (end == std::string_view::npos) {
                end = rest.size();
            }
            std::string component(rest.substr(start, end - start));
            start = end + 1;
            if (component.empty() || component == ".") {
                continue;
            }
            if (component == "..") {
                if (!components.empty()) {
                    components.pop_back();
                }
                continue;
            }
            while (!component.empty() && (component.back() == '.' || component.back() == ' ')) {
                component.pop_back();
            }
            if (!component.empty()) {
                components.push_back(std::move(component));
            }
        }
        bool trailingSeparator = rest.empty() || isSeparator(rest.back()) || components.empty();

        FullGuestPath result;
        result.path = root;
        for (const std::string &component : components) {
            result.path += "\\";
            if (&component == &components.back() && !trailingSeparator) {
                result.filePart = result.path.size();
            }
            result.path += component;
        }
        if (trailingSeparator) {
            result.path += "\\";
        }
        return result;
    }
}
