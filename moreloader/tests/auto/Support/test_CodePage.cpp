#include <string>

#include <boost/test/unit_test.hpp>

#include <moreloader/Support/CodePage.h>

#include "GoldenData.h"

using namespace more::loader;

BOOST_AUTO_TEST_SUITE(test_CodePage)

// utf8.txt, lines mbtowc: input, length, code units, strict length, error. Measured with
// MultiByteToWideChar(CP_UTF8) without and with MB_ERR_INVALID_CHARS.
BOOST_AUTO_TEST_CASE(test_multi_byte_to_wide_matches_windows) {
    int checked = 0;
    for (auto &fields : golden::read("utf8.txt")) {
        if (fields[0] != "mbtowc") {
            continue;
        }
        auto result = multiByteToWide(golden::unescape(fields[1]));
        BOOST_TEST_CONTEXT(fields[1]) {
            BOOST_TEST(golden::unitsText(result.text) == fields[3]);
            // A strict conversion fails exactly if the lenient one replaced something.
            BOOST_TEST(result.invalid == (fields[4] == "0"));
        }
        ++checked;
    }
    BOOST_TEST(checked == 15);
}

// utf8.txt, lines wctomb: code units, length including the terminator, bytes.
BOOST_AUTO_TEST_CASE(test_wide_to_multi_byte_matches_windows) {
    int checked = 0;
    for (auto &fields : golden::read("utf8.txt")) {
        if (fields[0] != "wctomb") {
            continue;
        }
        auto result = wideToMultiByte(golden::units(fields[1]));
        std::string expected = golden::unescape(fields[3]);
        expected.pop_back(); // terminator
        BOOST_TEST_CONTEXT(fields[1]) {
            BOOST_TEST(result.text == expected);
        }
        ++checked;
    }
    BOOST_TEST(checked == 6);
}

BOOST_AUTO_TEST_SUITE_END()
