#include "WinAPI_p.h"

#include <malloc.h>
#include <sys/mman.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <moreloader/Image/MappedImage.h>
#include <moreloader/Runtime/GuestLayout.h>
#include <moreloader/Runtime/GuestThread.h>
#include <moreloader/Runtime/Process.h>

namespace more::loader::winapi {

    namespace {

        enum MemoryState : DWORD {
            MemCommit = 0x1000,
            MemFree = 0x10000,
        };

        enum MemoryType : DWORD {
            MemPrivate = 0x20000,
            MemImage = 0x1000000,
        };

        constexpr std::uint32_t pageSize = 0x1000;

        // The handle of every heap. It is an address that no mapping occupies.
        constexpr HANDLE heapHandle = 0x7D000000;

        constexpr DWORD heapReallocInPlaceOnly = 0x10;

        DWORD protectionOfPermissions(const char *permissions) {
            bool read = permissions[0] == 'r';
            bool write = permissions[1] == 'w';
            bool execute = permissions[2] == 'x';
            if (execute) {
                return write ? PageExecuteReadWrite : (read ? PageExecuteRead : PageExecute);
            }
            if (write) {
                return PageReadWrite;
            }
            return read ? PageReadOnly : PageNoAccess;
        }

        // Describes a region outside the image from the mappings of the host process.
        void describeHostRegion(std::uint32_t address, MEMORY_BASIC_INFORMATION32 &info) {
            std::uint32_t page = address & ~(pageSize - 1);
            info.BaseAddress = page;
            info.AllocationBase = 0;
            info.AllocationProtect = 0;
            info.RegionSize = pageSize;
            info.State = MemFree;
            info.Protect = PageNoAccess;
            info.Type = 0;

            FILE *maps = std::fopen("/proc/self/maps", "r");
            if (!maps) {
                return;
            }
            char line[512];
            while (std::fgets(line, sizeof line, maps)) {
                unsigned long start, end;
                char permissions[8] = {};
                if (std::sscanf(line, "%lx-%lx %7s", &start, &end, permissions) != 3) {
                    continue;
                }
                if (address >= start && address < end) {
                    info.BaseAddress = page;
                    info.AllocationBase = std::uint32_t(start);
                    info.AllocationProtect = protectionOfPermissions(permissions);
                    info.RegionSize = std::uint32_t(end - page);
                    info.State = MemCommit;
                    info.Protect = info.AllocationProtect;
                    info.Type = MemPrivate;
                    break;
                }
            }
            std::fclose(maps);
        }

        int hostProtection(DWORD protection) {
            switch (protection & 0xFF) {
                case PageNoAccess:
                    return PROT_NONE;
                case PageReadOnly:
                    return PROT_READ;
                case PageReadWrite:
                case PageWriteCopy:
                    return PROT_READ | PROT_WRITE;
                case PageExecute:
                case PageExecuteRead:
                    return PROT_READ | PROT_EXEC;
                default:
                    return PROT_READ | PROT_WRITE | PROT_EXEC;
            }
        }

        /// Describes the pages at \a address. The pseudo-relocation code of MinGW queries the
        /// sections of the image before it makes them writable, therefore the image is
        /// described from the protections recorded by the loader.
        DWORD MORE_WINAPI kernel32_VirtualQuery(const void *address,
                                                MEMORY_BASIC_INFORMATION32 *buffer,
                                                DWORD length) {
            if (length < sizeof(MEMORY_BASIC_INFORMATION32)) {
                setLastError(ErrorInsufficientBuffer);
                return 0;
            }
            auto value = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(address));
            MappedImage &image = process().image();
            MEMORY_BASIC_INFORMATION32 info{};
            if (auto region = image.region(value)) {
                info.BaseAddress = value & ~(pageSize - 1);
                info.AllocationBase = image.base();
                info.AllocationProtect = PageExecuteWriteCopy;
                info.RegionSize = region->address + region->size - info.BaseAddress;
                info.State = MemCommit;
                info.Protect = region->protection;
                info.Type = MemImage;
            } else {
                describeHostRegion(value, info);
            }
            std::memcpy(buffer, &info, sizeof info);
            return sizeof info;
        }

