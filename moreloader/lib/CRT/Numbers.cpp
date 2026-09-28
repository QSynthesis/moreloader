#include "Numbers.h"

#include <cstdlib>
#include <cstring>
#include <string>

namespace more::loader::msvcrt {

    namespace {

        // White space of the C locale.
        template <class Char>
        bool isSpace(Char c) {
            return c == Char(' ') || (c >= Char('\t') && c <= Char('\r'));
        }

        template <class Char>
        int digitValue(Char c) {
            if (c >= Char('0') && c <= Char('9')) {
                return int(c - Char('0'));
            }
            if (c >= Char('a') && c <= Char('z')) {
                return int(c - Char('a')) + 10;
            }
            if (c >= Char('A') && c <= Char('Z')) {
                return int(c - Char('A')) + 10;
            }
            return 99;
        }

        // The conversion of strtoxl of the Microsoft C runtime: the magnitude is accumulated as
        // an unsigned value with overflow detection, then clamped, then negated.
        template <class Char>
        IntegerParse parseInteger(const Char *text, int base, bool isUnsigned) {
            IntegerParse result;
            const Char *p = text;
            while (isSpace(*p)) {
                ++p;
            }
            bool negative = false;
            if (*p == Char('-')) {
                negative = true;
                ++p;
            } else if (*p == Char('+')) {
                ++p;
            }

            if (base < 0 || base == 1 || base > 36) {
                return result;
            }
            if (base == 0) {
                if (*p != Char('0')) {
                    base = 10;
                } else if (p[1] == Char('x') || p[1] == Char('X')) {
                    base = 16;
                } else {
                    base = 8;
                }
            }
            if (base == 16 && *p == Char('0') && (p[1] == Char('x') || p[1] == Char('X'))) {
                p += 2;
            }

            const std::uint32_t maxValue = 0xFFFFFFFFu / std::uint32_t(base);
            const std::uint32_t maxDigit = 0xFFFFFFFFu % std::uint32_t(base);
            std::uint32_t number = 0;
            bool readDigit = false;
            for (;; ++p) {
                int digit = digitValue(*p);
                if (digit >= base) {
                    break;
                }
                readDigit = true;
                if (number < maxValue ||
                    (number == maxValue && std::uint32_t(digit) <= maxDigit)) {
                    number = number * std::uint32_t(base) + std::uint32_t(digit);
                } else {
                    result.overflow = true;
                }
            }

            if (!readDigit) {
                // The end pointer returns to the start of the text, including the white space
                // and the sign.
                return result;
            }
            if (!isUnsigned && !result.overflow &&
                ((negative && number > 0x80000000u) || (!negative && number > 0x7FFFFFFFu))) {
                result.overflow = true;
            }
            if (result.overflow) {
                if (isUnsigned) {
                    number = 0xFFFFFFFFu;
                } else if (negative) {
                    number = 0x80000000u;
                } else {
                    number = 0x7FFFFFFFu;
                }
            }
            if (negative) {
                number = 0u - number;
            }
            result.value = number;
            result.consumed = std::size_t(p - text);
            return result;
        }

    }

    IntegerParse strtol(const char *text, int base) {
        return parseInteger(text, base, false);
    }

    IntegerParse strtol(const char16_t *text, int base) {
        return parseInteger(text, base, false);
    }

    IntegerParse strtoul(const char *text, int base) {
        return parseInteger(text, base, true);
    }

    std::uint64_t atof(const char *text) {
        // Extracts the longest prefix in the syntax of msvcrt, so that glibc does not interpret
        // hexadecimal numbers, infinities and NaNs.
        const char *p = text;
        while (isSpace(*p)) {
            ++p;
        }
        std::string number;
        if (*p == '-' || *p == '+') {
            number.push_back(*p++);
        }
        bool digits = false;
        while (*p >= '0' && *p <= '9') {
            number.push_back(*p++);
            digits = true;
        }
        if (*p == '.') {
            number.push_back(*p++);
            while (*p >= '0' && *p <= '9') {
                number.push_back(*p++);
                digits = true;
            }
        }
        double value = 0;
        if (digits) {
            if (*p == 'e' || *p == 'E') {
                const char *q = p + 1;
                std::string exponent = "e";
                if (*q == '-' || *q == '+') {
                    exponent.push_back(*q++);
                }
                bool exponentDigits = false;
                while (*q >= '0' && *q <= '9') {
                    exponent.push_back(*q++);
                    exponentDigits = true;
                }
                if (exponentDigits) {
                    number += exponent;
                }
            }
            value = std::strtod(number.c_str(), nullptr);
        }
        // Measured: text without digits yields a positive zero, even after a minus sign.

        std::uint64_t bits;
        std::memcpy(&bits, &value, sizeof bits);
        return bits;
    }

}
