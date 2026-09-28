#ifndef MORELOADER_IMAGE_MAPPEDIMAGE_H
#define MORELOADER_IMAGE_MAPPEDIMAGE_H

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <moreloader/Image/PEFile.h>

namespace more::loader {

    /// Windows page protection constants, as returned by \c VirtualQuery.
    enum PageProtection : std::uint32_t {
        PageNoAccess = 0x01,
        PageReadOnly = 0x02,
        PageReadWrite = 0x04,
        PageWriteCopy = 0x08,
        PageExecute = 0x10,
        PageExecuteRead = 0x20,
        PageExecuteReadWrite = 0x40,
        PageExecuteWriteCopy = 0x80,
    };

    /// A range of pages of the image with one protection.
    struct ImageRegion {
        std::uint32_t address = 0;
        std::uint32_t size = 0;
        std::uint32_t protection = PageReadOnly;
    };

    /// An image mapped at its preferred base address.
    ///
    /// The image is never unmapped, because the guest runs until the process exits.
    class MappedImage {
    public:
        /// Reserves the address range of the image, copies the headers and the sections into
        /// it, and leaves every page writable until protect() is called.
        ///
        /// \return the mapping, or \c std::nullopt with a description in \a errorMessage if the
        ///         range is occupied or the image has relocations, which are not supported
        static std::optional<MappedImage> map(const PEFile &file, std::string &errorMessage);

        inline std::uint32_t base() const {
            return m_base;
        }

        inline std::uint32_t size() const {
            return m_size;
        }

        /// Returns whether \a address lies within the image.
        inline bool contains(std::uint32_t address) const {
            return address >= m_base && address - m_base < m_size;
        }

        /// Stores \a value at the relative virtual address \a rva.
        void write32(std::uint32_t rva, std::uint32_t value);

        /// Applies the protection of each section, as the Windows loader does after resolving
        /// the imports. The headers are read-only.
        bool protect(std::string &errorMessage);

        /// Changes the protection of the pages in [\a address, \a address + \a size) to the
        /// Windows protection \a protection and records it.
        ///
        /// \return the previous protection of the first page, or \c std::nullopt if the range
        ///         does not lie within the image or the host refuses the change
        std::optional<std::uint32_t> changeProtection(std::uint32_t address, std::uint32_t size,
                                                      std::uint32_t protection);

        /// Returns the largest region that contains \a address and has one protection.
        std::optional<ImageRegion> region(std::uint32_t address) const;

    private:
        std::uint32_t m_base = 0;
        std::uint32_t m_size = 0;

        // One protection per page, as a Windows constant.
        std::vector<std::uint32_t> m_pageProtection;

        // The protection of the headers and of each section, applied by protect().
        std::vector<ImageRegion> m_intended;
    };

    /// Converts the section characteristics \a characteristics to a Windows page protection.
    std::uint32_t protectionOfSection(std::uint32_t characteristics);

}

#endif // MORELOADER_IMAGE_MAPPEDIMAGE_H
