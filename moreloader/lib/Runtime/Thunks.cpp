#include "Thunks.h"

#include <sys/mman.h>

#include <cstring>
#include <mutex>

#include <moreloader/Support/Diagnostics.h>
#include <moreloader/Support/MoreLoaderSupportGlobal.h>

#include "GuestThread.h"

namespace more::loader {

    namespace {

        // Description of a generated thunk. Records are never freed.
        struct ThunkRecord {
            std::string library;
            std::string name;
        };

        [[noreturn]] void MORE_CDECL reportUnresolved(const ThunkRecord *record) {
            GuestThread *thread = GuestThread::current();
            fatal("thread %u called %s!%s, which is not implemented",
                  thread ? unsigned(thread->threadID()) : 0u, record->library.c_str(),
                  record->name.c_str());
        }

        void MORE_CDECL reportCall(const ThunkRecord *record) {
            GuestThread *thread = GuestThread::current();
            if (thread) {
                thread->setCurrentImport(record->name.c_str());
            }
            diagnostic("[%u] %s!%s", thread ? unsigned(thread->threadID()) : 0u,
                       record->library.c_str(), record->name.c_str());
        }

        // Executable memory for thunks, allocated in pages and never freed.
        class ThunkMemory {
        public:
            std::uint8_t *allocate(std::size_t size) {
                std::lock_guard<std::mutex> lock(m_mutex);
                if (m_used + size > m_capacity) {
                    m_capacity = 0x10000;
                    void *page = ::mmap(nullptr, m_capacity, PROT_READ | PROT_WRITE | PROT_EXEC,
                                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
                    if (page == MAP_FAILED) {
                        fatal("cannot allocate memory for thunks");
                    }
                    m_page = static_cast<std::uint8_t *>(page);
                    m_used = 0;
                }
                std::uint8_t *p = m_page + m_used;
                m_used += (size + 15) & ~std::size_t(15);
                return p;
            }

        private:
            std::mutex m_mutex;
            std::uint8_t *m_page = nullptr;
            std::size_t m_used = 0;
            std::size_t m_capacity = 0;
        };

        ThunkMemory &thunkMemory() {
            static ThunkMemory memory;
            return memory;
        }

        std::uint32_t addressOf(const void *p) {
            return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p));
        }

        // Emits the 32-bit relative displacement of a call or jump at \a p to \a target.
        void emitRelative(std::uint8_t *&p, std::uint32_t target) {
            std::uint32_t displacement = target - (addressOf(p) + 4);
            std::memcpy(p, &displacement, 4);
            p += 4;
        }

        void emit32(std::uint8_t *&p, std::uint32_t value) {
            std::memcpy(p, &value, 4);
            p += 4;
        }

    }

    std::uint32_t makeUnresolvedThunk(const std::string &library, const std::string &name) {
        auto record = new ThunkRecord{library, name};
        std::uint8_t *code = thunkMemory().allocate(16);
        std::uint8_t *p = code;
        *p++ = 0x68; // push imm32
        emit32(p, addressOf(record));
        *p++ = 0xE8; // call rel32
        emitRelative(p, addressOf(reinterpret_cast<const void *>(&reportUnresolved)));
        *p++ = 0xF4; // hlt, never reached
        return addressOf(code);
    }

    std::uint32_t makeTraceThunk(const std::string &library, const std::string &name,
                                 std::uint32_t target) {
        auto record = new ThunkRecord{library, name};
        std::uint8_t *code = thunkMemory().allocate(32);
        std::uint8_t *p = code;
        *p++ = 0x60; // pushad
        *p++ = 0x9C; // pushfd
        *p++ = 0x68; // push imm32
        emit32(p, addressOf(record));
        *p++ = 0xE8; // call rel32
        emitRelative(p, addressOf(reinterpret_cast<const void *>(&reportCall)));
        *p++ = 0x83; // add esp, 4
        *p++ = 0xC4;
        *p++ = 0x04;
        *p++ = 0x9D; // popfd
        *p++ = 0x61; // popad
        *p++ = 0xE9; // jmp rel32
        emitRelative(p, target);
        return addressOf(code);
    }

}
