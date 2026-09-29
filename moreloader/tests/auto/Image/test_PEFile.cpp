#include <unistd.h>

#include <cstdio>
#include <string>
#include <vector>

#include <boost/test/unit_test.hpp>

#include <moreloader/Image/PEFile.h>

#include "PEBuilder.h"

using namespace more::loader;
using namespace testing;

namespace {

    // An image with a code section and a data section that holds the imports and the TLS
    // directory.
    PEBuilder sampleImage(ImportPlacement *imports = nullptr) {
        PEBuilder builder;
        builder.entryPointRVA = 0x1010;

        BuilderSection code{".text", 0x1000, 0x20, std::vector<std::uint8_t>(0x20, 0x90),
                            codeSection};
        BuilderSection data{".data", 0x2000, 0x400, {}, dataSection};
        ImportPlacement placement =
            placeImports(data, 0,
                         {
                             {"KERNEL32.dll", {"GetVersion", "#300"}},
                             {"msvcrt.dll",   {"malloc"}            }
        });
        builder.setDirectory(ImportDirectory, placement.directoryRVA, placement.directorySize);
        std::uint32_t tls = placeTLS(data, 0x300, builder.imageBase, 0x10002380, 0x10002388,
                                     0x10002390, 0x10, {0x10001000, 0x10001008});
        builder.setDirectory(TLSDirectory, tls, 24);
        builder.sections = {code, data};
        if (imports) {
            *imports = placement;
        }
        return builder;
    }

    std::optional<PEFile> parse(const PEBuilder &builder, std::string &error) {
        return PEFile::parse(builder.build(), error);
    }

    // Parses \a bytes and returns the error message, or an empty string if parsing succeeded.
    std::string errorOf(std::vector<std::uint8_t> bytes) {
        std::string error;
        auto file = PEFile::parse(std::move(bytes), error);
        return file ? std::string() : error;
    }

}

BOOST_AUTO_TEST_SUITE(test_PEFile)

BOOST_AUTO_TEST_CASE(test_headers) {
    std::string error;
    auto file = parse(sampleImage(), error);
    BOOST_TEST_REQUIRE(file.has_value(), error);
    BOOST_TEST(file->imageBase() == 0x10000000u);
    BOOST_TEST(file->sizeOfImage() == 0x3000u);
    BOOST_TEST(file->sizeOfHeaders() == 0x400u);
    BOOST_TEST(file->entryPointRVA() == 0x1010u);
    BOOST_TEST(file->stackReserve() == 0x200000u);
    BOOST_TEST(file->characteristics() == 0x0103);
    BOOST_TEST(file->subsystem() == 3);
    BOOST_TEST(!file->hasRelocations());
}

BOOST_AUTO_TEST_CASE(test_sections) {
    std::string error;
    auto file = parse(sampleImage(), error);
    BOOST_TEST_REQUIRE(file.has_value(), error);
    const auto &sections = file->sections();
    BOOST_TEST_REQUIRE(sections.size() == 2u);
    BOOST_TEST(sections[0].name == ".text");
    BOOST_TEST(sections[0].virtualAddress == 0x1000u);
    BOOST_TEST(sections[0].virtualSize == 0x20u);
    BOOST_TEST(sections[0].rawOffset == 0x400u);
    BOOST_TEST(sections[0].rawSize == 0x200u);
    BOOST_TEST(sections[0].characteristics == codeSection);
    BOOST_TEST(sections[1].name == ".data");
    BOOST_TEST(sections[1].rawOffset == 0x600u);
    BOOST_TEST(sections[1].rawSize == 0x400u);
}

BOOST_AUTO_TEST_CASE(test_section_name_of_eight_characters) {
    PEBuilder builder;
    builder.sections = {
        {".rdata12", 0x1000, 0x10, std::vector<std::uint8_t>(0x10), dataSection}
    };
    std::string error;
    auto file = parse(builder, error);
    BOOST_TEST_REQUIRE(file.has_value(), error);
    BOOST_TEST(file->sections()[0].name == ".rdata12");
}

