#include "MappedImage.h"

#include <sys/mman.h>

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>

namespace more::loader {

    namespace {

        constexpr std::uint32_t pageSize = 0x1000;

        std::uint32_t alignUp(std::uint32_t value) {
            return (value + pageSize - 1) & ~(pageSize - 1);
        }

        std::string hexAddress(std::uint32_t address) {
            char buffer[16];
            std::snprintf(buffer, sizeof buffer, "0x%08x", address);
            return buffer;
        }

        int hostProtection(std::uint32_t protection) {
            switch (protection) {
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

    }

    std::uint32_t protectionOfSection(std::uint32_t characteristics) {
        bool execute = characteristics & SectionExecute;
        bool write = characteristics & SectionWrite;
        if (execute) {
            return write ? PageExecuteReadWrite : PageExecuteRead;
        }
        return write ? PageReadWrite : PageReadOnly;
    }

    std::optional<MappedImage> MappedImage::map(const PEFile &file, std::string &errorMessage) {
        if (file.hasRelocations()) {
            // Relocation is not implemented, because moresampler has none and must be mapped at
            // its preferred base in any case.
            errorMessage = "images with relocations are not supported";
            return std::nullopt;
        }

        MappedImage image;
        image.m_base = file.imageBase();
        image.m_size = alignUp(file.sizeOfImage());

        // MAP_FIXED_NOREPLACE fails instead of replacing an existing mapping. A kernel older
        // than Linux 4.17 treats it as a hint, which the address comparison detects.
        void *wanted = reinterpret_cast<void *>(static_cast<std::uintptr_t>(image.m_base));
        void *mapped = ::mmap(wanted, image.m_size, PROT_READ | PROT_WRITE | PROT_EXEC,
                              MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        if (mapped == MAP_FAILED || mapped != wanted) {
            if (mapped != MAP_FAILED) {
                ::munmap(mapped, image.m_size);
            }
            errorMessage = "cannot reserve the image range at " +
                           hexAddress(image.m_base) + ": " + std::strerror(errno);
            return std::nullopt;
        }

        auto base = static_cast<std::uint8_t *>(mapped);
        const auto &data = file.data();
        std::memcpy(base, data.data(), std::min<std::size_t>(file.sizeOfHeaders(), data.size()));
        // The headers are read-only, as are pages between sections.
        image.m_pageProtection.assign(image.m_size / pageSize, PageReadWrite);
        image.m_intended.push_back({image.m_base, alignUp(file.sizeOfHeaders()), PageReadOnly});

        for (const PESection &section : file.sections()) {
            // Raw data beyond the virtual size is file alignment padding.
            std::uint32_t size = section.rawSize;
            if (section.virtualSize != 0 && section.virtualSize < size) {
                size = section.virtualSize;
            }
            std::memcpy(base + section.virtualAddress, data.data() + section.rawOffset, size);

            std::uint32_t extent = alignUp(std::max(section.virtualSize, section.rawSize));
            if (extent != 0) {
                image.m_intended.push_back({image.m_base + section.virtualAddress, extent,
                                            protectionOfSection(section.characteristics)});
            }
        }
        return image;
    }

    void MappedImage::write32(std::uint32_t rva, std::uint32_t value) {
        std::memcpy(reinterpret_cast<void *>(static_cast<std::uintptr_t>(m_base + rva)), &value,
                    sizeof value);
    }

    bool MappedImage::protect(std::string &errorMessage) {
        for (const ImageRegion &region : m_intended) {
            if (!changeProtection(region.address, region.size, region.protection)) {
                errorMessage = "cannot protect the pages at " + hexAddress(region.address) +
                               ": " + std::strerror(errno);
                return false;
            }
        }
        return true;
    }

    std::optional<std::uint32_t> MappedImage::changeProtection(std::uint32_t address,
                                                               std::uint32_t size,
                                                               std::uint32_t protection) {
        if (size == 0 || !contains(address) || address - m_base + std::uint64_t(size) > m_size) {
            return std::nullopt;
        }
        std::uint32_t first = (address - m_base) / pageSize;
        std::uint32_t last = (address - m_base + size - 1) / pageSize;
        void *start = reinterpret_cast<void *>(static_cast<std::uintptr_t>(m_base + first * pageSize));
        if (::mprotect(start, (last - first + 1) * pageSize, hostProtection(protection)) != 0) {
            return std::nullopt;
        }
        std::uint32_t previous = m_pageProtection[first];
        for (std::uint32_t page = first; page <= last; ++page) {
            m_pageProtection[page] = protection;
        }
        return previous;
    }

    std::optional<ImageRegion> MappedImage::region(std::uint32_t address) const {
        if (!contains(address)) {
            return std::nullopt;
        }
        std::uint32_t page = (address - m_base) / pageSize;
        std::uint32_t protection = m_pageProtection[page];
        std::uint32_t first = page;
        while (first > 0 && m_pageProtection[first - 1] == protection) {
            --first;
        }
        std::uint32_t last = page;
        while (last + 1 < m_pageProtection.size() && m_pageProtection[last + 1] == protection) {
            ++last;
        }
        ImageRegion region;
        region.address = m_base + first * pageSize;
        region.size = (last - first + 1) * pageSize;
        region.protection = protection;
        return region;
    }

}
