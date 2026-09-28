#ifndef MORELOADER_RUNTIME_THUNKS_H
#define MORELOADER_RUNTIME_THUNKS_H

#include <cstdint>
#include <string>

namespace more::loader {

    /// Returns the address of generated code that reports a call of the unresolved import
    /// \a name of \a library and terminates the process. The import address table receives this
    /// address, so that an import that the guest never calls does not prevent loading.
    std::uint32_t makeUnresolvedThunk(const std::string &library, const std::string &name);

    /// Returns the address of generated code that reports a call of \a name of \a library on
    /// the standard error and jumps to \a target. All registers and flags are preserved, so that
    /// the calling convention of \a target is irrelevant.
    std::uint32_t makeTraceThunk(const std::string &library, const std::string &name,
                                 std::uint32_t target);

}

#endif // MORELOADER_RUNTIME_THUNKS_H
