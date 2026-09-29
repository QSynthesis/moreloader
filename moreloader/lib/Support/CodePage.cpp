#include "CodePage.h"

#include <cstdint>

namespace more::loader {

    static constexpr char16_t replacementCharacter = 0xFFFD;

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

    static void appendUtf8(std::string &out, char32_t codePoint) {
        if (codePoint < 0x80) {
            out.push_back(static_cast<char>(codePoint));
        } else if (codePoint < 0x800) {
            out.push_back(static_cast<char>(0xC0 | (codePoint >> 6)));
            out.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
        } else if (codePoint < 0x10000) {
            out.push_back(static_cast<char>(0xE0 | (codePoint >> 12)));
            out.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xF0 | (codePoint >> 18)));
            out.push_back(static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
        }
    }

    Conversion<std::string> wideToMultiByte(std::u16string_view text) {
        Conversion<std::string> result;
        std::string &out = result.text;
        out.reserve(text.size());
        for (std::size_t i = 0; i < text.size(); ++i) {
            char16_t unit = text[i];
            bool high = unit >= 0xD800 && unit <= 0xDBFF;
            bool low = unit >= 0xDC00 && unit <= 0xDFFF;
            if (high && i + 1 < text.size() && text[i + 1] >= 0xDC00 && text[i + 1] <= 0xDFFF) {
                char32_t codePoint = 0x10000 + ((char32_t(unit) - 0xD800) << 10) +
                                     (char32_t(text[i + 1]) - 0xDC00);
                appendUtf8(out, codePoint);
                ++i;
            } else if (high || low) {
                // Each unpaired surrogate becomes U+FFFD, as measured.
                appendUtf8(out, replacementCharacter);
                result.invalid = true;
            } else {
                appendUtf8(out, unit);
            }
        }
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
