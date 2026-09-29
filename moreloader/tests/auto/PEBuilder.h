#ifndef MORELOADER_TESTS_PEBUILDER_H
#define MORELOADER_TESTS_PEBUILDER_H

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <vector>

/// Construction of minimal 32-bit PE files for the tests of the image and the process.
namespace testing {

    /// Writes \a value in little-endian order at \a offset of \a data, growing it if necessary.
    inline void put32(std::vector<std::uint8_t> &data, std::size_t offset, std::uint32_t value) {
        if (data.size() < offset + 4) {
            data.resize(offset + 4);
        }
        std::memcpy(data.data() + offset, &value, 4);
    }

    inline void put16(std::vector<std::uint8_t> &data, std::size_t offset, std::uint16_t value) {
        if (data.size() < offset + 2) {
            data.resize(offset + 2);
        }
        std::memcpy(data.data() + offset, &value, 2);
    }

    inline void putBytes(std::vector<std::uint8_t> &data, std::size_t offset, const void *bytes,
                         std::size_t size) {
        if (data.size() < offset + size) {
            data.resize(offset + size);
        }
        std::memcpy(data.data() + offset, bytes, size);
    }

    /// A section of a PE file under construction. The raw data is \a data, padded to the file
    /// alignment.
    struct BuilderSection {
        std::string name;
        std::uint32_t rva = 0;
        std::uint32_t virtualSize = 0;
        std::vector<std::uint8_t> data;
        std::uint32_t characteristics = 0;
    };

    /// Section characteristics of code and of initialized data.
    constexpr std::uint32_t codeSection = 0x60000020;
    constexpr std::uint32_t dataSection = 0xC0000040;

    /// Indexes of the data directories.
    enum BuilderDirectory {
        ImportDirectory = 1,
        BaseRelocationDirectory = 5,
        TLSDirectory = 9,
        BoundImportDirectory = 11,
        DelayImportDirectory = 13,
    };

    /// A PE file under construction. The headers occupy 0x400 bytes, the file alignment is
    /// 0x200 and the section alignment 0x1000.
    struct PEBuilder {
        std::uint16_t machine = 0x14C;
        std::uint16_t magic = 0x10B;
        /// Relocations stripped, executable, 32-bit machine.
        std::uint16_t characteristics = 0x0103;
        std::uint16_t subsystem = 3;
        std::uint32_t imageBase = 0x10000000;
        std::uint32_t entryPointRVA = 0;
        std::uint32_t stackReserve = 0x200000;
        std::uint32_t directories[16][2] = {};
        std::vector<BuilderSection> sections;

        static constexpr std::uint32_t peOffset = 0x80;
        static constexpr std::uint32_t headersSize = 0x400;

        inline void setDirectory(int index, std::uint32_t rva, std::uint32_t size) {
            directories[index][0] = rva;
            directories[index][1] = size;
        }

        /// Returns the size of the image, which ends at the end of the last section rounded up
        /// to the section alignment.
        inline std::uint32_t sizeOfImage() const {
            std::uint32_t end = 0x1000;
            for (const BuilderSection &section : sections) {
                std::uint32_t extent = std::max<std::uint32_t>(section.virtualSize,
                                                               std::uint32_t(section.data.size()));
                end = std::max(end, section.rva + extent);
            }
            return (end + 0xFFF) & ~0xFFFu;
        }

        /// Returns the bytes of the file.
        inline std::vector<std::uint8_t> build() const {
            std::vector<std::uint8_t> file(headersSize, 0);
            put16(file, 0, 0x5A4D);
            put32(file, 0x3C, peOffset);
            put32(file, peOffset, 0x00004550);

            std::uint32_t fileHeader = peOffset + 4;
            put16(file, fileHeader, machine);
            put16(file, fileHeader + 2, std::uint16_t(sections.size()));
            put16(file, fileHeader + 16, 224);
            put16(file, fileHeader + 18, characteristics);

            std::uint32_t optionalHeader = fileHeader + 20;
            put16(file, optionalHeader, magic);
            put32(file, optionalHeader + 16, entryPointRVA);
            put32(file, optionalHeader + 28, imageBase);
            put32(file, optionalHeader + 32, 0x1000);
            put32(file, optionalHeader + 36, 0x200);
            put32(file, optionalHeader + 56, sizeOfImage());
            put32(file, optionalHeader + 60, headersSize);
            put16(file, optionalHeader + 68, subsystem);
            put32(file, optionalHeader + 72, stackReserve);
            put32(file, optionalHeader + 92, 16);
            for (int i = 0; i < 16; ++i) {
                put32(file, optionalHeader + 96 + 8 * i, directories[i][0]);
                put32(file, optionalHeader + 100 + 8 * i, directories[i][1]);
            }

            std::uint32_t sectionTable = optionalHeader + 224;
            std::uint32_t rawOffset = headersSize;
            for (std::size_t i = 0; i < sections.size(); ++i) {
                const BuilderSection &section = sections[i];
                std::uint32_t header = sectionTable + 40 * std::uint32_t(i);
                char name[8] = {};
                std::memcpy(name, section.name.data(),
                            std::min<std::size_t>(8, section.name.size()));
                putBytes(file, header, name, 8);
                put32(file, header + 8, section.virtualSize);
                put32(file, header + 12, section.rva);
                std::uint32_t rawSize = (std::uint32_t(section.data.size()) + 0x1FF) & ~0x1FFu;
                put32(file, header + 16, rawSize);
                put32(file, header + 20, rawSize ? rawOffset : 0);
                put32(file, header + 36, section.characteristics);
                if (rawSize) {
                    file.resize(rawOffset + rawSize, 0);
                    std::memcpy(file.data() + rawOffset, section.data.data(), section.data.size());
                    rawOffset += rawSize;
                }
            }
            return file;
        }
    };

