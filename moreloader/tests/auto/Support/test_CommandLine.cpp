#include <string>
#include <vector>

#include <boost/test/unit_test.hpp>

#include <moreloader/Support/CodePage.h>
#include <moreloader/Support/CommandLine.h>

#include "GoldenData.h"

using namespace more::loader;

BOOST_AUTO_TEST_SUITE(test_CommandLine)

// cmdline.txt: command line, argc, arguments. Measured with CommandLineToArgvW.
BOOST_AUTO_TEST_CASE(test_split_matches_windows) {
    int checked = 0;
    for (auto &fields : golden::read("cmdline.txt")) {
        std::string line = golden::unescape(fields[0]);
        if (line.empty()) {
            // CommandLineToArgvW returns the path of the executable, which is left to the caller.
            continue;
        }
        std::vector<std::u16string> expected;
        for (size_t i = 2; i < fields.size(); ++i) {
            expected.push_back(multiByteToWide(golden::unescape(fields[i])).text);
        }
        BOOST_REQUIRE_EQUAL(expected.size(), std::stoul(fields[1]));

        auto actual = splitCommandLine(multiByteToWide(line).text);
        BOOST_TEST_CONTEXT(fields[0]) {
            BOOST_TEST_REQUIRE(actual.size() == expected.size());
            for (size_t i = 0; i < actual.size(); ++i) {
                BOOST_TEST(golden::unitsText(actual[i]) == golden::unitsText(expected[i]));
            }
        }
        ++checked;
    }
    BOOST_TEST(checked == 19);
}

BOOST_AUTO_TEST_CASE(test_join_round_trip) {
    std::vector<std::vector<std::u16string>> cases = {
        {u"prog", u"a", u"b"},
        {u"C:\\Program Files\\x.exe", u"a b", u""},
        {u"prog", u"a\"b", u"a\\\"b", u"trail\\", u"trail\\\\", u"\\\\server\\share"},
        {u"prog", u"in.wav", u"out.wav", u"C4", u"100", u"", u"0", u"!120", u"AA#5#"},
        {u"prog", u"\"\"", u"\\", u" "},
    };
    for (auto &arguments : cases) {
        auto joined = joinCommandLine(arguments);
        auto split = splitCommandLine(joined);
        BOOST_TEST_REQUIRE(split.size() == arguments.size());
        for (size_t i = 0; i < split.size(); ++i) {
            BOOST_TEST(golden::unitsText(split[i]) == golden::unitsText(arguments[i]));
        }
    }
}

BOOST_AUTO_TEST_CASE(test_join_leaves_plain_arguments_unquoted) {
    BOOST_TEST((joinCommandLine({u"prog", u"a", u"b\\c"}) == u"prog a b\\c"));
    BOOST_TEST((joinCommandLine({u"my prog", u"a b"}) == u"\"my prog\" \"a b\""));
}

BOOST_AUTO_TEST_SUITE_END()
