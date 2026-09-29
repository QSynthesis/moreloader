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

// nls.txt, lines wctomb-partial: code units, result, error, contents of a buffer of 2 bytes
// that was filled with 0xCC before the call; lines sbuplow-lcmap: the same for a buffer of 256
// bytes and the case mappings of the bytes 0 to 255 in UTF-8. Bytes that remain 0xCC were not
// written.
BOOST_AUTO_TEST_CASE(test_whole_character_prefix_matches_windows) {
    int checked = 0;
    for (auto &fields : golden::read("nls.txt")) {
        std::string text;
        std::string buffer;
        if (fields[0] == "wctomb-partial") {
            text = wideToMultiByte(golden::units(fields[1])).text;
            buffer = golden::unescape(fields[4]);
        } else if (fields[0] == "sbuplow-lcmap") {
            // The mapped text is the measured output where it was written. The remainder
            // consists of U+FFFD, which each byte from 0x80 becomes.
            buffer = golden::unescape(fields[6]);
            text = buffer.substr(0, 128);
            for (int i = 128; i < 256; ++i) {
                text += "\xEF\xBF\xBD";
            }
        } else {
            continue;
        }
        std::size_t written = buffer.find_last_not_of('\xCC') + 1;
        BOOST_TEST_CONTEXT(fields[0] << " " << fields[1]) {
            BOOST_TEST(wholeCharacterPrefix(text, buffer.size()) == written);
            BOOST_TEST(text.substr(0, written) == buffer.substr(0, written));
        }
        ++checked;
    }
    BOOST_TEST(checked == 5);
}

BOOST_AUTO_TEST_SUITE_END()
