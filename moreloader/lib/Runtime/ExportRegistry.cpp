#include "ExportRegistry.h"

namespace more::loader {

    std::string ExportRegistry::normalizeLibrary(std::string_view library) {
        std::string out;
        for (char c : library) {
            out.push_back(c >= 'A' && c <= 'Z' ? char(c - 'A' + 'a') : c);
        }
        if (out.size() > 4 && out.compare(out.size() - 4, 4, ".dll") == 0) {
            out.resize(out.size() - 4);
        }
        return out;
    }

    void ExportRegistry::add(std::string_view library, std::string_view name, void *address,
                             ExportKind kind) {
        m_exports[normalizeLibrary(library)][std::string(name)] = Export{address, kind};
    }

    std::optional<Export> ExportRegistry::find(std::string_view library,
                                               std::string_view name) const {
        auto lib = m_exports.find(normalizeLibrary(library));
        if (lib == m_exports.end()) {
            return std::nullopt;
        }
        auto it = lib->second.find(name);
        if (it == lib->second.end()) {
            return std::nullopt;
        }
        return it->second;
    }

    bool ExportRegistry::hasLibrary(std::string_view library) const {
        return m_exports.count(normalizeLibrary(library)) != 0;
    }

    std::vector<std::string> ExportRegistry::libraries() const {
        std::vector<std::string> out;
        for (auto &[name, exports] : m_exports) {
            out.push_back(name);
        }
        return out;
    }

}
