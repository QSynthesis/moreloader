#ifndef MORELOADER_RUNTIME_EXPORTREGISTRY_H
#define MORELOADER_RUNTIME_EXPORTREGISTRY_H

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace more::loader {

    /// Kind of an export. Only functions receive trace thunks, because a data import is read
    /// through its address rather than called.
    enum class ExportKind {
        Function,
        Data,
    };

    /// An export of an emulated library.
    struct Export {
        void *address = nullptr;
        ExportKind kind = ExportKind::Function;
    };

    /// The exports of the emulated libraries, by library and name.
    ///
    /// Library names are compared without regard to letter case and to a trailing \c .dll, as
    /// Windows does. Export names are compared exactly.
    class ExportRegistry {
    public:
        /// Adds \a name of \a library. A later addition of the same name replaces the earlier.
        void add(std::string_view library, std::string_view name, void *address,
                 ExportKind kind = ExportKind::Function);

        /// Returns the export \a name of \a library, or \c std::nullopt if absent.
        std::optional<Export> find(std::string_view library, std::string_view name) const;

        /// Returns whether \a library has at least one export.
        bool hasLibrary(std::string_view library) const;

        /// Returns the normalized names of all libraries, in lowercase without \c .dll.
        std::vector<std::string> libraries() const;

        /// Returns the normalized form of \a library.
        static std::string normalizeLibrary(std::string_view library);

    private:
        std::map<std::string, std::map<std::string, Export, std::less<>>, std::less<>> m_exports;
    };

}

#endif // MORELOADER_RUNTIME_EXPORTREGISTRY_H
