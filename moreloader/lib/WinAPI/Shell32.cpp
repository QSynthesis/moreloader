#include "WinAPI_p.h"

#include <cstdlib>
#include <cstring>
#include <vector>

#include <moreloader/Runtime/GuestThread.h>
#include <moreloader/Runtime/Process.h>
#include <moreloader/Support/CommandLine.h>

namespace more::loader::winapi {

    namespace {

        /// Splits \a commandLine by the rules of shell32. The array and the strings are one
        /// allocation, which the guest releases with \c LocalFree. moresampler does not import
        /// \c LocalFree, therefore the allocation is never released.
        char16_t **MORE_WINAPI shell32_CommandLineToArgvW(const char16_t *commandLine,
                                                         std::int32_t *count) {
            if (!count) {
                setLastError(ErrorInvalidParameter);
                return nullptr;
            }
            std::vector<std::u16string> arguments;
            if (!commandLine || commandLine[0] == 0) {
                arguments.push_back(process().modulePath());
            } else {
                arguments = splitCommandLine(commandLine);
            }

            std::size_t size = (arguments.size() + 1) * sizeof(char16_t *);
            for (const auto &argument : arguments) {
                size += (argument.size() + 1) * sizeof(char16_t);
            }
            auto block = static_cast<char *>(std::malloc(size));
            if (!block) {
                setLastError(ErrorNotEnoughMemory);
                return nullptr;
            }
            auto pointers = reinterpret_cast<char16_t **>(block);
            auto strings = reinterpret_cast<char16_t *>(block + (arguments.size() + 1) *
                                                                    sizeof(char16_t *));
            for (std::size_t i = 0; i < arguments.size(); ++i) {
                pointers[i] = strings;
                std::memcpy(strings, arguments[i].c_str(),
                            (arguments[i].size() + 1) * sizeof(char16_t));
                strings += arguments[i].size() + 1;
            }
            pointers[arguments.size()] = nullptr;
            *count = std::int32_t(arguments.size());
            return pointers;
        }

    }

    void registerShell32(ExportRegistry &registry) {
        MORE_REGISTER(registry, shell32, CommandLineToArgvW);
    }

}