BOOST_AUTO_TEST_CASE(test_imports) {
    ImportPlacement placement;
    std::string error;
    auto file = parse(sampleImage(&placement), error);
    BOOST_TEST_REQUIRE(file.has_value(), error);
    const auto &imports = file->imports();
    BOOST_TEST_REQUIRE(imports.size() == 3u);

    BOOST_TEST(imports[0].library == "KERNEL32.dll");
    BOOST_TEST(imports[0].name == "GetVersion");
    BOOST_TEST(imports[0].ordinal == 0); // The hint.
    BOOST_TEST(imports[0].slotRVA == placement.slots["KERNEL32.dll!GetVersion"]);

    BOOST_TEST(imports[1].library == "KERNEL32.dll");
    BOOST_TEST(imports[1].name.empty());
    BOOST_TEST(imports[1].ordinal == 300);
    BOOST_TEST(imports[1].slotRVA == placement.slots["KERNEL32.dll!#300"]);

    BOOST_TEST(imports[2].library == "msvcrt.dll");
    BOOST_TEST(imports[2].name == "malloc");
    BOOST_TEST(imports[2].slotRVA == placement.slots["msvcrt.dll!malloc"]);
}

BOOST_AUTO_TEST_CASE(test_imports_without_lookup_table) {
    // Some linkers omit the original first thunk. The import address table then names the
    // imports.
    PEBuilder builder;
    BuilderSection data{".idata", 0x1000, 0x200, {}, dataSection};
    ImportPlacement placement = placeImports(data, 0,
                                             {
                                                 {"USER32.dll", {"MessageBoxW", "wsprintfW"}}
    },
                                             false);
    builder.setDirectory(ImportDirectory, placement.directoryRVA, placement.directorySize);
    builder.sections = {data};
    std::string error;
    auto file = parse(builder, error);
    BOOST_TEST_REQUIRE(file.has_value(), error);
    BOOST_TEST_REQUIRE(file->imports().size() == 2u);
    BOOST_TEST(file->imports()[1].name == "wsprintfW");
    BOOST_TEST(file->imports()[1].ordinal == 1);
    BOOST_TEST(file->imports()[1].slotRVA == placement.slots["USER32.dll!wsprintfW"]);
}

BOOST_AUTO_TEST_CASE(test_no_imports_and_no_tls) {
    PEBuilder builder;
    builder.sections = {
        {".text", 0x1000, 0x10, std::vector<std::uint8_t>(0x10), codeSection}
    };
    std::string error;
    auto file = parse(builder, error);
    BOOST_TEST_REQUIRE(file.has_value(), error);
    BOOST_TEST(file->imports().empty());
    BOOST_TEST(!file->tls().has_value());
}

BOOST_AUTO_TEST_CASE(test_tls_directory) {
    std::string error;
    auto file = parse(sampleImage(), error);
    BOOST_TEST_REQUIRE(file.has_value(), error);
    BOOST_TEST_REQUIRE(file->tls().has_value());
    const PETLSDirectory &tls = *file->tls();
    BOOST_TEST(tls.startOfRawData == 0x10002380u);
    BOOST_TEST(tls.endOfRawData == 0x10002388u);
    BOOST_TEST(tls.indexAddress == 0x10002390u);
    BOOST_TEST(tls.callbacksAddress == 0x10002318u);
    BOOST_TEST(tls.sizeOfZeroFill == 0x10u);
    BOOST_TEST_REQUIRE(tls.callbacks.size() == 2u);
    BOOST_TEST(tls.callbacks[0] == 0x10001000u);
    BOOST_TEST(tls.callbacks[1] == 0x10001008u);
}

BOOST_AUTO_TEST_CASE(test_relocations) {
    PEBuilder builder = sampleImage();
    builder.setDirectory(BaseRelocationDirectory, 0x2380, 8);
    std::string error;
    builder.characteristics = 0x0102;
    auto file = parse(builder, error);
    BOOST_TEST_REQUIRE(file.has_value(), error);
    BOOST_TEST(file->hasRelocations());

    // The directory is ignored if the flag IMAGE_FILE_RELOCS_STRIPPED is set.
    builder.characteristics = 0x0103;
    file = parse(builder, error);
    BOOST_TEST_REQUIRE(file.has_value(), error);
    BOOST_TEST(!file->hasRelocations());
}

