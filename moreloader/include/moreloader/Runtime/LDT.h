#ifndef MORELOADER_RUNTIME_LDT_H
#define MORELOADER_RUNTIME_LDT_H

#include <cstdint>
#include <optional>

namespace more::loader {

    /// Allocates an entry of the local descriptor table that describes a 32-bit data segment of
    /// \a limit + 1 bytes at \a base.
    ///
    /// The guest addresses its TEB through FS, which i386 glibc leaves unused because it keeps
    /// its own thread pointer in GS. Each thread requires its own entry, because the LDT is
    /// shared by all threads of the process.
    ///
    /// \return the selector of the entry, or \c std::nullopt if \c modify_ldt fails or the
    ///         table is full
    std::optional<std::uint16_t> allocateDataSegment(std::uint32_t base, std::uint32_t limit);

    /// Clears and releases the entry of \a selector.
    void releaseDataSegment(std::uint16_t selector);

    /// Loads \a selector into FS of the calling thread.
    void loadFS(std::uint16_t selector);

}

#endif // MORELOADER_RUNTIME_LDT_H
