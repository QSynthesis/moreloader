#include "CharacterClass.h"

namespace more::loader::msvcrt {

    int characterClass(int c) {
        if (c < 0 || c > 127) {
            return 0;
        }
        int classes = 0;
        if (c >= 'A' && c <= 'Z') {
            classes |= ClassUpper;
        }
        if (c >= 'a' && c <= 'z') {
            classes |= ClassLower;
        }
        if (c >= '0' && c <= '9') {
            classes |= ClassDigit;
        }
        if (c == ' ' || (c >= '\t' && c <= '\r')) {
            classes |= ClassSpace;
        }
        if (c < 0x20 || c == 0x7F) {
            classes |= ClassControl;
        }
        if (c == ' ' || c == '\t') {
            classes |= ClassBlank;
        }
        if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')) {
            classes |= ClassHex;
        }
        if (c > 0x20 && c < 0x7F && !(classes & (ClassUpper | ClassLower | ClassDigit))) {
            classes |= ClassPunct;
        }
        return classes;
    }

    int toLower(int c) {
        return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c;
    }

    int toUpper(int c) {
        return c >= 'a' && c <= 'z' ? c - ('a' - 'A') : c;
    }

}
