#include <string>

#include <boost/test/unit_test.hpp>

#include <moreloader/CRT/CharacterClass.h>

#include "GoldenData.h"

using namespace more::loader;
using namespace more::loader::msvcrt;

BOOST_AUTO_TEST_SUITE(test_CharacterClass)

// ctype.txt: character, flags of isalnum, isalpha, iscntrl, isgraph, islower, ispunct, isspace,
// isupper and isxdigit, tolower, toupper.
BOOST_AUTO_TEST_CASE(test_matches_msvcrt) {
    int checked = 0;
    for (auto &fields : golden::read("ctype.txt")) {
        int c = std::stoi(fields[0]);
        int k = msvcrt::characterClass(c);
        std::string flags;
        flags += (k & (ClassUpper | ClassLower | ClassDigit)) ? 'n' : '-';
        flags += (k & (ClassUpper | ClassLower)) ? 'a' : '-';
        flags += (k & ClassControl) ? 'c' : '-';
        flags += (k & (ClassUpper | ClassLower | ClassDigit | ClassPunct)) ? 'g' : '-';
        flags += (k & ClassLower) ? 'l' : '-';
        flags += (k & ClassPunct) ? 'p' : '-';
        flags += (k & ClassSpace) ? 's' : '-';
        flags += (k & ClassUpper) ? 'u' : '-';
        flags += (k & ClassHex) ? 'x' : '-';
        BOOST_TEST_CONTEXT("character " << c) {
            BOOST_TEST(flags == fields[1]);
            BOOST_TEST(msvcrt::toLower(c) == std::stoi(fields[2]));
            BOOST_TEST(msvcrt::toUpper(c) == std::stoi(fields[3]));
        }
        ++checked;
    }
    BOOST_TEST(checked == 384);
}

BOOST_AUTO_TEST_SUITE_END()
