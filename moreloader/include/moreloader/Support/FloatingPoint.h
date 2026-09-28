#ifndef MORELOADER_SUPPORT_FLOATINGPOINT_H
#define MORELOADER_SUPPORT_FLOATINGPOINT_H

namespace more::loader {

    /// Initial x87 control word of a Windows process: all exceptions masked, 53-bit precision,
    /// rounding to nearest. The initial control word of a Linux process is 0x37F, which selects
    /// 64-bit precision and changes the results of x87 arithmetic in the guest.
    constexpr unsigned short windowsControlWord = 0x27F;

    /// Initial value of MXCSR in both Windows and Linux: all exceptions masked, rounding to
    /// nearest.
    constexpr unsigned int windowsMXCSR = 0x1F80;

    /// Returns the x87 control word of the calling thread.
    unsigned short x87ControlWord();

    /// Sets the x87 control word of the calling thread.
    void setX87ControlWord(unsigned short controlWord);

    /// Sets MXCSR of the calling thread.
    void setMXCSR(unsigned int mxcsr);

    /// Returns the tangent of \a x computed by the instruction \c fptan. \a x must be less than
    /// 2^63 in magnitude.
    long double x87Tangent(long double x);

    /// Sets the precision control of the x87 control word to 64 bits for the lifetime of the
    /// object and restores the previous control word afterwards.
    class ExtendedPrecisionScope {
    public:
        ExtendedPrecisionScope();
        ~ExtendedPrecisionScope();

        ExtendedPrecisionScope(const ExtendedPrecisionScope &) = delete;
        ExtendedPrecisionScope &operator=(const ExtendedPrecisionScope &) = delete;

    private:
        unsigned short m_saved;
    };

}

#endif // MORELOADER_SUPPORT_FLOATINGPOINT_H
