#include <string>

#include <boost/test/unit_test.hpp>

#include <moreloader/Support/Diagnostics.h>

#include "ProcessTools.h"

using namespace more::loader;

BOOST_AUTO_TEST_SUITE(test_Diagnostics)

BOOST_AUTO_TEST_CASE(test_categories_are_independent) {
    const DiagnosticCategory all[] = {DiagnosticCategory::Imports, DiagnosticCategory::Stubs,
                                      DiagnosticCategory::DebugStrings,
                                      DiagnosticCategory::HeapCheck};
    for (DiagnosticCategory category : all) {
        BOOST_TEST(!isDiagnosticEnabled(category));
    }
    setDiagnosticEnabled(DiagnosticCategory::Stubs, true);
    for (DiagnosticCategory category : all) {
        BOOST_TEST(isDiagnosticEnabled(category) == (category == DiagnosticCategory::Stubs));
    }
    setDiagnosticEnabled(DiagnosticCategory::HeapCheck, true);
    setDiagnosticEnabled(DiagnosticCategory::Stubs, false);
    BOOST_TEST(!isDiagnosticEnabled(DiagnosticCategory::Stubs));
    BOOST_TEST(isDiagnosticEnabled(DiagnosticCategory::HeapCheck));
    setDiagnosticEnabled(DiagnosticCategory::HeapCheck, false);
    for (DiagnosticCategory category : all) {
        BOOST_TEST(!isDiagnosticEnabled(category));
    }
}

BOOST_AUTO_TEST_CASE(test_diagnostic_line) {
    std::string output = testing::captureStderr([] { diagnostic("value %d of %s", 42, "x"); });
    BOOST_TEST(output == "moreloader: value 42 of x\n");
}

BOOST_AUTO_TEST_CASE(test_long_lines_are_truncated) {
    std::string text(10000, 'a');
    std::string output = testing::captureStderr([&] { diagnostic("%s", text.c_str()); });
    // 4095 bytes including the prefix and the line feed.
    BOOST_TEST(output.size() == 4095u);
    BOOST_TEST(output.rfind("moreloader: aaa", 0) == 0u);
    BOOST_TEST(output.back() == '\n');
    BOOST_TEST(output.find_first_not_of('a', 12) == 4094u);
}

BOOST_AUTO_TEST_CASE(test_fatal_terminates_the_process) {
    testing::ChildResult child = testing::runInChild([] {
        std::atexit([] { std::fprintf(stderr, "exit handler\n"); });
        fatal("stopped at %u", 7u);
    });
    // No exit handler runs.
    BOOST_TEST(child.exitCode == 255);
    BOOST_TEST(child.errorOutput == "moreloader: stopped at 7\n");
}

BOOST_AUTO_TEST_SUITE_END()
