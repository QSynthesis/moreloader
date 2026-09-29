#include <cstdint>
#include <optional>
#include <thread>

#include <boost/test/unit_test.hpp>

#include <moreloader/Runtime/ThreadArea.h>

using namespace more::loader;

namespace {

    std::uint32_t readFS(std::uint32_t offset) {
        std::uint32_t value;
        asm volatile("movl %%fs:(%1), %0" : "=r"(value) : "r"(offset));
        return value;
    }

    std::uint32_t addressOf(const void *p) {
        return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p));
    }

    // Points FS at \a block and returns the selector together with the first two words read
    // through FS.
    struct Observation {
        std::optional<std::uint16_t> selector;
        std::uint32_t first = 0;
        std::uint32_t second = 0;
    };

    Observation observe(const std::uint32_t *block) {
        Observation observation;
        observation.selector = setThreadArea(addressOf(block), 8 - 1);
        if (observation.selector) {
            loadFS(*observation.selector);
            observation.first = readFS(0);
            observation.second = readFS(4);
            loadFS(0);
        }
        return observation;
    }

}

BOOST_AUTO_TEST_SUITE(test_ThreadArea)

BOOST_AUTO_TEST_CASE(test_fs_addresses_the_block) {
    static const std::uint32_t block[2] = {0x11223344, 0x55667788};
    Observation observation = observe(block);
    BOOST_TEST_REQUIRE(observation.selector.has_value());
    // A GDT selector with requested privilege level 3.
    BOOST_TEST((*observation.selector & 7) == 3);
    BOOST_TEST(observation.first == 0x11223344u);
    BOOST_TEST(observation.second == 0x55667788u);
}

BOOST_AUTO_TEST_CASE(test_each_thread_has_its_own_base) {
    static const std::uint32_t mainBlock[2] = {1, 2};
    static const std::uint32_t otherBlock[2] = {3, 4};
    Observation main = observe(mainBlock);
    Observation other;
    std::thread thread([&] { other = observe(otherBlock); });
    thread.join();
    // The entry is allocated once and then shared by number, but each thread writes its own
    // base into it.
    Observation mainAgain = observe(mainBlock);
    BOOST_TEST_REQUIRE(main.selector.has_value());
    BOOST_TEST_REQUIRE(other.selector.has_value());
    BOOST_TEST(*other.selector == *main.selector);
    BOOST_TEST(other.first == 3u);
    BOOST_TEST(other.second == 4u);
    BOOST_TEST(mainAgain.first == 1u);
}

BOOST_AUTO_TEST_SUITE_END()
