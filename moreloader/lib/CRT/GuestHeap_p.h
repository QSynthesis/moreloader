#ifndef MORELOADER_CRT_GUESTHEAP_P_H
#define MORELOADER_CRT_GUESTHEAP_P_H

#include <cstddef>

namespace more::loader::msvcrt {

    /// The heap of the msvcrt functions: every block that the guest frees with \c free comes
    /// from these functions.
    ///
    /// The blocks are those of the host allocator. With the diagnostic category \c HeapCheck,
    /// each block carries a header and 16 guard bytes after its end, which free and realloc
    /// check. A write beyond the end of a block is then reported with the guest addresses that
    /// allocated and freed the block. Such a write can go unnoticed by one allocator and corrupt
    /// another, whose blocks have less slack.
    ///
    /// \a caller is the guest address that called the msvcrt function, for the report.

    /// Allocates \a size bytes, filled with zeros if \a zero.
    void *guestAllocate(std::size_t size, bool zero, const void *caller);

    /// Resizes \a block as \c realloc does. A null \a block allocates.
    void *guestReallocate(void *block, std::size_t size, const void *caller);

    /// Frees \a block. A null \a block is ignored.
    void guestFree(void *block, const void *caller);

}

#endif // MORELOADER_CRT_GUESTHEAP_P_H
