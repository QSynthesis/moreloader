#include <cmath>
#include <cstring>
#include <map>
#include <string>

#include <boost/test/unit_test.hpp>

#include <moreloader/CRT/MathFunctions.h>
#include <moreloader/Support/FloatingPoint.h>

#include "GoldenData.h"

using namespace more::loader;

namespace {

    double fromBits(std::uint64_t bits) {
        double value;
        std::memcpy(&value, &bits, sizeof value);
        return value;
    }

    std::uint64_t toBits(double value) {
        std::uint64_t bits;
        std::memcpy(&bits, &value, sizeof bits);
        return bits;
    }

    // Sets the x87 control word for the lifetime of the object. The guest runs with the
    // control word of Windows.
    class ControlWord {
    public:
        explicit ControlWord(unsigned short word) : m_saved(x87ControlWord()) {
            setX87ControlWord(word);
        }

        ~ControlWord() {
            setX87ControlWord(m_saved);
        }

    private:
        unsigned short m_saved;
    };

}

BOOST_AUTO_TEST_SUITE(test_MathFunctions)

// math.txt: function, bits of the argument, bits of the result; frexp lines add the exponent.
BOOST_AUTO_TEST_CASE(test_matches_msvcrt) {
    ControlWord controlWord(windowsControlWord);

    using Function = long double (*)(double);
    const std::map<std::string, Function> functions = {
        {"tan", msvcrt::tan},   {"tanh", msvcrt::tanh}, {"acos", msvcrt::acos}, {"asin", msvcrt::asin},
        {"cosh", msvcrt::cosh}, {"sinh", msvcrt::sinh}, {"log10", msvcrt::log10},
    };

    std::map<std::string, int> mismatches;
    int checked = 0;
    for (auto &fields : golden::read("math.txt")) {
        if (fields[0] == "controlfp") {
            continue;
        }
        double x = fromBits(golden::bits(fields[1]));
        if (fields[0] == "tan" && std::fabs(x) >= 9223372036854775808.0 && std::isfinite(x)) {
            // The reduction of msvcrt for arguments beyond fptan is not reproduced, as declared
            // in MathFunctions.h.
            continue;
        }
        std::uint64_t expected = golden::bits(fields[2]);
        std::uint64_t actual;
        if (fields[0] == "frexp") {
            int exponent = 0;
            actual = toBits(msvcrt::frexp(x, &exponent));
            if (exponent != std::stoi(fields[3])) {
                ++mismatches["frexp exponent"];
            }
        } else {
            actual = toBits(double(functions.at(fields[0])(x)));
        }
        // NaN results are compared as NaN, because the payload is not part of the contract.
        bool bothNaN = ((expected >> 52) & 0x7FF) == 0x7FF && (expected << 12) != 0 &&
                       ((actual >> 52) & 0x7FF) == 0x7FF && (actual << 12) != 0;
        if (actual != expected && !bothNaN) {
            if (++mismatches[fields[0]] <= 3) {
                BOOST_TEST_MESSAGE(fields[0] << "(" << fields[1] << "): msvcrt " << fields[2]
                                             << ", host " << std::hex << actual);
            }
        }
        ++checked;
    }
    for (auto &[name, count] : mismatches) {
        BOOST_ERROR(name << ": " << count << " mismatches");
    }
    BOOST_TEST(checked == 8 * 820 - 1);
}

// moresampler calls sinh at 0x433836 with k / 6 for integer k and stores the scaled result as a
// float. This case measures whether the deviations of the double result survive that rounding.
BOOST_AUTO_TEST_CASE(test_sinh_at_bark_scale_arguments) {
    ControlWord controlWord(windowsControlWord);

    std::map<std::uint64_t, std::uint64_t> expected;
    for (auto &fields : golden::read("math.txt")) {
        if (fields[0] == "sinh") {
            expected[golden::bits(fields[1])] = golden::bits(fields[2]);
        }
    }

    int doubleMismatches = 0;
    int floatMismatches = 0;
    int checked = 0;
    for (int k = 0; k <= 400; ++k) {
        volatile double sixth = 1.0 / 6.0;
        double x = k * sixth;
        auto it = expected.find(toBits(x));
        BOOST_TEST_REQUIRE((it != expected.end()), "argument " << k << "/6 not in math.txt");
        double reference = fromBits(it->second);
        double actual = double(msvcrt::sinh(x));
        if (toBits(actual) != it->second) {
            ++doubleMismatches;
        }
        if (float(actual) != float(reference)) {
            ++floatMismatches;
            BOOST_TEST_MESSAGE("sinh(" << k << "/6) differs after rounding to float");
        }
        ++checked;
    }
    BOOST_TEST_MESSAGE("sinh(k/6): " << doubleMismatches << " double and " << floatMismatches
                                      << " float mismatches of " << checked);
    BOOST_TEST(checked == 401);
    BOOST_TEST(floatMismatches == 0);
}

BOOST_AUTO_TEST_SUITE_END()
