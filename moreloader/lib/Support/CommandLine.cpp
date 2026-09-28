#include "CommandLine.h"

namespace more::loader {

    static bool isBlank(char16_t c) {
        return c == u' ' || c == u'\t';
    }

    std::vector<std::u16string> splitCommandLine(std::u16string_view commandLine) {
        std::vector<std::u16string> arguments;
        if (commandLine.empty()) {
            return arguments;
        }

        // The program name.
        size_t i = 0;
        std::u16string current;
        if (commandLine[0] == u'"') {
            ++i;
            while (i < commandLine.size() && commandLine[i] != u'"') {
                current.push_back(commandLine[i++]);
            }
            if (i < commandLine.size()) {
                ++i;
            }
        } else {
            while (i < commandLine.size() && !isBlank(commandLine[i])) {
                current.push_back(commandLine[i++]);
            }
        }
        arguments.push_back(std::move(current));
        current.clear();

        while (i < commandLine.size() && isBlank(commandLine[i])) {
            ++i;
        }
        if (i == commandLine.size()) {
            return arguments;
        }

        // The remaining arguments. The quote count follows the algorithm of shell32, in which
        // the count includes the opening quotation mark of the run, a run of three produces one
        // literal quotation mark, and a count of two closes the run.
        size_t backslashes = 0;
        int quotes = 0;
        bool pending = false;
        while (i < commandLine.size()) {
            char16_t c = commandLine[i];
            if (isBlank(c) && quotes == 0) {
                if (pending) {
                    arguments.push_back(std::move(current));
                    current.clear();
                    pending = false;
                }
                backslashes = 0;
                ++i;
                continue;
            }

            pending = true;
            if (c == u'\\') {
                current.push_back(c);
                ++backslashes;
                ++i;
            } else if (c == u'"') {
                if (backslashes % 2 == 0) {
                    current.resize(current.size() - backslashes / 2);
                    ++quotes;
                } else {
                    current.resize(current.size() - backslashes / 2 - 1);
                    current.push_back(u'"');
                }
                ++i;
                backslashes = 0;
                while (i < commandLine.size() && commandLine[i] == u'"') {
                    if (++quotes == 3) {
                        current.push_back(u'"');
                        quotes = 0;
                    }
                    ++i;
                }
                if (quotes == 2) {
                    quotes = 0;
                }
            } else {
                current.push_back(c);
                backslashes = 0;
                ++i;
            }
        }
        if (pending) {
            arguments.push_back(std::move(current));
        }
        return arguments;
    }

    static void appendQuoted(std::u16string &out, const std::u16string &argument) {
        bool needsQuotes = argument.empty();
        for (char16_t c : argument) {
            if (isBlank(c) || c == u'"') {
                needsQuotes = true;
                break;
            }
        }
        if (!needsQuotes) {
            out += argument;
            return;
        }

        out.push_back(u'"');
        size_t backslashes = 0;
        for (char16_t c : argument) {
            if (c == u'\\') {
                ++backslashes;
                continue;
            }
            if (c == u'"') {
                // Each backslash before a quotation mark is doubled, and one more escapes the
                // quotation mark itself.
                out.append(backslashes * 2 + 1, u'\\');
            } else {
                out.append(backslashes, u'\\');
            }
            backslashes = 0;
            out.push_back(c);
        }
        // Backslashes before the closing quotation mark are doubled.
        out.append(backslashes * 2, u'\\');
        out.push_back(u'"');
    }

    std::u16string joinCommandLine(const std::vector<std::u16string> &arguments) {
        std::u16string out;
        for (size_t i = 0; i < arguments.size(); ++i) {
            if (i == 0) {
                bool needsQuotes = arguments[0].empty();
                for (char16_t c : arguments[0]) {
                    needsQuotes = needsQuotes || isBlank(c);
                }
                if (needsQuotes) {
                    out += u'"' + arguments[0] + u'"';
                } else {
                    out += arguments[0];
                }
                continue;
            }
            out.push_back(u' ');
            appendQuoted(out, arguments[i]);
        }
        return out;
    }

}