    /// A library and the functions imported from it. A function written as \c #n is imported
    /// by the ordinal n.
    struct ImportLibrary {
        std::string name;
        std::vector<std::string> functions;
    };

    /// Result of placeImports().
    struct ImportPlacement {
        std::uint32_t directoryRVA = 0;
        std::uint32_t directorySize = 0;

        /// Relative virtual address of the slot in the import address table, by
        /// <tt>library!function</tt>.
        std::map<std::string, std::uint32_t> slots;
    };

    /// Writes an import directory for \a libraries into \a section from \a offset on. The hint
    /// of the n-th function of a library is n. If \a withLookupTable is false, the descriptors
    /// have no lookup table and the import address table serves as one.
    inline ImportPlacement placeImports(BuilderSection &section, std::uint32_t offset,
                                        const std::vector<ImportLibrary> &libraries,
                                        bool withLookupTable = true) {
        ImportPlacement placement;
        placement.directoryRVA = section.rva + offset;
        placement.directorySize = 20 * std::uint32_t(libraries.size() + 1);

        std::uint32_t cursor = offset + placement.directorySize;
        std::vector<std::uint32_t> lookupOffsets, slotOffsets;
        for (const ImportLibrary &library : libraries) {
            std::uint32_t tableSize = 4 * std::uint32_t(library.functions.size() + 1);
            lookupOffsets.push_back(cursor);
            cursor += tableSize;
            slotOffsets.push_back(cursor);
            cursor += tableSize;
        }

        for (std::size_t l = 0; l < libraries.size(); ++l) {
            const ImportLibrary &library = libraries[l];
            std::uint32_t descriptor = offset + 20 * std::uint32_t(l);
            std::uint32_t nameOffset = cursor;
            putBytes(section.data, nameOffset, library.name.c_str(), library.name.size() + 1);
            cursor += std::uint32_t(library.name.size() + 1);
            cursor = (cursor + 1) & ~1u;

            put32(section.data, descriptor, withLookupTable ? section.rva + lookupOffsets[l] : 0);
            put32(section.data, descriptor + 12, section.rva + nameOffset);
            put32(section.data, descriptor + 16, section.rva + slotOffsets[l]);

            for (std::size_t f = 0; f < library.functions.size(); ++f) {
                const std::string &function = library.functions[f];
                std::uint32_t entry;
                if (!function.empty() && function[0] == '#') {
                    entry = 0x80000000u | std::uint32_t(std::stoul(function.substr(1)));
                } else {
                    std::uint32_t hintOffset = cursor;
                    put16(section.data, hintOffset, std::uint16_t(f));
                    putBytes(section.data, hintOffset + 2, function.c_str(), function.size() + 1);
                    cursor += std::uint32_t(2 + function.size() + 1);
                    cursor = (cursor + 1) & ~1u;
                    entry = section.rva + hintOffset;
                }
                if (withLookupTable) {
                    put32(section.data, lookupOffsets[l] + 4 * std::uint32_t(f), entry);
                }
                put32(section.data, slotOffsets[l] + 4 * std::uint32_t(f), entry);
                placement.slots[library.name + "!" + function] =
                    section.rva + slotOffsets[l] + 4 * std::uint32_t(f);
            }
            if (withLookupTable) {
                put32(section.data, lookupOffsets[l] + 4 * std::uint32_t(library.functions.size()),
                      0);
            }
            put32(section.data, slotOffsets[l] + 4 * std::uint32_t(library.functions.size()), 0);
        }
        put32(section.data, offset + 20 * std::uint32_t(libraries.size()) + 16, 0);
        if (section.data.size() < cursor) {
            section.data.resize(cursor);
        }
        return placement;
    }

    /// Writes a TLS directory into \a section at \a offset, followed by the null-terminated array
    /// of \a callbacks. Returns the relative virtual address of the directory.
    inline std::uint32_t placeTLS(BuilderSection &section, std::uint32_t offset,
                                  std::uint32_t imageBase, std::uint32_t startOfRawData,
                                  std::uint32_t endOfRawData, std::uint32_t indexAddress,
                                  std::uint32_t sizeOfZeroFill,
                                  const std::vector<std::uint32_t> &callbacks) {
        std::uint32_t array = offset + 24;
        put32(section.data, offset, startOfRawData);
        put32(section.data, offset + 4, endOfRawData);
        put32(section.data, offset + 8, indexAddress);
        put32(section.data, offset + 12, imageBase + section.rva + array);
        put32(section.data, offset + 16, sizeOfZeroFill);
        put32(section.data, offset + 20, 0);
        for (std::size_t i = 0; i < callbacks.size(); ++i) {
            put32(section.data, array + 4 * std::uint32_t(i), callbacks[i]);
        }
        put32(section.data, array + 4 * std::uint32_t(callbacks.size()), 0);
        return section.rva + offset;
    }

}

#endif // MORELOADER_TESTS_PEBUILDER_H
