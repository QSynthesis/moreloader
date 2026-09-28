#include "LDT.h"

#include <asm/ldt.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <bitset>
#include <mutex>

namespace more::loader {

    namespace {

        // Linux permits 8192 entries. Entry 0 is left unused.
        constexpr std::size_t entryCount = 8192;

        std::mutex s_mutex;
        std::bitset<entryCount> s_used;

        bool writeEntry(const user_desc &descriptor) {
            return ::syscall(SYS_modify_ldt, 1, &descriptor, sizeof descriptor) == 0;
        }

        std::uint16_t selectorOf(unsigned entry) {
            // Table indicator 1 selects the LDT, and requested privilege level 3 is user mode.
            return static_cast<std::uint16_t>((entry << 3) | 4 | 3);
        }

    }

    std::optional<std::uint16_t> allocateDataSegment(std::uint32_t base, std::uint32_t limit) {
        std::lock_guard<std::mutex> lock(s_mutex);
        for (unsigned entry = 1; entry < entryCount; ++entry) {
            if (s_used[entry]) {
                continue;
            }
            user_desc descriptor{};
            descriptor.entry_number = entry;
            descriptor.base_addr = base;
            descriptor.limit = limit;
            descriptor.seg_32bit = 1;
            descriptor.contents = 0; // data, expand-up
            descriptor.read_exec_only = 0;
            descriptor.limit_in_pages = 0;
            descriptor.seg_not_present = 0;
            descriptor.useable = 1;
            if (!writeEntry(descriptor)) {
                return std::nullopt;
            }
            s_used[entry] = true;
            return selectorOf(entry);
        }
        return std::nullopt;
    }

    void releaseDataSegment(std::uint16_t selector) {
        unsigned entry = selector >> 3;
        std::lock_guard<std::mutex> lock(s_mutex);
        if (entry == 0 || entry >= entryCount || !s_used[entry]) {
            return;
        }
        // The empty descriptor of the kernel: no base, no limit, not present.
        user_desc descriptor{};
        descriptor.entry_number = entry;
        descriptor.read_exec_only = 1;
        descriptor.seg_not_present = 1;
        writeEntry(descriptor);
        s_used[entry] = false;
    }

    void loadFS(std::uint16_t selector) {
        asm volatile("movw %0, %%fs" : : "r"(selector));
    }

}
