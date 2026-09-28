#include <boost/test/unit_test.hpp>

#include <moreloader/Support/PathMapping.h>

using namespace more::loader;

BOOST_AUTO_TEST_SUITE(test_PathMapping)

BOOST_AUTO_TEST_CASE(test_guest_from_host) {
    BOOST_TEST(guestPathFromHost("/") == "Z:\\");
    BOOST_TEST(guestPathFromHost("/home/user/voice/a.wav") == "Z:\\home\\user\\voice\\a.wav");
    BOOST_TEST(guestPathFromHost("voice/a.wav") == "voice\\a.wav");
    BOOST_TEST(guestPathFromHost("a.wav") == "a.wav");
}

BOOST_AUTO_TEST_CASE(test_host_from_guest) {
    BOOST_TEST(hostPathFromGuest("Z:\\home\\user\\a.wav") == "/home/user/a.wav");
    BOOST_TEST(hostPathFromGuest("z:/home/user/a.wav") == "/home/user/a.wav");
    BOOST_TEST(hostPathFromGuest("Z:\\") == "/");
    BOOST_TEST(hostPathFromGuest("Z:") == "/");
    BOOST_TEST(hostPathFromGuest("\\tmp\\x") == "/tmp/x");
    BOOST_TEST(hostPathFromGuest("\\\\?\\Z:\\tmp\\x") == "/tmp/x");
    BOOST_TEST(hostPathFromGuest("voice\\a.wav") == "voice/a.wav");
    BOOST_TEST(hostPathFromGuest("Z:voice\\a.wav") == "voice/a.wav");
    BOOST_TEST(hostPathFromGuest("C:\\Windows") == "c:/Windows");
}

BOOST_AUTO_TEST_CASE(test_nul_device) {
    BOOST_TEST(hostPathFromGuest("nul") == "/dev/null");
    BOOST_TEST(hostPathFromGuest("NUL") == "/dev/null");
    BOOST_TEST(hostPathFromGuest("Z:\\tmp\\Nul") == "/dev/null");
    BOOST_TEST(hostPathFromGuest("nul.txt") == "nul.txt");
    BOOST_TEST(hostPathFromGuest("null") == "null");
}

BOOST_AUTO_TEST_CASE(test_round_trip) {
    for (const char *path : {"/", "/a/b", "/a b/c.wav", "rel/x"}) {
        BOOST_TEST(hostPathFromGuest(guestPathFromHost(path)) == path);
    }
}

BOOST_AUTO_TEST_CASE(test_arguments) {
    BOOST_TEST(guestArgumentFromHost("/tmp/in.wav") == "Z:\\tmp\\in.wav");
    BOOST_TEST(guestArgumentFromHost("in.wav") == "in.wav");
    BOOST_TEST(guestArgumentFromHost("AA#5#") == "AA#5#");
    BOOST_TEST(guestArgumentFromHost("") == "");
}

BOOST_AUTO_TEST_SUITE_END()