BOOST_AUTO_TEST_CASE(test_file_offset) {
    std::string error;
    auto file = parse(sampleImage(), error);
    BOOST_TEST_REQUIRE(file.has_value(), error);
    // The headers.
    BOOST_TEST(file->fileOffset(0x3C, 4).value_or(0xFFFFFFFF) == 0x3Cu);
    // Raw data of the sections.
    BOOST_TEST(file->fileOffset(0x1004, 4).value_or(0) == 0x404u);
    BOOST_TEST(file->fileOffset(0x2100, 16).value_or(0) == 0x700u);
    // The last bytes of the raw data, and one byte beyond.
    BOOST_TEST(file->fileOffset(0x11FC, 4).value_or(0) == 0x5FCu);
    BOOST_TEST(!file->fileOffset(0x11FD, 4).has_value());
    // Beyond the headers without belonging to a section.
    BOOST_TEST(!file->fileOffset(0x3FE, 4).has_value());
    BOOST_TEST(!file->fileOffset(0x5000, 1).has_value());
}

BOOST_AUTO_TEST_CASE(test_rejects_invalid_files) {
    std::vector<std::uint8_t> good = sampleImage().build();
    BOOST_TEST(errorOf(good).empty());

    BOOST_TEST(errorOf({}) == "not an MZ executable");
    auto bytes = good;
    bytes[0] = 'X';
    BOOST_TEST(errorOf(bytes) == "not an MZ executable");

    bytes = good;
    bytes[PEBuilder::peOffset] = 'X';
    BOOST_TEST(errorOf(bytes) == "no PE signature");

    bytes = good;
    put32(bytes, 0x3C, 0xFFFFFF00);
    BOOST_TEST(errorOf(bytes) == "no PE signature");

    PEBuilder builder = sampleImage();
    builder.machine = 0x8664;
    BOOST_TEST(errorOf(builder.build()) == "not an i386 image");

    builder = sampleImage();
    builder.magic = 0x20B;
    BOOST_TEST(errorOf(builder.build()) == "not a PE32 optional header");

    builder = sampleImage();
    builder.setDirectory(BoundImportDirectory, 0x2000, 8);
    BOOST_TEST(errorOf(builder.build()) == "bound and delay-load imports are not supported");

    builder = sampleImage();
    builder.setDirectory(DelayImportDirectory, 0x2000, 32);
    BOOST_TEST(errorOf(builder.build()) == "bound and delay-load imports are not supported");

    // The raw data of the second section ends beyond the end of the file.
    bytes = sampleImage().build();
    bytes.resize(bytes.size() - 0x10);
    BOOST_TEST(errorOf(bytes) == "section .data lies beyond the end of the file");

    // A section that extends beyond SizeOfImage.
    bytes = sampleImage().build();
    put32(bytes, PEBuilder::peOffset + 24 + 56, 0x2000);
    BOOST_TEST(errorOf(bytes) == "section .data lies beyond the image");

    // The section table is cut off.
    bytes = sampleImage().build();
    put16(bytes, PEBuilder::peOffset + 6, 0x7FFF);
    BOOST_TEST(errorOf(bytes) == "truncated section table");

    builder = sampleImage();
    builder.setDirectory(ImportDirectory, 0x8000, 40);
    BOOST_TEST(errorOf(builder.build()) == "invalid import directory");

    builder = sampleImage();
    builder.setDirectory(TLSDirectory, 0x8000, 24);
    BOOST_TEST(errorOf(builder.build()) == "invalid TLS directory");
}

BOOST_AUTO_TEST_CASE(test_read) {
    std::string error;
    BOOST_TEST(!PEFile::read("/nonexistent/moreloader-test.exe", error).has_value());
    BOOST_TEST(error == "cannot open /nonexistent/moreloader-test.exe");

    char path[] = "/tmp/moreloader-pe-XXXXXX";
    int descriptor = ::mkstemp(path);
    BOOST_TEST_REQUIRE(descriptor >= 0);
    std::vector<std::uint8_t> bytes = sampleImage().build();
    BOOST_TEST_REQUIRE(::write(descriptor, bytes.data(), bytes.size()) == ssize_t(bytes.size()));
    ::close(descriptor);
    auto file = PEFile::read(path, error);
    ::unlink(path);
    BOOST_TEST_REQUIRE(file.has_value(), error);
    BOOST_TEST(file->data() == bytes);
    BOOST_TEST(file->imports().size() == 3u);
}

BOOST_AUTO_TEST_SUITE_END()
