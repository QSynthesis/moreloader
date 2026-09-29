#include "GuestHeap_p.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>

#include <moreloader/Support/Diagnostics.h>

namespace more::loader::msvcrt {

    namespace {

        // The header keeps the returned address aligned to 16 bytes, as the host allocator
        // aligns its blocks.
        struct BlockHeader {
            std::uint32_t size;
            std::uint32_t caller;
            std::uint32_t magic;
            std::uint32_t reserved;
        };
        static_assert(sizeof(BlockHeader) == 16);

        constexpr std::uint32_t blockMagic = 0x4D4C4850; // "PHLM"
        constexpr std::size_t guardSize = 16;
        constexpr unsigned char guardByte = 0xFD;

        bool isChecked() {
            static const bool checked = isDiagnosticEnabled(DiagnosticCategory::HeapCheck);
            return checked;
        }

        std::uint32_t addressOf(const void *p) {
            return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p));
        }

        BlockHeader *headerOf(void *block) {
            return reinterpret_cast<BlockHeader *>(static_cast<char *>(block) -
                                                   sizeof(BlockHeader));
        }

        // Writes the header and the guard bytes of a block of the checked heap.
        void *prepare(void *base, std::size_t size, const void *caller) {
            auto header = static_cast<BlockHeader *>(base);
            header->size = static_cast<std::uint32_t>(size);
            header->caller = addressOf(caller);
            header->magic = blockMagic;
            header->reserved = 0;
            auto data = static_cast<unsigned char *>(base) + sizeof(BlockHeader);
            std::memset(data + size, guardByte, guardSize);
            return data;
        }

        // Reports a block whose header or guard bytes were overwritten.
        void check(void *block, const char *operation, const void *caller) {
            BlockHeader *header = headerOf(block);
            if (header->magic != blockMagic) {
                diagnostic("heap check: %s of 0x%08x at 0x%08x: not a block of the msvcrt heap, "
                           "or its header was overwritten",
                           operation, addressOf(block), addressOf(caller));
                return;
            }
            auto guard = static_cast<unsigned char *>(block) + header->size;
            for (std::size_t i = 0; i < guardSize; ++i) {
                if (guard[i] != guardByte) {
                    std::size_t last = i;
                    for (std::size_t j = i; j < guardSize; ++j) {
                        if (guard[j] != guardByte) {
                            last = j;
                        }
                    }
                    diagnostic("heap check: %s of 0x%08x at 0x%08x: block of %u bytes allocated "
                               "at 0x%08x was written %u to %u bytes beyond its end",
                               operation, addressOf(block), addressOf(caller),
                               unsigned(header->size), unsigned(header->caller), unsigned(i + 1),
                               unsigned(last + 1));
                    return;
                }
            }
        }

    }

    void *guestAllocate(std::size_t size, bool zero, const void *caller) {
        if (!isChecked()) {
            return zero ? std::calloc(1, size) : std::malloc(size);
        }
        void *base = zero ? std::calloc(1, sizeof(BlockHeader) + size + guardSize)
                          : std::malloc(sizeof(BlockHeader) + size + guardSize);
        return base ? prepare(base, size, caller) : nullptr;
    }

    void *guestReallocate(void *block, std::size_t size, const void *caller) {
        if (!isChecked()) {
            return std::realloc(block, size);
        }
        if (!block) {
            return guestAllocate(size, false, caller);
        }
        check(block, "realloc", caller);
        void *base = std::realloc(headerOf(block), sizeof(BlockHeader) + size + guardSize);
        return base ? prepare(base, size, caller) : nullptr;
    }

    void guestFree(void *block, const void *caller) {
        if (!isChecked()) {
            std::free(block);
            return;
        }
        if (!block) {
            return;
        }
        check(block, "free", caller);
        std::free(headerOf(block));
    }

}
