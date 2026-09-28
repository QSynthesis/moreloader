#include "WinAPI_p.h"

#include <sys/stat.h>

#include <moreloader/Support/CodePage.h>
#include <moreloader/Support/PathMapping.h>

namespace more::loader::winapi {

    namespace {

        constexpr BOOL fileAttributeDirectory = 0x10;

        /// Returns \c FILE_ATTRIBUTE_DIRECTORY if the path names a directory, and \c FALSE
        /// otherwise.
        BOOL MORE_WINAPI shlwapi_PathIsDirectoryW(const char16_t *path) {
            if (!path) {
                return FALSE;
            }
            std::string host = hostPathFromGuest(wideToMultiByte(path).text);
            struct stat info;
            if (::stat(host.c_str(), &info) != 0 || !S_ISDIR(info.st_mode)) {
                return FALSE;
            }
            return fileAttributeDirectory;
        }

    }

    void registerShlwapi(ExportRegistry &registry) {
        MORE_REGISTER(registry, shlwapi, PathIsDirectoryW);
    }

}
