#include "CharacterType.h"

#include <algorithm>
#include <array>

namespace more::loader {

    namespace {

        constexpr char16_t replacementCharacter = 0xFFFD;

        bool isLatin1UpperLetter(char16_t unit) {
            return (unit >= u'A' && unit <= u'Z') || (unit >= 0xC0 && unit <= 0xDE && unit != 0xD7);
        }

        bool isLatin1LowerLetter(char16_t unit) {
            // U+00DF, U+00AA, U+00B5 and U+00BA are lowercase letters without an uppercase
            // counterpart in Latin-1, except U+00FF, which maps to U+0178.
            return (unit >= u'a' && unit <= u'z') || (unit >= 0xDF && unit <= 0xFF && unit != 0xF7);
        }

        // The types of U+0000 to U+00FF as measured with GetStringTypeW(CT_CTYPE1).
        std::uint16_t latin1Type(char16_t unit) {
            if (unit == u'\t') {
                return C1Cntrl | C1Space | C1Blank | C1Defined;
            }
            if ((unit >= 0x0A && unit <= 0x0D) || unit == 0x85) {
                return C1Cntrl | C1Space | C1Defined;
            }
            if (unit < 0x20 || (unit >= 0x7F && unit <= 0x9F)) {
                return C1Cntrl | C1Defined;
            }
            if (unit == u' ' || unit == 0xA0) {
                return C1Space | C1Blank | C1Defined;
            }
            if (unit >= u'0' && unit <= u'9') {
                return C1Digit | C1XDigit | C1Defined;
            }
            if (isLatin1UpperLetter(unit)) {
                std::uint16_t hex = (unit >= u'A' && unit <= u'F') ? C1XDigit : 0;
                return C1Upper | C1Alpha | C1Defined | hex;
            }
            if (unit == 0xAA || unit == 0xB5 || unit == 0xBA) {
                return C1Lower | C1Punct | C1Alpha | C1Defined;
            }
            if (isLatin1LowerLetter(unit)) {
                std::uint16_t hex = (unit >= u'a' && unit <= u'f') ? C1XDigit : 0;
                return C1Lower | C1Alpha | C1Defined | hex;
            }
            if (unit == 0xAD) {
                return C1Punct | C1Cntrl | C1Defined;
            }
            if (unit == 0xB2 || unit == 0xB3 || unit == 0xB9) {
                return C1Digit | C1Punct | C1Defined;
            }
            return C1Punct | C1Defined;
        }

        // The single units U+0001 to U+007F in the order of CompareStringW with NORM_IGNORECASE,
        // as measured with the locales 0 and 0x804. Lowercase letters rank with their uppercase
        // counterparts.
        constexpr char sortOrder[] = "\x01\x02\x03\x04\x05\x06\x07\x08\x0E\x0F\x10\x11\x12\x13\x14"
                                     "\x15\x16\x17\x18\x19\x1A\x1B\x1C\x1D\x1E\x1F\x7F'- \t\n\v\f"
                                     "\r!\"#$%&()*,./:;?@[\\]^_`{|}~+<=>0123456789"
                                     "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

        char16_t foldCase(char16_t unit) {
            return unit >= u'a' && unit <= u'z' ? char16_t(unit - 0x20) : unit;
        }

        int rankOf(char16_t unit) {
            static const auto ranks = [] {
                std::array<int, 0x80> table{};
                for (std::size_t i = 0; i + 1 < sizeof sortOrder; ++i) {
                    table[static_cast<unsigned char>(sortOrder[i])] = int(i);
                }
                return table;
            }();
            return ranks[foldCase(unit)];
        }

        bool isSeparatelyWeighted(char16_t unit) {
            return unit < 0x20 || unit == 0x7F || unit == u'\'' || unit == u'-';
        }

    }

    std::optional<std::uint16_t> characterType1Of(char16_t unit) {
        if (unit <= 0xFF) {
            return latin1Type(unit);
        }
        if (unit == replacementCharacter) {
            return C1Defined;
        }
        return std::nullopt;
    }

    std::optional<char16_t> lowerCaseOf(char16_t unit) {
        if (unit <= 0xFF) {
            return isLatin1UpperLetter(unit) ? char16_t(unit + 0x20) : unit;
        }
        if (unit == replacementCharacter) {
            return unit;
        }
        return std::nullopt;
    }

    std::optional<char16_t> upperCaseOf(char16_t unit) {
        if (unit == 0xFF) {
            return char16_t(0x178);
        }
        if (unit <= 0xFF) {
            bool mapped =
                (unit >= u'a' && unit <= u'z') || (unit >= 0xE0 && unit <= 0xFE && unit != 0xF7);
            return mapped ? char16_t(unit - 0x20) : unit;
        }
        if (unit == replacementCharacter) {
            return unit;
        }
        return std::nullopt;
    }

    std::optional<int> compareIgnoringCase(std::u16string_view a, std::u16string_view b) {
        bool separate = false;
        for (std::u16string_view text : {a, b}) {
            for (char16_t unit : text) {
                if (unit == 0 || unit >= 0x80) {
                    return std::nullopt;
                }
                separate = separate || isSeparatelyWeighted(unit);
            }
        }
        std::size_t length = std::min(a.size(), b.size());
        std::size_t i = 0;
        while (i < length && foldCase(a[i]) == foldCase(b[i])) {
            ++i;
        }
        if (i == a.size() && i == b.size()) {
            return 0;
        }
        if (separate) {
            return std::nullopt;
        }
        if (i == length) {
            return a.size() < b.size() ? -1 : 1;
        }
        return rankOf(a[i]) < rankOf(b[i]) ? -1 : 1;
    }

}
