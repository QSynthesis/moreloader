#ifndef MORELOADER_CRT_SORT_H
#define MORELOADER_CRT_SORT_H

#include <cstddef>

namespace more::loader::msvcrt {

    /// Comparison function of the guest, called with the \c __cdecl convention, which is also the
    /// convention of the host on i386.
    using Comparator = int (*)(const void *, const void *);

    /// Sorts \a count elements of \a size bytes at \a base as \c qsort of msvcrt.dll does.
    ///
    /// The algorithm determines the order of equal elements and the sequence of calls of
    /// \a compare, both of which differ from glibc, whose \c qsort is a merge sort. The order of
    /// equal elements affects the output of the guest if its comparison function reports
    /// distinct elements as equal.
    ///
    /// \sa moreloader/tests/auto/data/msvcrt/qsort.txt
    void quickSort(void *base, std::size_t count, std::size_t size, Comparator compare);

}

#endif // MORELOADER_CRT_SORT_H
