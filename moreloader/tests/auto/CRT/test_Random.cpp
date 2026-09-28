#include <algorithm>
#include <string>

#include <boost/test/unit_test.hpp>

#include <moreloader/CRT/Random.h>

#include "GoldenData.h"

using namespace more::loader;

BOOST_AUTO_TEST_SUITE(test_Random)

// rand.txt: newthread with the first values of a new thread, or seed<n> with the first values
// after srand(n).
BOOST_AUTO_TEST_CASE(test_matches_msvcrt) {
    int checked = 0;
    for (auto &fields : golden::read("rand.txt")) {
        msvcrt::Random random;
        if (fields[0] != "newthread") {
            random.seed(std::uint32_t(std::stoul(fields[0].substr(4))));
        }
        std::string values;
        size_t count = std::count(fields[1].begin(), fields[1].end(), ',') + 1;
        for (size_t i = 0; i < count; ++i) {
            values += (i ? "," : "") + std::to_string(random.next());
        }
        BOOST_TEST_CONTEXT(fields[0]) {
            BOOST_TEST(values == fields[1]);
        }
        ++checked;
    }
    BOOST_TEST(checked == 5);
}

BOOST_AUTO_TEST_SUITE_END()
