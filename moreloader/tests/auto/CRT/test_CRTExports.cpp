#include <string>
#include <vector>

#include <boost/test/unit_test.hpp>

#include <moreloader/CRT/CRTExports.h>
#include <moreloader/Runtime/ExportRegistry.h>

#include "ExecutableImports.h"

using namespace more::loader;
namespace utf = boost::unit_test;

namespace {

    ExportRegistry crtRegistry() {
        ExportRegistry registry;
        registerCRTExports(registry);
        return registry;
    }

    // Returns the imports from msvcrt.dll of the executable at \a path that \a registry lacks.
    std::vector<std::string> missingImports(const ExportRegistry &registry,
                                            const std::string &path) {
        std::vector<std::string> missing;
        for (const PEImport &import : testing::importsOf(path)) {
            if (ExportRegistry::normalizeLibrary(import.library) == "msvcrt" &&
                !registry.find(import.library, import.name)) {
                missing.push_back(import.name);
            }
        }
        return missing;
    }

}

BOOST_AUTO_TEST_SUITE(test_CRTExports)

BOOST_AUTO_TEST_CASE(test_registers_only_msvcrt) {
    std::vector<std::string> expected = {"msvcrt"};
    BOOST_TEST(crtRegistry().libraries() == expected, boost::test_tools::per_element());
}

// moresampler reads these imports as variables (TaskSpec 3.3, static analysis), so the slot in
// the import address table must hold the address of the variable.
BOOST_AUTO_TEST_CASE(test_data_imports) {
    ExportRegistry registry = crtRegistry();
    for (const char *name : {"_iob", "__mb_cur_max", "__initenv", "_acmdln", "_fmode"}) {
        BOOST_TEST_CONTEXT(name) {
            auto found = registry.find("msvcrt.dll", name);
            BOOST_TEST_REQUIRE(found.has_value());
            BOOST_TEST((found->kind == ExportKind::Data));
        }
    }
    auto function = registry.find("msvcrt.dll", "fopen");
    BOOST_TEST_REQUIRE(function.has_value());
    BOOST_TEST((function->kind == ExportKind::Function));
}

BOOST_AUTO_TEST_CASE(test_every_import_of_moresampler_is_registered,
                     *utf::precondition(testing::ExecutableAvailable{testing::moresamplerPath()})) {
    std::vector<std::string> missing = missingImports(crtRegistry(), testing::moresamplerPath());
    BOOST_TEST(missing.empty(), "missing: " << missing.size());
    for (const std::string &name : missing) {
        BOOST_TEST_MESSAGE("msvcrt.dll!" << name << " is not registered");
    }
}

BOOST_AUTO_TEST_SUITE_END()
