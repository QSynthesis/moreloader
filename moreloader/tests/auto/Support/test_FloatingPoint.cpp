#include <string>

#include <boost/test/unit_test.hpp>

#include <moreloader/Support/FloatingPoint.h>

#include "GoldenData.h"

using namespace more::loader;

BOOST_AUTO_TEST_SUITE(test_FloatingPoint)

// threads.txt: creator-before with the state of the probe, then per creation function the control
// word of the creator and the control word and MXCSR of the new thread. Measured on Windows with
// CreateThread and the _beginthreadex of msvcrt.dll, the creator set to 0x37F and to 0x07F.
BOOST_AUTO_TEST_CASE(test_new_thread_state_matches_windows) {
    int checked = 0;
    for (auto &fields : golden::read("threads.txt")) {
        if (fields[0] == "creator-before") {
            continue;
        }
        BOOST_TEST_CONTEXT(fields[0] << " created by " << fields[1]) {
            BOOST_TEST(std::stoul(fields[2], nullptr, 16) == windowsControlWord);
            BOOST_TEST(std::stoul(fields[3], nullptr, 16) == windowsMXCSR);
        }
        ++checked;
    }
    BOOST_TEST(checked == 4);
}

BOOST_AUTO_TEST_CASE(test_set_control_word) {
    unsigned short saved = x87ControlWord();
    setX87ControlWord(windowsControlWord);
    BOOST_TEST(x87ControlWord() == windowsControlWord);
    setX87ControlWord(saved);
}

BOOST_AUTO_TEST_SUITE_END()
