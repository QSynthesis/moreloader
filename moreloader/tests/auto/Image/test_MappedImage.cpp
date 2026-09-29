#include <cstdint>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>

#include <boost/test/unit_test.hpp>

#include <moreloader/Image/MappedImage.h>

#include "PEBuilder.h"

using namespace more::loader;
using namespace testing;

namespace {

    // An image of four pages: the headers, a code page whose raw data is longer than its virtual
    // size, and two data pages whose raw data is shorter than the virtual size.
    PEFile sampleFile(std::uint32_t base) {
        PEBuilder builder;
        builder.imageBase = base;
        std::vector<std::uint8_t> code(0x20, 0xC3);
        std::vector<std::uint8_t> data(0x300, 0xAB);
        builder.sections = {
            {".text", 0x1000, 0x10,   code, codeSection},
            {".data", 0x2000, 0x1800, data, dataSection}
        };
        std::string error;
        auto file = PEFile::parse(builder.build(), error);
        BOOST_REQUIRE_MESSAGE(file.has_value(), error);
        return *file;
    }

    const std::uint8_t *at(std::uint32_t address) {
        return reinterpret_cast<const std::uint8_t *>(static_cast<std::uintptr_t>(address));
    }

    // Returns the permissions, such as r-xp, of the host mapping that contains \a address.
    std::string hostPermissions(std::uint32_t address) {
        std::ifstream maps("/proc/self/maps");
        std::string line;
        while (std::getline(maps, line)) {
            std::istringstream fields(line);
            std::string range, permissions;
            fields >> range >> permissions;
            auto dash = range.find('-');
            std::uint64_t start = std::stoull(range.substr(0, dash), nullptr, 16);
            std::uint64_t end = std::stoull(range.substr(dash + 1), nullptr, 16);
            if (address >= start && address < end) {
                return permissions;
            }
        }
        return "";
    }

}

BOOST_AUTO_TEST_SUITE(test_MappedImage)

BOOST_AUTO_TEST_CASE(test_protection_of_section) {
    BOOST_TEST(protectionOfSection(0x60000020) == PageExecuteRead);
    BOOST_TEST(protectionOfSection(0xE0000020) == PageExecuteReadWrite);
    BOOST_TEST(protectionOfSection(0xC0000040) == PageReadWrite);
    BOOST_TEST(protectionOfSection(0x40000040) == PageReadOnly);
    // The read flag is not consulted. A section without the execute and write flags is read-only.
    BOOST_TEST(protectionOfSection(0) == PageReadOnly);
    BOOST_TEST(protectionOfSection(SectionExecute) == PageExecuteRead);
}

BOOST_AUTO_TEST_CASE(test_map_copies_headers_and_sections) {
    std::uint32_t base = 0x31000000;
    PEFile file = sampleFile(base);
    std::string error;
    auto image = MappedImage::map(file, error);
    BOOST_TEST_REQUIRE(image.has_value(), error);
    BOOST_TEST(image->base() == base);
    BOOST_TEST(image->size() == 0x4000u);
    BOOST_TEST(image->contains(base));
    BOOST_TEST(image->contains(base + 0x3FFF));
    BOOST_TEST(!image->contains(base + 0x4000));
    BOOST_TEST(!image->contains(base - 1));

    BOOST_TEST(std::memcmp(at(base), file.data().data(), 0x400) == 0);
    // Only the virtual size of the code section is copied from its padded raw data.
    BOOST_TEST(at(base + 0x1000)[0] == 0xC3);
    BOOST_TEST(at(base + 0x100F)[0] == 0xC3);
    BOOST_TEST(at(base + 0x1010)[0] == 0);
    // The data section is zero beyond its raw data.
    BOOST_TEST(at(base + 0x22FF)[0] == 0xAB);
    BOOST_TEST(at(base + 0x2400)[0] == 0);
    BOOST_TEST(at(base + 0x37FF)[0] == 0);

    image->write32(0x2004, 0x12345678);
    std::uint32_t value;
    std::memcpy(&value, at(base + 0x2004), 4);
    BOOST_TEST(value == 0x12345678u);
}