        BOOL MORE_WINAPI kernel32_VirtualProtect(void *address, DWORD size, DWORD protection,
                                                 DWORD *oldProtection) {
            auto value = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(address));
            MappedImage &image = process().image();
            if (image.contains(value)) {
                auto previous = image.changeProtection(value, size, protection);
                if (!previous) {
                    setLastError(ErrorInvalidParameter);
                    return FALSE;
                }
                if (oldProtection) {
                    *oldProtection = *previous;
                }
                return TRUE;
            }

            std::uint32_t first = value & ~(pageSize - 1);
            std::uint32_t last = (value + size + pageSize - 1) & ~(pageSize - 1);
            MEMORY_BASIC_INFORMATION32 info{};
            describeHostRegion(value, info);
            if (::mprotect(reinterpret_cast<void *>(static_cast<std::uintptr_t>(first)),
                           last - first, hostProtection(protection)) != 0) {
                setLastError(ErrorInvalidParameter);
                return FALSE;
            }
            if (oldProtection) {
                *oldProtection = info.Protect;
            }
            return TRUE;
        }

        /// Returns a handle for a heap. All heaps of the guest share the allocator of the host,
        /// therefore the handle only identifies the heap in later calls.
        HANDLE MORE_WINAPI kernel32_HeapCreate(DWORD options, DWORD initialSize,
                                               DWORD maximumSize) {
            return heapHandle;
        }

        /// Succeeds without releasing the blocks, which the host allocator keeps. The guest
        /// destroys its heap only when it exits.
        BOOL MORE_WINAPI kernel32_HeapDestroy(HANDLE heap) {
            return TRUE;
        }

        /// Allocates a block that is filled with zeros regardless of \c HEAP_ZERO_MEMORY, so
        /// that a guest that reads a block before writing it behaves the same on every run.
        void *MORE_WINAPI kernel32_HeapAlloc(HANDLE heap, DWORD flags, DWORD size) {
            void *block = std::calloc(1, size ? size : 1);
            if (!block) {
                setLastError(ErrorNotEnoughMemory);
            }
            return block;
        }

        BOOL MORE_WINAPI kernel32_HeapFree(HANDLE heap, DWORD flags, void *block) {
            std::free(block);
            return TRUE;
        }

        /// Resizes a block. The bytes added to a block are filled with zeros, as in HeapAlloc.
        /// With \c HEAP_REALLOC_IN_PLACE_ONLY the block keeps its address or the call fails.
        void *MORE_WINAPI kernel32_HeapReAlloc(HANDLE heap, DWORD flags, void *block, DWORD size) {
            std::size_t oldSize = ::malloc_usable_size(block);
            if (flags & heapReallocInPlaceOnly) {
                if (size > oldSize) {
                    setLastError(ErrorNotEnoughMemory);
                    return nullptr;
                }
                return block;
            }
            auto resized = static_cast<char *>(std::realloc(block, size ? size : 1));
            if (!resized) {
                setLastError(ErrorNotEnoughMemory);
                return nullptr;
            }
            std::size_t newSize = ::malloc_usable_size(resized);
            if (newSize > oldSize) {
                std::memset(resized + oldSize, 0, newSize - oldSize);
            }
            return resized;
        }
    }

    void registerKernel32Memory(ExportRegistry &registry) {
        MORE_REGISTER(registry, kernel32, VirtualQuery);
        MORE_REGISTER(registry, kernel32, VirtualProtect);
        MORE_REGISTER(registry, kernel32, HeapCreate);
        MORE_REGISTER(registry, kernel32, HeapDestroy);
        MORE_REGISTER(registry, kernel32, HeapAlloc);
        MORE_REGISTER(registry, kernel32, HeapFree);
        MORE_REGISTER(registry, kernel32, HeapReAlloc);
    }

}
