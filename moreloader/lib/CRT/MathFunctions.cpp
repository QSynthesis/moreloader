#include "MathFunctions.h"

#include <cmath>
#include <cstdint>
#include <cstring>

#include <moreloader/Support/FloatingPoint.h>

namespace more::loader::msvcrt {

    namespace {

        std::uint64_t bitsOf(double x) {
            std::uint64_t bits;
            std::memcpy(&bits, &x, sizeof bits);
            return bits;
        }

        double fromBits(std::uint64_t bits) {
            double x;
            std::memcpy(&x, &bits, sizeof x);
            return x;
        }

    }

    long double tan(double x) {
        ExtendedPrecisionScope precision;
        long double y = x;
        if (std::fabs(x) >= 9223372036854775808.0) {
            return tanl(y);
        }
        return x87Tangent(y);
    }

    long double tanh(double x) {
        ExtendedPrecisionScope precision;
        return tanhl(x);
    }

    long double acos(double x) {
        // Measured: the double function under the control word of the guest agrees with msvcrt
        // after rounding, and the extended function does not.
        return std::acos(x);
    }

    long double asin(double x) {
        // Measured: the double function under the control word of the guest agrees with msvcrt
        // after rounding, and the extended function does not.
        return std::asin(x);
    }

    long double cosh(double x) {
        ExtendedPrecisionScope precision;
        return coshl(x);
    }

    long double sinh(double x) {
        ExtendedPrecisionScope precision;
        return sinhl(x);
    }

    long double log10(double x) {
        ExtendedPrecisionScope precision;
        return log10l(x);
    }

    double frexp(double x, int *exponent) {
        std::uint64_t bits = bitsOf(x);
        unsigned biased = unsigned(bits >> 52) & 0x7FF;
        if (biased == 0x7FF) {
            *exponent = -1;
            if ((bits << 12) == 0) {
                return fromBits(0xFFF8000000000000ull);
            }
            return x;
        }
        if ((bits << 1) == 0) {
            *exponent = 0;
            return 0.0;
        }
        return std::frexp(x, exponent);
    }

}