BOOST_AUTO_TEST_CASE(test_protect) {
    std::uint32_t base = 0x32000000;
    std::string error;
    auto image = MappedImage::map(sampleFile(base), error);
    BOOST_TEST_REQUIRE(image.has_value(), error);

    // Before protect(), the whole image is one writable region.
    auto region = image->region(base + 0x1000);
    BOOST_TEST_REQUIRE(region.has_value());
    BOOST_TEST(region->address == base);
    BOOST_TEST(region->size == 0x4000u);
    BOOST_TEST(region->protection == PageReadWrite);

    BOOST_TEST_REQUIRE(image->protect(error), error);
    region = image->region(base + 0x10);
    BOOST_TEST(region->address == base);
    BOOST_TEST(region->size == 0x1000u);
    BOOST_TEST(region->protection == PageReadOnly);
    region = image->region(base + 0x1234);
    BOOST_TEST(region->address == base + 0x1000);
    BOOST_TEST(region->size == 0x1000u);
    BOOST_TEST(region->protection == PageExecuteRead);
    region = image->region(base + 0x3FFF);
    BOOST_TEST(region->address == base + 0x2000);
    BOOST_TEST(region->size == 0x2000u);
    BOOST_TEST(region->protection == PageReadWrite);
    BOOST_TEST(!image->region(base + 0x4000).has_value());

    BOOST_TEST(hostPermissions(base).substr(0, 3) == "r--");
    BOOST_TEST(hostPermissions(base + 0x1000).substr(0, 3) == "r-x");
    BOOST_TEST(hostPermissions(base + 0x2000).substr(0, 3) == "rw-");
}

BOOST_AUTO_TEST_CASE(test_change_protection) {
    std::uint32_t base = 0x33000000;
    std::string error;
    auto image = MappedImage::map(sampleFile(base), error);
    BOOST_TEST_REQUIRE(image.has_value(), error);
    BOOST_TEST_REQUIRE(image->protect(error), error);

    // A range within one page changes the whole page, and the previous protection is that of
    // the first page.
    auto previous = image->changeProtection(base + 0x2010, 4, PageExecuteReadWrite);
    BOOST_TEST(previous.value_or(0) == PageReadWrite);
    auto region = image->region(base + 0x2000);
    BOOST_TEST(region->size == 0x1000u);
    BOOST_TEST(region->protection == PageExecuteReadWrite);
    BOOST_TEST(hostPermissions(base + 0x2000).substr(0, 3) == "rwx");
    BOOST_TEST(image->region(base + 0x3000)->protection == PageReadWrite);

    // A range across two pages.
    previous = image->changeProtection(base + 0x1FFF, 2, PageReadOnly);
    BOOST_TEST(previous.value_or(0) == PageExecuteRead);
    BOOST_TEST(image->region(base + 0x1000)->protection == PageReadOnly);
    BOOST_TEST(image->region(base + 0x2000)->protection == PageReadOnly);
    // The headers and the two pages now form one read-only region.
    BOOST_TEST(image->region(base)->size == 0x3000u);

    BOOST_TEST(!image->changeProtection(base + 0x3FFF, 2, PageReadOnly).has_value());
    BOOST_TEST(!image->changeProtection(base + 0x4000, 1, PageReadOnly).has_value());
    BOOST_TEST(!image->changeProtection(base, 0, PageReadOnly).has_value());
}

BOOST_AUTO_TEST_CASE(test_rejects_occupied_range) {
    std::uint32_t base = 0x34000000;
    PEFile file = sampleFile(base);
    std::string error;
    auto first = MappedImage::map(file, error);
    BOOST_TEST_REQUIRE(first.has_value(), error);
    auto second = MappedImage::map(file, error);
    BOOST_TEST(!second.has_value());
    BOOST_TEST(error.rfind("cannot reserve the image range at 0x34000000: ", 0) == 0u);
}

BOOST_AUTO_TEST_CASE(test_rejects_relocations) {
    PEBuilder builder;
    builder.imageBase = 0x35000000;
    builder.characteristics = 0x0102;
    builder.sections = {
        {".reloc", 0x1000, 0x10, std::vector<std::uint8_t>(0x10), dataSection}
    };
    builder.setDirectory(BaseRelocationDirectory, 0x1000, 0x10);
    std::string error;
    auto file = PEFile::parse(builder.build(), error);
    BOOST_TEST_REQUIRE(file.has_value(), error);
    BOOST_TEST(!MappedImage::map(*file, error).has_value());
    BOOST_TEST(error == "images with relocations are not supported");
}

BOOST_AUTO_TEST_SUITE_END()
