#include "CodePage.h"

#include <cstdint>

#include <stdcorelib/utf.h>

namespace more::loader {

    static constexpr char16_t replacementCharacter = stdc::utf::replacement_character;

    static void appendUtf16(std::u16string &out, char32_t codePoint) {
        if (codePoint < 0x10000) {
            out.push_back(static_cast<char16_t>(codePoint));
            return;
        }
        codePoint -= 0x10000;
        out.push_back(static_cast<char16_t>(0xD800 + (codePoint >> 10)));
        out.push_back(static_cast<char16_t>(0xDC00 + (codePoint & 0x3FF)));
    }

    static bool isContinuation(std::uint8_t byte) {
        return byte >= 0x80 && byte <= 0xBF;
    }

    Conversion<std::u16string> multiByteToWide(std::string_view text) {
        Conversion<std::u16string> result;
        std::u16string &out = result.text;
        out.reserve(text.size());

        size_t i = 0;
        auto fail = [&](size_t consumed) {
            out.push_back(replacementCharacter);
            result.invalid = true;
            i += consumed;
        };

        while (i < text.size()) {
            auto lead = static_cast<std::uint8_t>(text[i]);
            if (lead < 0x80) {
                out.push_back(lead);
                ++i;
                continue;
            }

            int length;
            std::uint8_t secondLow = 0x80;
            std::uint8_t secondHigh = 0xBF;
            char32_t codePoint;
            if (lead >= 0xC2 && lead <= 0xDF) {
                length = 2;
                codePoint = lead & 0x1F;
            } else if (lead >= 0xE0 && lead <= 0xEF) {
                length = 3;
                codePoint = lead & 0x0F;
                if (lead == 0xE0) {
                    secondLow = 0xA0;
                } else if (lead == 0xED) {
                    secondHigh = 0x9F;
                }
            } else if (lead >= 0xF0 && lead <= 0xF4) {
                length = 4;
                codePoint = lead & 0x07;
                if (lead == 0xF0) {
                    secondLow = 0x90;
                } else if (lead == 0xF4) {
                    secondHigh = 0x8F;
                }
            } else {
                fail(1);
                continue;
            }

            if (i + 1 >= text.size() || !isContinuation(static_cast<std::uint8_t>(text[i + 1]))) {
                fail(1);
                continue;
            }
            auto second = static_cast<std::uint8_t>(text[i + 1]);
            if (second < secondLow || second > secondHigh) {
                // Measured: ED A0 80 yields two replacement characters, therefore the lead byte
                // and an out-of-range continuation byte form one invalid sequence.
                fail(2);
                continue;
            }
            codePoint = (codePoint << 6) | (second & 0x3F);

            size_t j = 2;
            for (; j < size_t(length); ++j) {
                if (i + j >= text.size() ||
                    !isContinuation(static_cast<std::uint8_t>(text[i + j]))) {
                    break;
                }
                codePoint = (codePoint << 6) | (static_cast<std::uint8_t>(text[i + j]) & 0x3F);
            }
            if (j < size_t(length)) {
                fail(j);
                continue;
            }
            appendUtf16(out, codePoint);
            i += length;
        }
        return result;
    }

    Conversion<std::string> wideToMultiByte(std::u16string_view text) {
        // The encoding direction of stdcorelib agrees with WideCharToMultiByte, which replaces
        // each unpaired surrogate with U+FFFD.
        Conversion<std::string> result;
        bool ok = true;
        result.text = stdc::utf::utf16_to_utf8(text, stdc::utf::replace, &ok);
        result.invalid = !ok;
        return result;
    }

    std::size_t wholeCharacterPrefix(std::string_view text, std::size_t limit) {
        if (limit >= text.size()) {
            return text.size();
        }
        // The byte at the limit is the first byte that does not fit. The prefix ends before the
        // character that this byte belongs to.
        std::size_t length = limit;
        while (length > 0 && isContinuation(static_cast<std::uint8_t>(text[length]))) {
            --length;
        }
        return length;
    }
}
