#include <cstring>
#include <deque>
#include <string>
#include <vector>

#include <boost/test/unit_test.hpp>

#include <moreloader/CRT/Format.h>
#include <moreloader/Support/CodePage.h>

#include "GoldenData.h"

using namespace more::loader;

namespace {

    // Packs the argument specification of the probe, such as i:42,f:3FF0000000000000,s:abc, as
    // the i386 calling convention lays the arguments out on the stack.
    struct PackedArguments {
        std::vector<std::uint8_t> area;
        std::deque<std::string> narrow;
        std::deque<std::u16string> wide;

        explicit PackedArguments(const std::string &spec) {
            size_t start = 0;
            while (start < spec.size()) {
                size_t end = spec.find(',', start);
                if (end == std::string::npos) {
                    end = spec.size();
                }
                std::string item = spec.substr(start, end - start);
                char kind = item[0];
                std::string value = item.substr(2);
                switch (kind) {
                    case 'i':
                        push32(std::uint32_t(std::stol(value)));
                        break;
                    case 'p':
                        push32(std::uint32_t(std::stoul(value)));
                        break;
                    case 'I':
                        push64(std::uint64_t(std::stoll(value)));
                        break;
                    case 'f':
                        push64(golden::bits(value));
                        break;
                    case 's':
                        // Trailing null characters, because a wide conversion of a narrow
                        // argument reads two bytes at a time past the terminator, as the probe
                        // did from storage that happened to be zero.
                        narrow.push_back(golden::unescape(value) + std::string(4, '\0'));
                        push32(std::uint32_t(reinterpret_cast<std::uintptr_t>(narrow.back().c_str())));
                        break;
                    case 'S':
                        wide.push_back(multiByteToWide(golden::unescape(value)).text);
                        push32(std::uint32_t(reinterpret_cast<std::uintptr_t>(wide.back().c_str())));
                        break;
                    default:
                        BOOST_FAIL("unknown argument kind " << kind);
                }
                start = end + 1;
            }
            // Padding against formats that read one argument more than supplied, as in the probe.
            for (int i = 0; i < 8; ++i) {
                push32(0);
            }
        }

        void push32(std::uint32_t v) {
            auto p = reinterpret_cast<const std::uint8_t *>(&v);
            area.insert(area.end(), p, p + 4);
        }

        void push64(std::uint64_t v) {
            auto p = reinterpret_cast<const std::uint8_t *>(&v);
            area.insert(area.end(), p, p + 8);
        }
    };

}

BOOST_AUTO_TEST_SUITE(test_Format)

// printf.txt: N or W, format, arguments, result, return value. The narrow result was recorded as
// a C string and therefore ends at the first null character. The wide result is a list of code
// units of the returned length.
BOOST_AUTO_TEST_CASE(test_matches_msvcrt) {
    int narrowCases = 0;
    int wideCases = 0;
    int failures = 0;
    for (auto &fields : golden::read("printf.txt")) {
        const std::string &kind = fields[0];
        std::string format = golden::unescape(fields[1]);
        PackedArguments packed(fields[2]);
        msvcrt::GuestArguments arguments(packed.area.data());
        int expectedReturn = std::stoi(fields[4]);

        if (kind == "N") {
            auto result = msvcrt::formatNarrow(format.c_str(), arguments);
            std::string text = result.text.substr(0, result.text.find('\0'));
            int actualReturn = result.failed ? -1 : int(result.text.size());
            if (text != golden::unescape(fields[3]) || actualReturn != expectedReturn) {
                if (++failures <= 40) {
                    BOOST_ERROR("N " << fields[1] << " | " << fields[2] << ": expected ["
                                     << fields[3] << "] " << expectedReturn << ", got [" << text
                                     << "] " << actualReturn);
                }
            }
            ++narrowCases;
        } else {
            std::u16string wideFormat = multiByteToWide(format).text;
            auto result = msvcrt::formatWide(wideFormat.c_str(), arguments);
            int actualReturn = result.failed ? -1 : int(result.text.size());
            std::string actual = actualReturn >= 0 ? golden::unitsText(result.text) : "";
            if (actual != fields[3] || actualReturn != expectedReturn) {
                if (++failures <= 40) {
                    BOOST_ERROR("W " << fields[1] << " | " << fields[2] << ": expected ["
                                     << fields[3] << "] " << expectedReturn << ", got [" << actual
                                     << "] " << actualReturn);
                }
            }
            ++wideCases;
        }
    }
    BOOST_TEST(failures == 0);
    BOOST_TEST(narrowCases > 5000);
    BOOST_TEST(wideCases > 1000);
}

// snprintf.txt: N or W, format, arguments, count, buffer of count + 2 elements filled with # in
// advance, return value.
BOOST_AUTO_TEST_CASE(test_truncation_matches_msvcrt) {
    int checked = 0;
    for (auto &fields : golden::read("snprintf.txt")) {
        std::string format = golden::unescape(fields[1]);
        PackedArguments packed(fields[2]);
        msvcrt::GuestArguments arguments(packed.area.data());
        size_t count = std::stoul(fields[3]);
        int expectedReturn = std::stoi(fields[5]);

        BOOST_TEST_CONTEXT(fields[0] << " " << fields[1] << " " << count) {
            if (fields[0] == "N") {
                std::string buffer(32, '#');
                int ret = msvcrt::storeTruncated(msvcrt::formatNarrow(format.c_str(), arguments), buffer.data(),
                                         count);
                BOOST_TEST(buffer.substr(0, count + 2) == golden::unescape(fields[4]));
                BOOST_TEST(ret == expectedReturn);
            } else {
                std::u16string buffer(32, u'#');
                std::u16string wideFormat = multiByteToWide(format).text;
                int ret = msvcrt::storeTruncated(msvcrt::formatWide(wideFormat.c_str(), arguments), buffer.data(),
                                         count);
                BOOST_TEST(golden::unitsText(buffer.substr(0, count + 2)) == fields[4]);
                BOOST_TEST(ret == expectedReturn);
            }
        }
        ++checked;
    }
    BOOST_TEST(checked == 56);
}

BOOST_AUTO_TEST_SUITE_END()
