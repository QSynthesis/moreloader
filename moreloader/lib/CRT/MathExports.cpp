#include "CRTExports_p.h"

#include "MathFunctions.h"

namespace more::loader::msvcrt {

    namespace {

        // The functions return long double, so that ST0 holds the unrounded value that msvcrt
        // returns. See MathFunctions.h.

        long double MORE_CDECL msvcrt_tan(double x) {
            return tan(x);
        }

        long double MORE_CDECL msvcrt_tanh(double x) {
            return tanh(x);
        }

        long double MORE_CDECL msvcrt_acos(double x) {
            return acos(x);
        }

        long double MORE_CDECL msvcrt_asin(double x) {
            return asin(x);
        }

        long double MORE_CDECL msvcrt_cosh(double x) {
            return cosh(x);
        }

        long double MORE_CDECL msvcrt_sinh(double x) {
            return sinh(x);
        }

        long double MORE_CDECL msvcrt_log10(double x) {
            return log10(x);
        }

        double MORE_CDECL msvcrt_frexp(double x, std::int32_t *exponent) {
            int e = 0;
            double m = frexp(x, &e);
            *exponent = e;
            return m;
        }

    }

    void registerMathExports(ExportRegistry &registry) {
        MORE_REGISTER(registry, msvcrt, tan);
        MORE_REGISTER(registry, msvcrt, tanh);
        MORE_REGISTER(registry, msvcrt, acos);
        MORE_REGISTER(registry, msvcrt, asin);
        MORE_REGISTER(registry, msvcrt, cosh);
        MORE_REGISTER(registry, msvcrt, sinh);
        MORE_REGISTER(registry, msvcrt, log10);
        MORE_REGISTER(registry, msvcrt, frexp);
    }

}
