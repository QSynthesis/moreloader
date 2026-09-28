#ifndef MORELOADER_IMAGE_PEFILE_H
#define MORELOADER_IMAGE_PEFILE_H

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace more::loader {

    /// Section header of a PE file.
    struct PESection {
        std::string name;
        std::uint32_t virtualAddress = 0;
        std::uint32_t virtualSize = 0;
        std::uint32_t rawOffset = 0;
        std::uint32_t rawSize = 0;
        std::uint32_t characteristics = 0;
    };

    /// Section characteristics that determine the protection of a mapped section.
    enum SectionCharacteristic : std::uint32_t {
        SectionExecute = 0x20000000,
        SectionRead = 0x40000000,
        SectionWrite = 0x80000000,
    };

    /// One imported function or variable.
    struct PEImport {
        /// Name of the library as written in the import directory, such as \c KERNEL32.dll.
        std::string library;

        /// Name of the import, empty if imported by ordinal.
        std::string name;

        std::uint16_t ordinal = 0;

        /// Relative virtual address of the slot in the import address table.
        std::uint32_t slotRVA = 0;
    };

    /// TLS directory of a PE file. Addresses are virtual addresses, not relative ones.
    struct PETLSDirectory {
        std::uint32_t startOfRawData = 0;
        std::uint32_t endOfRawData = 0;
        std::uint32_t indexAddress = 0;
        std::uint32_t callbacksAddress = 0;
        std::uint32_t sizeOfZeroFill = 0;

        /// Virtual addresses of the callbacks, read from the null-terminated array at
        /// \a callbacksAddress.
        std::vector<std::uint32_t> callbacks;
    };

    /// A parsed 32-bit PE file, read entirely into memory.
    ///
    /// Only the parts that a loader of an executable without relocations requires are parsed:
    /// the headers, the sections, the import directory and the TLS directory. Imports are
    /// resolved by the caller, therefore bound and delay-load imports are rejected.
    class PEFile {
    public:
        /// Reads and parses the file at the host path \a path.
        ///
        /// \return the parsed file, or \c std::nullopt with a description in \a errorMessage
        static std::optional<PEFile> read(const std::string &path, std::string &errorMessage);

        /// Parses \a data as a PE file.
        static std::optional<PEFile> parse(std::vector<std::uint8_t> data,
                                           std::string &errorMessage);

        inline std::uint32_t imageBase() const {
            return m_imageBase;
        }

        inline std::uint32_t sizeOfImage() const {
            return m_sizeOfImage;
        }

        inline std::uint32_t sizeOfHeaders() const {
            return m_sizeOfHeaders;
        }

        inline std::uint32_t entryPointRVA() const {
            return m_entryPointRVA;
        }

        inline std::uint32_t stackReserve() const {
            return m_stackReserve;
        }

        inline std::uint16_t characteristics() const {
            return m_characteristics;
        }

        inline std::uint16_t subsystem() const {
            return m_subsystem;
        }

        /// Returns whether the file has base relocations. An image without them must be mapped
        /// at its preferred base.
        inline bool hasRelocations() const {
            return m_hasRelocations;
        }

        inline const std::vector<PESection> &sections() const {
            return m_sections;
        }

        inline const std::vector<PEImport> &imports() const {
            return m_imports;
        }

        inline const std::optional<PETLSDirectory> &tls() const {
            return m_tls;
        }

        inline const std::vector<std::uint8_t> &data() const {
            return m_data;
        }

        /// Returns the file offset of the relative virtual address \a rva, provided that
        /// \a size bytes from there lie within the raw data of one section or the headers.
        std::optional<std::uint32_t> fileOffset(std::uint32_t rva, std::uint32_t size) const;

    private:
        std::vector<std::uint8_t> m_data;
        std::uint32_t m_imageBase = 0;
        std::uint32_t m_sizeOfImage = 0;
        std::uint32_t m_sizeOfHeaders = 0;
        std::uint32_t m_entryPointRVA = 0;
        std::uint32_t m_stackReserve = 0;
        std::uint16_t m_characteristics = 0;
        std::uint16_t m_subsystem = 0;
        bool m_hasRelocations = false;
        std::vector<PESection> m_sections;
        std::vector<PEImport> m_imports;
        std::optional<PETLSDirectory> m_tls;
    };

}

#endif // MORELOADER_IMAGE_PEFILE_H
