#ifndef MORELOADER_RUNTIME_THREADAREA_H
#define MORELOADER_RUNTIME_THREADAREA_H

#include <cstdint>
#include <optional>

namespace more::loader {

    /// Points the thread-local segment descriptor of the calling thread at \a limit + 1 bytes
    /// at \a base, and returns the selector of the descriptor.
    ///
    /// The guest addresses its TEB through FS, which i386 glibc leaves unused because it keeps
    /// its own thread pointer in GS. The descriptor is one of the TLS entries of the GDT that
    /// \c set_thread_area manages. These entries belong to each thread and are copied to a new
    /// thread by \c clone, therefore the process allocates one entry number on the first call and
    /// every thread writes its own base into that entry. The LDT is not used because
    /// \c modify_ldt is shared by all threads and is not implemented by FEX-Emu for 32-bit
    /// guests.
    ///
    /// \return the selector, or \c std::nullopt if \c set_thread_area fails
    std::optional<std::uint16_t> setThreadArea(std::uint32_t base, std::uint32_t limit);

    /// Loads \a selector into FS of the calling thread.
    void loadFS(std::uint16_t selector);

}

#endif // MORELOADER_RUNTIME_THREADAREA_H
