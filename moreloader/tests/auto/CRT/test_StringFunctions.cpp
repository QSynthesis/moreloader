#include <cerrno>
#include <string>

#include <boost/test/unit_test.hpp>

#include <moreloader/CRT/StringFunctions.h>

#include "GoldenData.h"

using namespace more::loader;

BOOST_AUTO_TEST_SUITE(test_StringFunctions)

// strings.txt: strerror, asctime, _ultoa, _mbslen and _stricmp lines.
BOOST_AUTO_TEST_CASE(test_matches_msvcrt) {
    // The struct tm values of the probe, in the order of its asctime lines.
    const msvcrt::TimeFields times[] = {
        {55, 3, 2, 2, 0, 80, 3, 1, 0},
        {0, 0, 0, 31, 11, 126, 4, 364, 0},
        {9, 8, 17, 28, 8, 126, 1, 270, 0},
    };
    int asctimeIndex = 0;
    int checked = 0;
    for (auto &fields : golden::read("strings.txt")) {
        const std::string &kind = fields[0];
        BOOST_TEST_CONTEXT(kind << " " << fields[1]) {
            if (kind == "strerror") {
                BOOST_TEST(msvcrt::errorMessage(std::stoi(fields[1])) ==
                           golden::unescape(fields[2]));
            } else if (kind == "asctime") {
                BOOST_TEST(msvcrt::asctime(times[asctimeIndex++]) == golden::unescape(fields[1]));
            } else if (kind == "_ultoa") {
                BOOST_TEST(msvcrt::ultoa(std::uint32_t(std::stoul(fields[1])), std::stoi(fields[2])) ==
                           fields[3]);
            } else if (kind == "_stricmp") {
                std::string a = golden::unescape(fields[1]);
                std::string b = golden::unescape(fields[2]);
                BOOST_TEST(msvcrt::compareInsensitive(a.c_str(), b.c_str(), size_t(-1)) ==
                           std::stoi(fields[3]));
                BOOST_TEST(msvcrt::compareInsensitive(a.c_str(), b.c_str(), 1) ==
                           std::stoi(fields[4]));
            } else {
                // _mbslen depends on the ANSI code page of the measuring machine.
                continue;
            }
        }
        ++checked;
    }
    BOOST_TEST(asctimeIndex == 3);
    BOOST_TEST(checked == 76);
}

BOOST_AUTO_TEST_CASE(test_error_numbers) {
    BOOST_TEST(msvcrt::errorNumber(2) == 2);
    BOOST_TEST(msvcrt::errorNumber(34) == 34);
    BOOST_TEST(msvcrt::errorNumber(ENOTEMPTY) == 41);
    BOOST_TEST(msvcrt::errorNumber(ENAMETOOLONG) == 38);
}

BOOST_AUTO_TEST_SUITE_END()
