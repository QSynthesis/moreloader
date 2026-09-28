#include "FloatingPoint.h"

#if !defined(__GNUC__) || !defined(__i386__)
#  error "The x87 functions are implemented for GCC and Clang targeting i386."
#endif

namespace more::loader {

    unsigned short x87ControlWord() {
        unsigned short controlWord;
        asm volatile("fnstcw %0" : "=m"(controlWord));
        return controlWord;
    }

    void setX87ControlWord(unsigned short controlWord) {
        asm volatile("fldcw %0" : : "m"(controlWord));
    }

    void setMXCSR(unsigned int mxcsr) {
        asm volatile("ldmxcsr %0" : : "m"(mxcsr));
    }

    long double x87Tangent(long double x) {
        long double result;
        // fptan pushes 1.0 above the tangent, which is discarded.
        asm("fptan\n\tfstp %%st(0)" : "=t"(result) : "0"(x));
        return result;
    }

    ExtendedPrecisionScope::ExtendedPrecisionScope() : m_saved(x87ControlWord()) {
        setX87ControlWord(static_cast<unsigned short>(m_saved | 0x0300));
    }

    ExtendedPrecisionScope::~ExtendedPrecisionScope() {
        setX87ControlWord(m_saved);
    }

}
