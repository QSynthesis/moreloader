#include "Sort.h"

namespace more::loader::msvcrt {

    namespace {

        // Partitions of at most this many elements are sorted by selection.
        constexpr std::size_t cutoff = 8;

        // The explicit stack holds the partitions that remain to be sorted. The smaller half is
        // always processed first, therefore 8 * sizeof(void *) - 2 entries suffice.
        constexpr int stackSize = 8 * sizeof(void *) - 2;

        void swapElements(char *a, char *b, std::size_t size) {
            if (a == b) {
                return;
            }
            while (size--) {
                char t = *a;
                *a++ = *b;
                *b++ = t;
            }
        }

        // Moves the largest element to the end, then repeats on the remaining elements. Of
        // several equal maxima the first is selected.
        void shortSort(char *lo, char *hi, std::size_t size, Comparator compare) {
            while (hi > lo) {
                char *max = lo;
                for (char *p = lo + size; p <= hi; p += size) {
                    if (compare(p, max) > 0) {
                        max = p;
                    }
                }
                swapElements(max, hi, size);
                hi -= size;
            }
        }

    }

    void quickSort(void *base, std::size_t count, std::size_t size, Comparator compare) {
        if (count < 2 || size == 0) {
            return;
        }

        char *lowStack[stackSize];
        char *highStack[stackSize];
        int stackPointer = 0;

        char *lo = static_cast<char *>(base);
        char *hi = lo + size * (count - 1);

        for (;;) {
            std::size_t partitionSize = std::size_t(hi - lo) / size + 1;

            if (partitionSize <= cutoff) {
                shortSort(lo, hi, size, compare);
            } else {
                // The median of the first, middle and last elements becomes the pivot, which is
                // tracked by address as the elements move.
                char *mid = lo + (partitionSize / 2) * size;
                if (compare(lo, mid) > 0) {
                    swapElements(lo, mid, size);
                }
                if (compare(lo, hi) > 0) {
                    swapElements(lo, hi, size);
                }
                if (compare(mid, hi) > 0) {
                    swapElements(mid, hi, size);
                }

                char *loGuy = lo;
                char *hiGuy = hi;
                for (;;) {
                    if (mid > loGuy) {
                        do {
                            loGuy += size;
                        } while (loGuy < mid && compare(loGuy, mid) <= 0);
                    }
                    if (mid <= loGuy) {
                        do {
                            loGuy += size;
                        } while (loGuy <= hi && compare(loGuy, mid) <= 0);
                    }

                    do {
                        hiGuy -= size;
                    } while (hiGuy > mid && compare(hiGuy, mid) > 0);

                    if (hiGuy < loGuy) {
                        break;
                    }

                    swapElements(loGuy, hiGuy, size);

                    if (mid == hiGuy) {
                        mid = loGuy;
                    }
                }

                // Elements equal to the pivot adjacent to the boundary are excluded from the
                // lower partition.
                hiGuy += size;
                if (mid < hiGuy) {
                    do {
                        hiGuy -= size;
                    } while (hiGuy > mid && compare(hiGuy, mid) == 0);
                }
                if (mid >= hiGuy) {
                    do {
                        hiGuy -= size;
                    } while (hiGuy > lo && compare(hiGuy, mid) == 0);
                }

                // The larger partition is deferred and the smaller one is sorted next.
                if (hiGuy - lo >= hi - loGuy) {
                    if (lo < hiGuy) {
                        lowStack[stackPointer] = lo;
                        highStack[stackPointer] = hiGuy;
                        ++stackPointer;
                    }
                    if (loGuy < hi) {
                        lo = loGuy;
                        continue;
                    }
                } else {
                    if (loGuy < hi) {
                        lowStack[stackPointer] = loGuy;
                        highStack[stackPointer] = hi;
                        ++stackPointer;
                    }
                    if (lo < hiGuy) {
                        hi = hiGuy;
                        continue;
                    }
                }
            }

            --stackPointer;
            if (stackPointer < 0) {
                return;
            }
            lo = lowStack[stackPointer];
            hi = highStack[stackPointer];
        }
    }

}
