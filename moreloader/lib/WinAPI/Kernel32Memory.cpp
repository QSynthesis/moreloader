#include "WinAPI_p.h"

#include <sys/mman.h>

#include <cstdio>
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

    }

    void registerKernel32Memory(ExportRegistry &registry) {
        MORE_REGISTER(registry, kernel32, VirtualQuery);
        MORE_REGISTER(registry, kernel32, VirtualProtect);
    }

}
