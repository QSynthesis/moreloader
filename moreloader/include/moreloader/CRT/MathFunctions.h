#ifndef MORELOADER_CRT_MATHFUNCTIONS_H
#define MORELOADER_CRT_MATHFUNCTIONS_H

namespace more::loader::msvcrt {

    /// The mathematical functions that moresampler imports from msvcrt. The remaining
    /// mathematical functions of moresampler are linked statically into its image.
    ///
    /// msvcrt computes these functions on the x87 stack with 64-bit precision and returns the
    /// unrounded 80-bit value in ST0, and the guest continues computing with that register.
    /// Therefore the functions return \c long \c double, and those computed in extended
    /// precision set the precision control to 64 bits, restoring the control word of the guest
    /// afterwards.
    ///
    /// Measured agreement with msvcrt.dll (math.txt and math-st0.txt):
    ///
    /// - \c log10 and \c tan agree in all 80 bits. \c tan is the instruction \c fptan, which
    ///   msvcrt also uses. For arguments of magnitude 2^63 or more, which \c fptan rejects, the
    ///   reduction of msvcrt is not reproduced.
    /// - \c sinh, \c cosh and \c tanh agree after rounding to a double, but not in all 80 bits,
    ///   because msvcrt uses its own algorithms. moresampler calls \c sinh at 0x433836 with
    ///   arguments k / 6 and continues on the x87 stack before rounding to a float.
    /// - \c acos and \c asin agree after rounding to a double if computed in double precision
    ///   under the control word of the guest, and are computed so. Their 80-bit results are not
    ///   reproduced.
    ///
    /// \sa moreloader/tests/auto/data/msvcrt/math.txt
    long double tan(double x);
    long double tanh(double x);
    long double acos(double x);
    long double asin(double x);
    long double cosh(double x);
    long double sinh(double x);
    long double log10(double x);

    /// Splits \a x as \c frexp of msvcrt does. Measured differences from glibc: a negative zero
    /// yields a positive zero, and an infinity yields the indefinite NaN with exponent -1, as
    /// does a NaN.
    double frexp(double x, int *exponent);

}

#endif // MORELOADER_CRT_MATHFUNCTIONS_H
