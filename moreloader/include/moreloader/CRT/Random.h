#ifndef MORELOADER_CRT_RANDOM_H
#define MORELOADER_CRT_RANDOM_H

#include <cstdint>

namespace more::loader::msvcrt {

    /// The linear congruential generator of \c rand in msvcrt.
    ///
    /// msvcrt keeps one state per thread, and the state of a new thread is seed 1 regardless of
    /// any \c srand call in another thread. moresampler seeds the generator with 0 at startup and
    /// calls \c rand in synthesis, therefore both the algorithm and the per-thread state affect
    /// its output.
    ///
    /// \sa moreloader/tests/auto/data/msvcrt/rand.txt
    class Random {
    public:
        inline void seed(std::uint32_t seed) {
            m_state = seed;
        }

        /// Returns the next value in [0, 32767].
        inline int next() {
            m_state = m_state * 214013u + 2531011u;
            return int((m_state >> 16) & 0x7FFF);
        }

    private:
        std::uint32_t m_state = 1;
    };

}

#endif // MORELOADER_CRT_RANDOM_H
