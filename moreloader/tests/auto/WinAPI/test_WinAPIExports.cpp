#include <algorithm>
#include <string>
#include <vector>

#include <boost/test/unit_test.hpp>

#include <moreloader/Runtime/ExportRegistry.h>
#include <moreloader/WinAPI/WinAPIExports.h>

#include "ExecutableImports.h"

using namespace more::loader;
namespace utf = boost::unit_test;

namespace {

    ExportRegistry winapiRegistry() {
        ExportRegistry registry;
        registerWinAPIExports(registry);
        return registry;
    }

    // Returns the imports other than those from msvcrt.dll of the executable at \a path that
    // \a registry lacks, as library!name.
    std::vector<std::string> missingImports(const ExportRegistry &registry,
                                            const std::string &path) {
        std::vector<std::string> missing;
        for (const PEImport &import : testing::importsOf(path)) {
            if (ExportRegistry::normalizeLibrary(import.library) != "msvcrt" &&
                !registry.find(import.library, import.name)) {
                missing.push_back(import.library + "!" + import.name);
            }
        }
        return missing;
    }

    void reportMissing(const std::vector<std::string> &missing) {
        BOOST_TEST(missing.empty(), "missing: " << missing.size());
        for (const std::string &name : missing) {
            BOOST_TEST_MESSAGE(name << " is not registered");
        }
    }

}

BOOST_AUTO_TEST_SUITE(test_WinAPIExports)

BOOST_AUTO_TEST_CASE(test_registered_libraries) {
    std::vector<std::string> expected = {"kernel32", "shell32", "shlwapi", "user32"};
    BOOST_TEST(winapiRegistry().libraries() == expected, boost::test_tools::per_element());
}

BOOST_AUTO_TEST_CASE(test_every_import_of_moresampler_is_registered,
                     *utf::precondition(testing::ExecutableAvailable{testing::moresamplerPath()})) {
    reportMissing(missingImports(winapiRegistry(), testing::moresamplerPath()));
}

// The imports of resampler.exe that are deliberately not registered, because the program does not
// call them in the paths that it takes. The reasons are recorded in
// docs/claude/20260929-resampler.md, section 6.
BOOST_AUTO_TEST_CASE(test_imports_of_resampler_are_registered_except_the_documented,
                     *utf::precondition(testing::ExecutableAvailable{testing::resamplerPath()})) {
    std::vector<std::string> missing = missingImports(winapiRegistry(), testing::resamplerPath());
    std::sort(missing.begin(), missing.end());
    std::vector<std::string> documented = {
        "KERNEL32.dll!CompareStringA",       "KERNEL32.dll!GetStringTypeA",
        "KERNEL32.dll!LCMapStringA",         "KERNEL32.dll!RtlUnwind",
        "KERNEL32.dll!SetCurrentDirectoryA", "KERNEL32.dll!SetEnvironmentVariableA",
        "KERNEL32.dll!SetStdHandle",         "KERNEL32.dll!VirtualAlloc",
        "KERNEL32.dll!VirtualFree",
    };
    BOOST_TEST(missing == documented, boost::test_tools::per_element());
}

BOOST_AUTO_TEST_SUITE_END()
