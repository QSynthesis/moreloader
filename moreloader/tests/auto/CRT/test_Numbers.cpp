#include <set>
#include <string>

#include <boost/test/unit_test.hpp>

#include <moreloader/CRT/Numbers.h>
#include <moreloader/Support/CodePage.h>

#include "GoldenData.h"

using namespace more::loader;

BOOST_AUTO_TEST_SUITE(test_Numbers)

// atof.txt: input, bits of the result.
BOOST_AUTO_TEST_CASE(test_atof_matches_msvcrt) {
    // Measured deviations of msvcrt from correct rounding that are not reproduced, as declared
    // in Numbers.h.
    const std::set<std::string> knownDeviations = {
        "1.000000000000000111022302462515654042363166809082031250000001",
        "2.4703282292062328e-324",
    };
    int checked = 0;
    int skipped = 0;
    for (auto &fields : golden::read("atof.txt")) {
        std::string input = golden::unescape(fields[0]);
        if (knownDeviations.count(input)) {
            ++skipped;
            continue;
        }
        BOOST_TEST_CONTEXT(fields[0]) {
            BOOST_TEST(msvcrt::atof(input.c_str()) == golden::bits(fields[1]));
        }
        ++checked;
    }
    BOOST_TEST(skipped == 2);
    BOOST_TEST(checked > 600);
}

// strtol.txt: input, base, strtol value, strtol consumed, strtol errno, strtoul value, strtoul
// consumed, strtoul errno, wcstol value, wcstol consumed.
BOOST_AUTO_TEST_CASE(test_strtol_matches_msvcrt) {
    int checked = 0;
    for (auto &fields : golden::read("strtol.txt")) {
        std::string input = golden::unescape(fields[0]);
        int base = std::stoi(fields[1]);
        BOOST_TEST_CONTEXT(fields[0] << " base " << base) {
            auto l = msvcrt::strtol(input.c_str(), base);
            BOOST_TEST(std::int32_t(l.value) == std::stol(fields[2]));
            BOOST_TEST(l.consumed == std::stoul(fields[3]));
            BOOST_TEST(l.overflow == (fields[4] == "34"));

            auto u = msvcrt::strtoul(input.c_str(), base);
            BOOST_TEST(u.value == std::stoul(fields[5]));
            BOOST_TEST(u.consumed == std::stoul(fields[6]));
            BOOST_TEST(u.overflow == (fields[7] == "34"));

            std::u16string wide = multiByteToWide(input).text;
            auto w = msvcrt::strtol(wide.c_str(), base);
            BOOST_TEST(std::int32_t(w.value) == std::stol(fields[8]));
            BOOST_TEST(w.consumed == std::stoul(fields[9]));
        }
        ++checked;
    }
    BOOST_TEST(checked == 68);
}

BOOST_AUTO_TEST_SUITE_END()
