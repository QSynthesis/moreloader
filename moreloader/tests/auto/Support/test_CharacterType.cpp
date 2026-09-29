#include <string>
#include <vector>

#include <boost/test/unit_test.hpp>

#include <moreloader/Support/CharacterType.h>

#include "GoldenData.h"

using namespace more::loader;

BOOST_AUTO_TEST_SUITE(test_CharacterType)

// nls.txt, lines ctype1: code unit, result, type, lowercase length, lowercase unit, uppercase
// length, uppercase unit. Measured with GetStringTypeW(CT_CTYPE1) and LCMapStringW.
BOOST_AUTO_TEST_CASE(test_types_and_case_mappings_match_windows) {
    int checked = 0;
    for (auto &fields : golden::read("nls.txt")) {
        if (fields[0] != "ctype1") {
            continue;
        }
        auto unit = char16_t(std::stoul(fields[1], nullptr, 16));
        BOOST_TEST_CONTEXT(fields[1]) {
            auto type = characterType1Of(unit);
            BOOST_TEST_REQUIRE(type.has_value());
            BOOST_TEST(*type == std::stoul(fields[3], nullptr, 16));
            BOOST_TEST(unsigned(*lowerCaseOf(unit)) == std::stoul(fields[5], nullptr, 16));
            BOOST_TEST(unsigned(*upperCaseOf(unit)) == std::stoul(fields[7], nullptr, 16));
        }
        ++checked;
    }
    BOOST_TEST(checked == 257);
}

// nls.txt, lines compare-order: locale, the units 0x01 to 0x7F sorted by CompareStringW with
// NORM_IGNORECASE, separated by '=' if equal and by a space otherwise.
BOOST_AUTO_TEST_CASE(test_single_unit_order_matches_windows) {
    int checked = 0;
    for (auto &fields : golden::read("nls.txt")) {
        if (fields[0] != "compare-order") {
            continue;
        }
        const std::string &groups = fields[2];
        std::vector<char16_t> units;
        std::vector<bool> equalToPrevious;
        for (std::size_t i = 0; i < groups.size(); i += 5) {
            units.push_back(char16_t(std::stoul(groups.substr(i, 4), nullptr, 16)));
            equalToPrevious.push_back(i > 0 && groups[i - 1] == '=');
        }
        BOOST_TEST(units.size() == 127u);
        for (std::size_t i = 1; i < units.size(); ++i) {
            std::u16string a(1, units[i - 1]);
            std::u16string b(1, units[i]);
            auto result = compareIgnoringCase(a, b);
            BOOST_TEST_CONTEXT(fields[1] << " " << unsigned(a[0]) << " " << unsigned(b[0])) {
                if (equalToPrevious[i]) {
                    BOOST_TEST((result && *result == 0));
                } else {
                    // Unequal single units that are weighted separately have no known order.
                    BOOST_TEST((!result || *result == -1));
                }
            }
        }
        ++checked;
    }
    BOOST_TEST(checked == 2);
}

// nls.txt, lines compare-pair: locale, two strings, result of CompareStringW (1 less, 2 equal,
// 3 greater).
BOOST_AUTO_TEST_CASE(test_string_comparisons_match_windows) {
    int checked = 0;
    for (auto &fields : golden::read("nls.txt")) {
        if (fields[0] != "compare-pair") {
            continue;
        }
        std::u16string a(fields[2].begin(), fields[2].end());
        std::u16string b(fields[3].begin(), fields[3].end());
        auto result = compareIgnoringCase(a, b);
        int expected = std::stoi(fields[4]) - 2;
        BOOST_TEST_CONTEXT(fields[1] << " " << fields[2] << " " << fields[3]) {
            if (fields[2].find_first_of("-'") != std::string::npos && expected != 0) {
                BOOST_TEST(!result.has_value());
            } else {
                BOOST_TEST_REQUIRE(result.has_value());
                BOOST_TEST(*result == expected);
            }
        }
        ++checked;
    }
    BOOST_TEST(checked == 18);
}

BOOST_AUTO_TEST_CASE(test_units_outside_the_measured_range_are_absent) {
    BOOST_TEST(!characterType1Of(u'\u0100').has_value());
    BOOST_TEST(!lowerCaseOf(u'\u4F60').has_value());
    BOOST_TEST(!upperCaseOf(u'\uFFFE').has_value());
    BOOST_TEST(!compareIgnoringCase(u"a\u00E9", u"a").has_value());
}

BOOST_AUTO_TEST_SUITE_END()
