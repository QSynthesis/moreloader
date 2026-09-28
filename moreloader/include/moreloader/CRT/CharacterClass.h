#ifndef MORELOADER_CRT_CHARACTERCLASS_H
#define MORELOADER_CRT_CHARACTERCLASS_H

namespace more::loader::msvcrt {

    /// Character classes of the C locale of msvcrt, as bits.
    enum CharacterClass {
        ClassUpper = 0x1,
        ClassLower = 0x2,
        ClassDigit = 0x4,
        ClassSpace = 0x8,
        ClassPunct = 0x10,
        ClassControl = 0x20,
        ClassBlank = 0x40,
        ClassHex = 0x80,
    };

    /// Returns the classes of \a c in the C locale of msvcrt.
    ///
    /// Only ASCII characters are classified. Values from 128 to 255 and negative values have no
    /// class, as measured for msvcrt, which is not the case for glibc in every locale.
    ///
    /// \sa moreloader/tests/auto/data/msvcrt/ctype.txt
    int characterClass(int c);

    /// Returns \a c converted by \c tolower or \c toupper of msvcrt in the C locale. Only the
    /// ASCII letters are converted.
    int toLower(int c);
    int toUpper(int c);

}

#endif // MORELOADER_CRT_CHARACTERCLASS_H
