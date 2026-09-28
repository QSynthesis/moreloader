#include "ThreadArea.h"

#include <asm/ldt.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <mutex>

namespace more::loader {

    namespace {

        std::mutex s_mutex;

        // The GDT entry of the TEB descriptor, or -1 before the first call.
        int s_entry = -1;

        std::uint16_t selectorOf(unsigned entry) {
            // Table indicator 0 selects the GDT, and requested privilege level 3 is user mode.
            return static_cast<std::uint16_t>((entry << 3) | 3);
        }

    }

    std::optional<std::uint16_t> setThreadArea(std::uint32_t base, std::uint32_t limit) {
        std::lock_guard<std::mutex> lock(s_mutex);
        user_desc descriptor{};
        // An entry number of -1 requests a free TLS entry, and the kernel stores the number of
        // the allocated entry into the descriptor.
        descriptor.entry_number = static_cast<unsigned>(s_entry);
        descriptor.base_addr = base;
        descriptor.limit = limit;
        descriptor.seg_32bit = 1;
        descriptor.contents = 0; // data, expand-up
        descriptor.read_exec_only = 0;
        descriptor.limit_in_pages = 0;
        descriptor.seg_not_present = 0;
        descriptor.useable = 1;
        if (::syscall(SYS_set_thread_area, &descriptor) != 0) {
            return std::nullopt;
        }
        s_entry = static_cast<int>(descriptor.entry_number);
        return selectorOf(descriptor.entry_number);
    }

    void loadFS(std::uint16_t selector) {
        asm volatile("movw %0, %%fs" : : "r"(selector));
    }

}
