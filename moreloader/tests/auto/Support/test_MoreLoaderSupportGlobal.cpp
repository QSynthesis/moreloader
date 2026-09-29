#include <cstdint>

#include <boost/test/unit_test.hpp>

#include <moreloader/Support/MoreLoaderSupportGlobal.h>

namespace {

    std::uint32_t s_alignedAddress;

    // A variable that requires 16-byte alignment. Its address is aligned only if the function
    // realigns a stack that the caller left misaligned.
    MORE_NOINLINE std::uint32_t MORE_WINAPI winapiDifference(std::uint32_t a, std::uint32_t b) {
        alignas(16) volatile std::uint8_t aligned[16] = {};
        s_alignedAddress = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(aligned));
        return a - b;
    }

    MORE_NOINLINE std::uint32_t MORE_CDECL cdeclDifference(std::uint32_t a, std::uint32_t b) {
        alignas(16) volatile std::uint8_t aligned[16] = {};
        s_alignedAddress = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(aligned));
        return a - b;
    }

    // Calls \a function with the arguments 50 and 8 from a stack that is misaligned by
    // \a misalignment bytes, and returns the result and the change of the stack pointer across
    // the call, which is 8 if the callee removed its arguments and 0 otherwise.
    struct CallResult {
        std::uint32_t value;
        std::uint32_t stackChange;
    };

    CallResult callMisaligned(void *function, std::uint32_t misalignment) {
        std::uint32_t value, before, after;
        asm volatile("movl %%esp, %%esi\n\t"
                     "andl $-16, %%esp\n\t"
                     "subl %[misalignment], %%esp\n\t"
                     "subl $8, %%esp\n\t"
                     "movl %%esp, %[before]\n\t"
                     "pushl $8\n\t"
                     "pushl $50\n\t"
                     "call *%[function]\n\t"
                     "movl %%esp, %[after]\n\t"
                     "movl %%esi, %%esp"
                     : "=a"(value), [before] "=&m"(before), [after] "=&m"(after)
                     : [function] "r"(function), [misalignment] "r"(misalignment)
                     : "esi", "ecx", "edx", "memory", "cc");
        return {value, after - before + 8};
    }

    MORE_NOINLINE void *returnAddressOf() {
        return MORE_RETURN_ADDRESS();
    }

}

BOOST_AUTO_TEST_SUITE(test_MoreLoaderSupportGlobal)

BOOST_AUTO_TEST_CASE(test_winapi_callee_removes_arguments_and_realigns) {
    for (std::uint32_t misalignment : {0u, 4u, 8u, 12u}) {
        BOOST_TEST_CONTEXT("misalignment " << misalignment) {
            CallResult result =
                callMisaligned(reinterpret_cast<void *>(&winapiDifference), misalignment);
            BOOST_TEST(result.value == 42u);
            BOOST_TEST(result.stackChange == 8u);
            BOOST_TEST(s_alignedAddress % 16 == 0u);
        }
    }
}

BOOST_AUTO_TEST_CASE(test_cdecl_caller_removes_arguments_and_callee_realigns) {
    for (std::uint32_t misalignment : {0u, 4u, 8u, 12u}) {
        BOOST_TEST_CONTEXT("misalignment " << misalignment) {
            CallResult result =
                callMisaligned(reinterpret_cast<void *>(&cdeclDifference), misalignment);
            BOOST_TEST(result.value == 42u);
            BOOST_TEST(result.stackChange == 0u);
            BOOST_TEST(s_alignedAddress % 16 == 0u);
        }
    }
}

BOOST_AUTO_TEST_CASE(test_return_address) {
    void *address = nullptr;
    void *label = nullptr;
    asm volatile("call 1f\n\t"
                 "1: popl %0"
                 : "=r"(label));
    address = returnAddressOf();
    // The return address lies in this function, after the label.
    auto distance =
        reinterpret_cast<std::uintptr_t>(address) - reinterpret_cast<std::uintptr_t>(label);
    BOOST_TEST(distance > 0u);
    BOOST_TEST(distance < 256u);
}

BOOST_AUTO_TEST_SUITE_END()
