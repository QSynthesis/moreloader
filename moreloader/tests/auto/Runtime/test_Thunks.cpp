#include <sys/mman.h>

#include <cstdint>
#include <cstring>
#include <string>

#include <boost/test/unit_test.hpp>

#include <moreloader/Runtime/Thunks.h>
#include <moreloader/Support/MoreLoaderSupportGlobal.h>

#include "ProcessTools.h"

using namespace more::loader;

namespace {

    std::uint32_t addressOf(const void *p) {
        return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p));
    }

    int MORE_STDCALL subtract(int a, int b) {
        return a - b;
    }

    // Returns the address of executable code that computes ecx + edx into eax, so that the
    // result shows whether a thunk preserves the registers of its caller.
    std::uint32_t sumOfRegisters() {
        static std::uint32_t code = [] {
            void *page = ::mmap(nullptr, 0x1000, PROT_READ | PROT_WRITE | PROT_EXEC,
                                MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            // lea eax, [ecx+edx], then ret.
            static const std::uint8_t bytes[] = {0x8D, 0x04, 0x11, 0xC3};
            std::memcpy(page, bytes, sizeof bytes);
            return addressOf(page);
        }();
        return code;
    }

    std::uint32_t callWithRegisters(std::uint32_t target, std::uint32_t ecx, std::uint32_t edx) {
        std::uint32_t result;
        asm volatile("call *%3"
                     : "=a"(result), "+c"(ecx), "+d"(edx)
                     : "r"(target)
                     : "memory", "cc");
        return result;
    }

}

BOOST_AUTO_TEST_SUITE(test_Thunks)

BOOST_AUTO_TEST_CASE(test_trace_thunk_forwards_the_call) {
    std::uint32_t thunk =
        makeTraceThunk("testlib.dll", "Subtract", addressOf(reinterpret_cast<void *>(&subtract)));
    int result = 0;
    std::string output = testing::captureStderr([&] {
        auto function = reinterpret_cast<int(MORE_STDCALL *)(int, int)>(thunk);
        result = function(50, 8);
    });
    BOOST_TEST(result == 42);
    // No guest thread runs on the test thread, which is reported as thread 0.
    BOOST_TEST(output == "moreloader: [0] testlib.dll!Subtract\n");
}

BOOST_AUTO_TEST_CASE(test_trace_thunk_preserves_registers) {
    std::uint32_t thunk = makeTraceThunk("testlib.dll", "Sum", sumOfRegisters());
    std::uint32_t result = 0;
    std::string output =
        testing::captureStderr([&] { result = callWithRegisters(thunk, 0x12340000, 0x00005678); });
    BOOST_TEST(result == 0x12345678u);
    BOOST_TEST(output == "moreloader: [0] testlib.dll!Sum\n");
    // The target alone, for comparison.
    BOOST_TEST(callWithRegisters(sumOfRegisters(), 1, 2) == 3u);
}

BOOST_AUTO_TEST_CASE(test_unresolved_thunk_terminates_the_process) {
    std::uint32_t thunk = makeUnresolvedThunk("testlib.dll", "Missing");
    testing::ChildResult child = testing::runInChild([&] {
        reinterpret_cast<void (*)()>(thunk)();
        std::fprintf(stderr, "returned\n");
    });
    BOOST_TEST(child.exitCode == 255);
    BOOST_TEST(child.errorOutput ==
               "moreloader: thread 0 called testlib.dll!Missing, which is not implemented\n");
}

BOOST_AUTO_TEST_SUITE_END()
