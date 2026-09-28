#include "PEFile.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <iterator>

namespace more::loader {

    namespace {

        constexpr std::uint16_t machineI386 = 0x14C;
        constexpr std::uint16_t optionalHeaderMagic32 = 0x10B;
        constexpr std::uint16_t fileRelocsStripped = 0x0001;

        enum DirectoryIndex {
            DirectoryImport = 1,
            DirectoryBaseRelocation = 5,
            DirectoryTLS = 9,
            DirectoryBoundImport = 11,
            DirectoryDelayImport = 13,
        };

        // Bounds-checked little-endian reads from the file data.
        class Reader {
        public:
            explicit Reader(const std::vector<std::uint8_t> &data) : m_data(data) {
            }

            bool has(std::uint64_t offset, std::uint64_t size) const {
                return offset + size <= m_data.size();
            }

            std::uint16_t u16(std::uint32_t offset) const {
                std::uint16_t v;
                std::memcpy(&v, m_data.data() + offset, sizeof v);
                return v;
            }

            std::uint32_t u32(std::uint32_t offset) const {
                std::uint32_t v;
                std::memcpy(&v, m_data.data() + offset, sizeof v);
                return v;
            }

        private:
            const std::vector<std::uint8_t> &m_data;
        };

    }

    std::optional<PEFile> PEFile::read(const std::string &path, std::string &errorMessage) {
        std::ifstream in(path, std::ios::binary);
        if (!in) {
            errorMessage = "cannot open " + path;
            return std::nullopt;
        }
        std::vector<std::uint8_t> data((std::istreambuf_iterator<char>(in)),
                                       std::istreambuf_iterator<char>());
        return parse(std::move(data), errorMessage);
    }

