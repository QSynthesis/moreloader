#ifndef MORELOADER_TESTS_GOLDENDATA_H
#define MORELOADER_TESTS_GOLDENDATA_H

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

/// Reading of the golden data that moreloader/tests/probe/msvcrt/MsvcrtProbe.cpp measured on
/// Windows. The files are UTF-8 text with one case per line and tab-separated fields. Strings are
/// escaped: every byte other than printable ASCII, backslash, tab and comma is written as \xHH.
namespace golden {

    /// Returns the path of the data file \a name in the directory of msvcrt measurements.
    inline std::string path(const std::string &name) {
        return std::string(MORE_TEST_DATA_DIR) + "/msvcrt/" + name;
    }

    /// Returns the lines of \a name, each split into fields. Empty fields are preserved.
    inline std::vector<std::vector<std::string>> read(const std::string &name) {
        std::ifstream in(path(name), std::ios::binary);
        if (!in) {
            throw std::runtime_error("cannot read golden data " + path(name));
        }
        std::vector<std::vector<std::string>> lines;
        std::string line;
        while (std::getline(in, line)) {
            std::vector<std::string> fields;
            size_t start = 0;
            while (true) {
                size_t tab = line.find('\t', start);
                fields.push_back(line.substr(start, tab - start));
                if (tab == std::string::npos) {
                    break;
                }
                start = tab + 1;
            }
            lines.push_back(std::move(fields));
        }
        return lines;
    }

    /// Reverses the escaping of the probe.
    inline std::string unescape(const std::string &text) {
        std::string out;
        for (size_t i = 0; i < text.size(); ++i) {
            if (text[i] == '\\' && i + 3 < text.size() && text[i + 1] == 'x') {
                out.push_back(static_cast<char>(std::stoi(text.substr(i + 2, 2), nullptr, 16)));
                i += 3;
            } else {
                out.push_back(text[i]);
            }
        }
        return out;
    }

    /// Parses a space-separated list of hexadecimal UTF-16 code units.
    inline std::u16string units(const std::string &text) {
        std::u16string out;
        size_t i = 0;
        while (i < text.size()) {
            size_t end = text.find(' ', i);
            if (end == std::string::npos) {
                end = text.size();
            }
            out.push_back(static_cast<char16_t>(std::stoul(text.substr(i, end - i), nullptr, 16)));
            i = end + 1;
        }
        return out;
    }

    /// Formats \a text as the probe formats code units, for comparison and for messages.
    inline std::string unitsText(const std::u16string &text) {
        std::string out;
        char buffer[8];
        for (size_t i = 0; i < text.size(); ++i) {
            std::snprintf(buffer, sizeof buffer, "%s%04X", i ? " " : "", unsigned(text[i]));
            out += buffer;
        }
        return out;
    }

    /// Parses 16 hexadecimal digits as the bits of a double.
    inline uint64_t bits(const std::string &text) {
        return std::stoull(text, nullptr, 16);
    }

}

#endif // MORELOADER_TESTS_GOLDENDATA_H
