#include <cstddef>
#include <map>
#include <string>

#include <boost/test/unit_test.hpp>

#include <moreloader/Runtime/GuestLayout.h>

#include "GoldenData.h"

using namespace more::loader;

BOOST_AUTO_TEST_SUITE(test_GuestLayout)

// layout.txt: sizes and offsets measured on Windows by MsvcrtProbe.cpp. Names ending in .sizeof
// are sizes of SDK structures, TEB.<field> and TEB.<field>.runtime are offsets in the TEB from
// the SDK and from values found in the TEB of a running thread, and PEB.<field>.runtime is an
// offset in the PEB found the same way.
BOOST_AUTO_TEST_CASE(test_layout_matches_windows) {
    const std::map<std::string, std::size_t> ours = {
        {"CRITICAL_SECTION.sizeof", sizeof(CRITICAL_SECTION32)},
        {"MEMORY_BASIC_INFORMATION.sizeof", sizeof(MEMORY_BASIC_INFORMATION32)},
        {"FILETIME.sizeof", sizeof(FILETIME32)},
        {"WIN32_FIND_DATAW.sizeof", sizeof(WIN32_FIND_DATAW32)},
        {"WIN32_FIND_DATAW.cFileName", offsetof(WIN32_FIND_DATAW32, cFileName)},
        {"WIN32_FIND_DATAA.sizeof", sizeof(WIN32_FIND_DATAA32)},
        {"WIN32_FIND_DATAA.cFileName", offsetof(WIN32_FIND_DATAA32, cFileName)},
        {"SYSTEMTIME.sizeof", sizeof(SYSTEMTIME32)},
        {"TIME_ZONE_INFORMATION.sizeof", sizeof(TIME_ZONE_INFORMATION32)},
        {"OVERLAPPED.sizeof", sizeof(OVERLAPPED32)},
        {"STARTUPINFOA.sizeof", sizeof(STARTUPINFOA32)},
        {"OSVERSIONINFOA.sizeof", sizeof(OSVERSIONINFOA32)},
        {"CPINFO.sizeof", sizeof(CPINFO32)},
        {"TEB.ExceptionList", offsetof(TEB32, ExceptionList)},
        {"TEB.StackBase", offsetof(TEB32, StackBase)},
        {"TEB.StackLimit", offsetof(TEB32, StackLimit)},
        {"TEB.Self", offsetof(TEB32, Self)},
        {"TEB.Self.runtime", offsetof(TEB32, Self)},
        {"TEB.UniqueProcess.runtime", offsetof(TEB32, UniqueProcess)},
        {"TEB.UniqueThread.runtime", offsetof(TEB32, UniqueThread)},
        {"TEB.ThreadLocalStoragePointer.runtime", offsetof(TEB32, ThreadLocalStoragePointer)},
        {"TEB.ProcessEnvironmentBlock", offsetof(TEB32, ProcessEnvironmentBlock)},
        {"PEB.ImageBaseAddress.runtime", offsetof(PEB32, ImageBaseAddress)},
        {"TEB.LastErrorValue.runtime", offsetof(TEB32, LastErrorValue)},
        {"TEB.TlsSlots", offsetof(TEB32, TlsSlots)},
        {"TEB.TlsSlots.runtime", offsetof(TEB32, TlsSlots)},
        {"TEB.TlsExpansionSlots", offsetof(TEB32, TlsExpansionSlots)},
        {"TEB.TlsExpansionSlots.runtime", offsetof(TEB32, TlsExpansionSlots)},
    };
    std::size_t checked = 0;
    for (auto &fields : golden::read("layout.txt")) {
        BOOST_TEST_CONTEXT(fields[0]) {
            auto it = ours.find(fields[0]);
            BOOST_TEST_REQUIRE((it != ours.end()));
            BOOST_TEST(std::stol(fields[1]) == long(it->second));
        }
        ++checked;
    }
    BOOST_TEST(checked == ours.size());
}

BOOST_AUTO_TEST_CASE(test_tls_slots) {
    BOOST_TEST(sizeof(TEB32::TlsSlots) / sizeof(TEB32::TlsSlots[0]) == tlsMinimumAvailable);
}

BOOST_AUTO_TEST_SUITE_END()
