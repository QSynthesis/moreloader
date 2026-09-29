#include <string>

#include <boost/test/unit_test.hpp>

#include <moreloader/Support/PathMapping.h>

#include "GoldenData.h"

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
    // The host has /tmp, /tmp/in.wav and /home/user/out, but not /home/user/out/new.wav.
    auto exists = [](const std::string &path) {
        return path == "/tmp" || path == "/tmp/in.wav" || path == "/home" || path == "/home/user" ||
               path == "/home/user/out";
    };
    BOOST_TEST(guestArgumentFromHost("/tmp/in.wav", exists) == "Z:\\tmp\\in.wav");
    BOOST_TEST(guestArgumentFromHost("/home/user/out/new.wav", exists) ==
               "Z:\\home\\user\\out\\new.wav");
    BOOST_TEST(guestArgumentFromHost("in.wav", exists) == "in.wav");
    BOOST_TEST(guestArgumentFromHost("AA#5#", exists) == "AA#5#");
    BOOST_TEST(guestArgumentFromHost("", exists) == "");
    // Pitch bend strings whose first value is negative, from the render comparison of
    // helloutau. The first names no directory, the second a file in the root.
    BOOST_TEST(guestArgumentFromHost("/N/i/y/8AA#36#ABANAtBf", exists) == "/N/i/y/8AA#36#ABANAtBf");
    BOOST_TEST(guestArgumentFromHost("/2AA#30#", exists) == "/2AA#30#");
    BOOST_TEST(
        guestArgumentFromHost("/n/r/w/0/4/9AAAEAHAJAKALAMAMALAKAJAHAGAEADABAA#54#", exists) ==
        "/n/r/w/0/4/9AAAEAHAJAKALAMAMALAKAJAHAGAEADABAA#54#");
    // The root never counts as the directory of an argument, even if every directory exists.
    auto everyDirectory = [](const std::string &path) {
        return path.find('#') == std::string::npos;
    };
    BOOST_TEST(guestArgumentFromHost("/2AA#30#", everyDirectory) == "/2AA#30#");
}

// fullpath.txt: input, success, result, final component or (null), error. Measured with
// GetFullPathNameA in a current directory whose path the results replace with <CWD> and whose
// drive they replace with <D>. The line of .. gives the parent of the current directory.
BOOST_AUTO_TEST_CASE(test_full_guest_path_matches_windows) {
    auto lines = golden::read("fullpath.txt");
    std::string parent;
    for (auto &fields : lines) {
        if (golden::unescape(fields[0]) == "..") {
            parent = golden::unescape(fields[2]);
        }
    }
    BOOST_TEST_REQUIRE(parent.compare(0, 3, "<D>") == 0);
    // Any drive other than C: serves as the current drive, because only C: is named.
    const std::string drive = "Q:";
    const std::string cwd = drive + parent.substr(3) + "\\dir";

    int checked = 0;
    for (auto &fields : lines) {
        if (fields[0] == "required" || fields[0] == "getcwd") {
            continue;
        }
        std::string input = golden::unescape(fields[0]);
        std::string expected = golden::unescape(fields[2]);
        if (expected.compare(0, 5, "<CWD>") == 0) {
            expected = cwd + expected.substr(5);
        } else if (expected.compare(0, 3, "<D>") == 0) {
            expected = drive + expected.substr(3);
        }
        auto result = fullGuestPath(input, cwd);
        BOOST_TEST_CONTEXT(fields[0]) {
            BOOST_TEST(fields[1] == "1");
            BOOST_TEST(result.path == expected);
            std::string file = result.filePart ? result.path.substr(*result.filePart) : "(null)";
            BOOST_TEST(file == golden::unescape(fields[3]));
        }
        ++checked;
    }
    BOOST_TEST(checked == 26);
}

BOOST_AUTO_TEST_SUITE_END()
