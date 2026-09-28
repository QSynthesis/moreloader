#include <string>
#include <utility>
#include <vector>

#include <boost/test/unit_test.hpp>

#include <moreloader/CRT/Sort.h>

#include "GoldenData.h"

using namespace more::loader;

namespace {

    struct Element {
        int key;
        int id;
    };

    std::vector<std::pair<int, int>> s_comparisons;

    int compareElements(const void *a, const void *b) {
        auto x = static_cast<const Element *>(a);
        auto y = static_cast<const Element *>(b);
        s_comparisons.emplace_back(x->id, y->id);
        return x->key < y->key ? -1 : (x->key > y->key ? 1 : 0);
    }

    std::vector<int> parseList(const std::string &text) {
        std::vector<int> out;
        size_t start = 0;
        while (start < text.size()) {
            size_t end = text.find(',', start);
            if (end == std::string::npos) {
                end = text.size();
            }
            out.push_back(std::stoi(text.substr(start, end - start)));
            start = end + 1;
        }
        return out;
    }

}

BOOST_AUTO_TEST_SUITE(test_Sort)

// qsort.txt: name, keys, final order of element ids, comparisons as id:id. Measured with a
// comparator that records its arguments.
BOOST_AUTO_TEST_CASE(test_matches_msvcrt) {
    int checked = 0;
    for (auto &fields : golden::read("qsort.txt")) {
        std::vector<int> keys = parseList(fields[1]);
        std::vector<Element> elements;
        for (size_t i = 0; i < keys.size(); ++i) {
            elements.push_back({keys[i], int(i)});
        }
        s_comparisons.clear();
        msvcrt::quickSort(elements.data(), elements.size(), sizeof(Element), compareElements);

        std::string order;
        for (size_t i = 0; i < elements.size(); ++i) {
            order += (i ? "," : "") + std::to_string(elements[i].id);
        }
        std::string comparisons;
        for (size_t i = 0; i < s_comparisons.size(); ++i) {
            comparisons += (i ? "," : "") + std::to_string(s_comparisons[i].first) + ":" +
                           std::to_string(s_comparisons[i].second);
        }
        BOOST_TEST_CONTEXT(fields[0]) {
            BOOST_TEST(order == (fields.size() > 2 ? fields[2] : ""));
            BOOST_TEST(comparisons == (fields.size() > 3 ? fields[3] : ""));
        }
        ++checked;
    }
    BOOST_TEST(checked == 80);
}

BOOST_AUTO_TEST_SUITE_END()
