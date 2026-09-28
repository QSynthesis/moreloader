#include "WinAPI_p.h"

#include <moreloader/Support/CodePage.h>
#include <moreloader/Support/Diagnostics.h>

namespace more::loader::winapi {

    namespace {

        constexpr std::int32_t idOK = 1;

        /// Writes the message to the standard error of the host and returns \c IDOK, because
        /// the loader has no graphical interface. moresampler shows a message box if it is
        /// started without arguments.
        std::int32_t MORE_WINAPI user32_MessageBoxW(HANDLE window, const char16_t *text,
                                                    const char16_t *caption, DWORD type) {
            std::string message = text ? wideToMultiByte(text).text : std::string();
            std::string title = caption ? wideToMultiByte(caption).text : std::string();
            diagnostic("%s: %s", title.c_str(), message.c_str());
            return idOK;
        }

    }

    void registerUser32(ExportRegistry &registry) {
        MORE_REGISTER(registry, user32, MessageBoxW);
    }

}