    std::optional<PEFile> PEFile::parse(std::vector<std::uint8_t> data,
                                        std::string &errorMessage) {
        PEFile file;
        file.m_data = std::move(data);
        Reader r(file.m_data);

        if (!r.has(0, 0x40) || r.u16(0) != 0x5A4D) {
            errorMessage = "not an MZ executable";
            return std::nullopt;
        }
        std::uint32_t peOffset = r.u32(0x3C);
        if (!r.has(peOffset, 24) || r.u32(peOffset) != 0x00004550) {
            errorMessage = "no PE signature";
            return std::nullopt;
        }

        std::uint32_t fileHeader = peOffset + 4;
        if (r.u16(fileHeader) != machineI386) {
            errorMessage = "not an i386 image";
            return std::nullopt;
        }
        std::uint16_t sectionCount = r.u16(fileHeader + 2);
        std::uint16_t optionalHeaderSize = r.u16(fileHeader + 16);
        file.m_characteristics = r.u16(fileHeader + 18);

        std::uint32_t optionalHeader = fileHeader + 20;
        if (!r.has(optionalHeader, optionalHeaderSize) || optionalHeaderSize < 96 ||
            r.u16(optionalHeader) != optionalHeaderMagic32) {
            errorMessage = "not a PE32 optional header";
            return std::nullopt;
        }
        file.m_entryPointRVA = r.u32(optionalHeader + 16);
        file.m_imageBase = r.u32(optionalHeader + 28);
        file.m_sizeOfImage = r.u32(optionalHeader + 56);
        file.m_sizeOfHeaders = r.u32(optionalHeader + 60);
        file.m_subsystem = r.u16(optionalHeader + 68);
        file.m_stackReserve = r.u32(optionalHeader + 72);
        std::uint32_t directoryCount = r.u32(optionalHeader + 92);

        auto directory = [&](int index) -> std::pair<std::uint32_t, std::uint32_t> {
            if (std::uint32_t(index) >= directoryCount ||
                96 + 8 * std::uint32_t(index) + 8 > optionalHeaderSize) {
                return {0, 0};
            }
            std::uint32_t entry = optionalHeader + 96 + 8 * std::uint32_t(index);
            return {r.u32(entry), r.u32(entry + 4)};
        };

        file.m_hasRelocations = !(file.m_characteristics & fileRelocsStripped) &&
                                directory(DirectoryBaseRelocation).second != 0;
        if (directory(DirectoryBoundImport).second != 0 ||
            directory(DirectoryDelayImport).second != 0) {
            errorMessage = "bound and delay-load imports are not supported";
            return std::nullopt;
        }

        std::uint32_t sectionTable = optionalHeader + optionalHeaderSize;
        if (!r.has(sectionTable, std::uint64_t(sectionCount) * 40)) {
            errorMessage = "truncated section table";
            return std::nullopt;
        }
        for (std::uint16_t i = 0; i < sectionCount; ++i) {
            std::uint32_t header = sectionTable + 40 * i;
            PESection section;
            const char *name = reinterpret_cast<const char *>(file.m_data.data() + header);
            section.name.assign(name, strnlen(name, 8));
            section.virtualSize = r.u32(header + 8);
            section.virtualAddress = r.u32(header + 12);
            section.rawSize = r.u32(header + 16);
            section.rawOffset = r.u32(header + 20);
            section.characteristics = r.u32(header + 36);
            if (section.rawSize != 0 && !r.has(section.rawOffset, section.rawSize)) {
                errorMessage = "section " + section.name + " lies beyond the end of the file";
                return std::nullopt;
            }
            if (std::uint64_t(section.virtualAddress) +
                    std::max(section.virtualSize, section.rawSize) >
                file.m_sizeOfImage) {
                errorMessage = "section " + section.name + " lies beyond the image";
                return std::nullopt;
            }
            file.m_sections.push_back(section);
        }

        // Import directory: an array of 20-byte descriptors terminated by a zeroed one.
        auto [importRVA, importSize] = directory(DirectoryImport);
        if (importRVA != 0) {
            for (std::uint32_t d = importRVA;; d += 20) {
                auto descriptor = file.fileOffset(d, 20);
                if (!descriptor) {
                    errorMessage = "invalid import directory";
                    return std::nullopt;
                }
                std::uint32_t lookupRVA = r.u32(*descriptor);
                std::uint32_t nameRVA = r.u32(*descriptor + 12);
                std::uint32_t slotRVA = r.u32(*descriptor + 16);
                if (nameRVA == 0 && slotRVA == 0) {
                    break;
                }
                auto nameOffset = file.fileOffset(nameRVA, 1);
                if (!nameOffset) {
                    errorMessage = "invalid import library name";
                    return std::nullopt;
                }
                std::string library(reinterpret_cast<const char *>(file.m_data.data() +
                                                                   *nameOffset));
                // The lookup table is the original first thunk if present, otherwise the
                // address table itself.
                std::uint32_t tableRVA = lookupRVA != 0 ? lookupRVA : slotRVA;
                for (std::uint32_t i = 0;; ++i) {
                    auto entryOffset = file.fileOffset(tableRVA + 4 * i, 4);
                    if (!entryOffset) {
                        errorMessage = "invalid import lookup table of " + library;
                        return std::nullopt;
                    }
                    std::uint32_t entry = r.u32(*entryOffset);
                    if (entry == 0) {
                        break;
                    }
                    PEImport import;
                    import.library = library;
                    import.slotRVA = slotRVA + 4 * i;
                    if (entry & 0x80000000u) {
                        import.ordinal = std::uint16_t(entry & 0xFFFF);
                    } else {
                        auto hintOffset = file.fileOffset(entry, 3);
                        if (!hintOffset) {
                            errorMessage = "invalid import name in " + library;
                            return std::nullopt;
                        }
                        import.ordinal = r.u16(*hintOffset);
                        import.name = reinterpret_cast<const char *>(file.m_data.data() +
                                                                     *hintOffset + 2);
                    }
                    file.m_imports.push_back(std::move(import));
                }
            }
        }

        // TLS directory of 24 bytes with virtual addresses.
        auto [tlsRVA, tlsSize] = directory(DirectoryTLS);
        if (tlsRVA != 0) {
            auto tlsOffset = file.fileOffset(tlsRVA, 24);
            if (!tlsOffset) {
                errorMessage = "invalid TLS directory";
                return std::nullopt;
            }
            PETLSDirectory tls;
            tls.startOfRawData = r.u32(*tlsOffset);
            tls.endOfRawData = r.u32(*tlsOffset + 4);
            tls.indexAddress = r.u32(*tlsOffset + 8);
            tls.callbacksAddress = r.u32(*tlsOffset + 12);
            tls.sizeOfZeroFill = r.u32(*tlsOffset + 16);
            if (tls.callbacksAddress != 0) {
                for (std::uint32_t va = tls.callbacksAddress;; va += 4) {
                    auto offset = file.fileOffset(va - file.m_imageBase, 4);
                    if (!offset) {
                        errorMessage = "invalid TLS callback array";
                        return std::nullopt;
                    }
                    std::uint32_t callback = r.u32(*offset);
                    if (callback == 0) {
                        break;
                    }
                    tls.callbacks.push_back(callback);
                }
            }
            file.m_tls = tls;
        }

        return file;
    }

    std::optional<std::uint32_t> PEFile::fileOffset(std::uint32_t rva, std::uint32_t size) const {
        if (std::uint64_t(rva) + size <= m_sizeOfHeaders) {
            return rva;
        }
        for (const PESection &section : m_sections) {
            if (rva >= section.virtualAddress &&
                std::uint64_t(rva) + size <= std::uint64_t(section.virtualAddress) +
                                                 section.rawSize) {
                return section.rawOffset + (rva - section.virtualAddress);
            }
        }
        return std::nullopt;
    }

}
