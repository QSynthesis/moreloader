#include "Format.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace more::loader::msvcrt {

    std::uint32_t GuestArguments::nextUInt32() {
        std::uint32_t value;
        std::memcpy(&value, m_next, sizeof value);
        m_next += sizeof value;
        return value;
    }

    std::uint64_t GuestArguments::nextUInt64() {
        std::uint64_t value;
        std::memcpy(&value, m_next, sizeof value);
        m_next += sizeof value;
        return value;
    }

    namespace {

        enum FormatFlag {
            FlagLeft = 1,
            FlagPlus = 2,
            FlagSpace = 4,
            FlagAlternate = 8,
            FlagLeadZero = 16,
        };

        // Parsed conversion specification.
        struct Specification {
            int flags = 0;
            int width = 0;
            int precision = -1;
            bool shortInteger = false;
            bool longLong = false;
            // Set by l and w. Selects a wide character or string for c, C, s and S.
            bool forceWide = false;
            // Set by h. Selects a narrow character or string for c, C, s and S.
            bool forceNarrow = false;
        };

        // Decimal representation of a double as msvcrt produces it before formatting: at most
        // 17 significant digits and the position of the decimal point, such that the value is
        // 0.d1d2d3... times 10 to the power decimalPoint. Infinities and NaNs are represented by
        // the strings of msvcrt with the decimal point after the first character.
        struct Decimal {
            std::string mantissa;
            int decimalPoint = 0;
            bool negative = false;
            bool zero = false;
        };

        // The value is passed as its bits throughout. Loading a signaling NaN into an x87
        // register converts it into a quiet NaN, which would change %f from 1.#SNAN to 1.#QNAN.
        Decimal decompose(std::uint64_t bits) {
            Decimal d;
            d.negative = (bits >> 63) != 0;
            unsigned exponent = unsigned(bits >> 52) & 0x7FF;
            std::uint64_t fraction = bits & ((std::uint64_t(1) << 52) - 1);

            if (exponent == 0x7FF) {
                d.decimalPoint = 1;
                if (fraction == 0) {
                    d.mantissa = "1#INF";
                } else if (bits == 0xFFF8000000000000ull) {
                    d.mantissa = "1#IND";
                } else if (fraction & (std::uint64_t(1) << 51)) {
                    d.mantissa = "1#QNAN";
                } else {
                    d.mantissa = "1#SNAN";
                }
                return d;
            }
            if (exponent == 0 && fraction == 0) {
                // Measured: %#g of zero yields 0.000000, which requires a decimal point of 0.
                d.mantissa = "0";
                d.decimalPoint = 0;
                d.zero = true;
                return d;
            }

            // msvcrt keeps 17 significant digits. Measured: they are the correctly rounded digits,
            // except that a value exactly halfway between two 17-digit representations is
            // rounded down, in 120 of 120 constructed cases regardless of the parity of the last
            // digit. glibc prints the exact expansion, of which 60 digits decide the rounding. A
            // value that is not halfway but agrees with a halfway value in 60 digits does not
            // occur among doubles of practical magnitude.
            //
            // The value is finite here, therefore it can be passed by value.
            double magnitude;
            std::uint64_t magnitudeBits = bits & ~(std::uint64_t(1) << 63);
            std::memcpy(&magnitude, &magnitudeBits, sizeof magnitude);
            char buffer[96];
            std::snprintf(buffer, sizeof buffer, "%.59e", magnitude);
            std::string exact(1, buffer[0]);
            exact.append(buffer + 2, 59);
            d.decimalPoint = std::atoi(std::strchr(buffer, 'e') + 1) + 1;

            d.mantissa = exact.substr(0, 17);
            bool roundUp = false;
            if (exact[17] > '5') {
                roundUp = true;
            } else if (exact[17] == '5') {
                roundUp = exact.find_first_not_of('0', 18) != std::string::npos;
            }
            if (roundUp) {
                int k = 16;
                while (k >= 0 && d.mantissa[k] == '9') {
                    d.mantissa[k] = '0';
                    --k;
                }
                if (k >= 0) {
                    ++d.mantissa[k];
                } else {
                    d.mantissa.insert(0, 1, '1');
                    d.mantissa.pop_back();
                    ++d.decimalPoint;
                }
            }
            return d;
        }

        // Keeps the first \a count characters of the mantissa and rounds half up on the next
        // character, as _fptostr of msvcrt does. The comparison is made on characters, therefore
        // the strings of infinities and NaNs are rounded as well, which yields 1.#J for %.2f of
        // an infinity. A carry out of the first digit increments the decimal point.
        std::string roundDigits(const std::string &mantissa, int count, int &decimalPoint) {
            if (count < 0) {
                return {};
            }
            std::string buffer = "0";
            for (int i = 0; i < count; ++i) {
                buffer.push_back(size_t(i) < mantissa.size() ? mantissa[i] : '0');
            }
            char next = size_t(count) < mantissa.size() ? mantissa[count] : '\0';
            if (next >= '5') {
                size_t k = buffer.size() - 1;
                while (buffer[k] == '9') {
                    buffer[k] = '0';
                    --k;
                }
                ++buffer[k];
            }
            if (buffer[0] == '1') {
                ++decimalPoint;
                return buffer;
            }
            return buffer.substr(1);
        }

        char digitAt(const std::string &digits, int index) {
            return index >= 0 && size_t(index) < digits.size() ? digits[index] : '0';
        }

        std::string fixedBody(const std::string &digits, int decimalPoint, int precision,
                              bool alternate) {
            std::string body;
            if (decimalPoint > 0) {
                for (int k = 0; k < decimalPoint; ++k) {
                    body.push_back(digitAt(digits, k));
                }
            } else {
                body.push_back('0');
            }
            if (precision > 0 || alternate) {
                body.push_back('.');
            }
            for (int i = 0; i < precision; ++i) {
                body.push_back(digitAt(digits, decimalPoint + i));
            }
            return body;
        }

        std::string exponentBody(const std::string &digits, int exponent, int precision,
                                 bool alternate, bool upper) {
            std::string body;
            body.push_back(digitAt(digits, 0));
            if (precision > 0 || alternate) {
                body.push_back('.');
            }
            for (int i = 1; i <= precision; ++i) {
                body.push_back(digitAt(digits, i));
            }
            body.push_back(upper ? 'E' : 'e');
            body.push_back(exponent < 0 ? '-' : '+');
            char buffer[16];
            std::snprintf(buffer, sizeof buffer, "%03d", std::abs(exponent));
            body += buffer;
            return body;
        }

        // Removes trailing zeros of the fraction and a decimal point left without digits, as
        // %g does without the # flag. Only the characters before an exponent are affected.
        std::string stripTrailingZeros(std::string body) {
            size_t exponent = body.find_first_of("eE");
            std::string tail = exponent == std::string::npos ? "" : body.substr(exponent);
            std::string head = exponent == std::string::npos ? body : body.substr(0, exponent);
            if (head.find('.') != std::string::npos) {
                while (!head.empty() && head.back() == '0') {
                    head.pop_back();
                }
                if (!head.empty() && head.back() == '.') {
                    head.pop_back();
                }
            }
            return head + tail;
        }

        std::string hexBody(std::uint64_t bits, int precision, bool alternate, bool upper) {
            unsigned exponent = unsigned(bits >> 52) & 0x7FF;
            std::uint64_t fraction = bits & ((std::uint64_t(1) << 52) - 1);

            // Measured: %a without a precision writes six hexadecimal digits.
            if (precision < 0) {
                precision = 6;
            }
            unsigned lead;
            int binaryExponent;
            if (exponent == 0 && fraction == 0) {
                lead = 0;
                binaryExponent = 0;
            } else if (exponent == 0) {
                lead = 0;
                binaryExponent = -1022;
            } else {
                lead = 1;
                binaryExponent = int(exponent) - 1023;
            }

            // Round the 13 hexadecimal digits of the fraction half up at the requested position.
            if (precision < 13) {
                int dropped = (13 - precision) * 4;
                std::uint64_t half = std::uint64_t(1) << (dropped - 1);
                std::uint64_t kept = fraction >> dropped;
                if (fraction & half) {
                    ++kept;
                }
                if (kept >> (precision * 4)) {
                    kept &= (std::uint64_t(1) << (precision * 4)) - 1;
                    ++lead;
                }
                fraction = kept << dropped;
            }

            const char *hexDigits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
            std::string body = upper ? "0X" : "0x";
            body.push_back(hexDigits[lead]);
            if (precision > 0 || alternate) {
                body.push_back('.');
            }
            for (int i = 0; i < precision; ++i) {
                body.push_back(i < 13 ? hexDigits[(fraction >> (48 - 4 * i)) & 0xF] : '0');
            }
            body.push_back(upper ? 'P' : 'p');
            char buffer[16];
            std::snprintf(buffer, sizeof buffer, "%+d", binaryExponent);
            body += buffer;
            return body;
        }

        // Formats a floating-point conversion into its sign prefix and body.
        void formatDouble(std::uint64_t bits, char conversion, const Specification &spec,
                          std::string &prefix, std::string &body) {
            Decimal d = decompose(bits);
            bool alternate = (spec.flags & FlagAlternate) != 0;
            bool upper = conversion == 'E' || conversion == 'G' || conversion == 'A';

            if (d.negative) {
                prefix = "-";
            } else if (spec.flags & FlagPlus) {
                prefix = "+";
            } else if (spec.flags & FlagSpace) {
                prefix = " ";
            }

            int precision = spec.precision < 0 ? 6 : spec.precision;
            switch (conversion) {
                case 'e':
                case 'E': {
                    int decimalPoint = d.decimalPoint;
                    std::string digits = roundDigits(d.mantissa, precision + 1, decimalPoint);
                    int exponent = d.zero ? 0 : decimalPoint - 1;
                    body = exponentBody(digits, exponent, precision, alternate, upper);
                    break;
                }
                case 'f': {
                    int decimalPoint = d.decimalPoint;
                    std::string digits =
                        roundDigits(d.mantissa, d.decimalPoint + precision, decimalPoint);
                    body = fixedBody(digits, decimalPoint, precision, alternate);
                    break;
                }
                case 'g':
                case 'G': {
                    if (precision == 0) {
                        precision = 1;
                    }
                    int decimalPoint = d.decimalPoint;
                    std::string digits = roundDigits(d.mantissa, precision, decimalPoint);
                    int magnitude = decimalPoint - 1;
                    if (magnitude < -4 || magnitude >= precision) {
                        int exponent = d.zero ? 0 : magnitude;
                        body = exponentBody(digits, exponent, precision - 1, alternate, upper);
                    } else {
                        body = fixedBody(digits, decimalPoint, precision - decimalPoint,
                                         alternate);
                    }
                    if (!alternate) {
                        body = stripTrailingZeros(body);
                    }
                    break;
                }
                default: {
                    // The hexadecimal form. The sign is part of the prefix like the others.
                    if (((bits >> 52) & 0x7FF) != 0x7FF) {
                        body = hexBody(bits, spec.precision, alternate, upper);
                    } else {
                        // Not measured. The digit strings of msvcrt are used as for %e.
                        int decimalPoint = d.decimalPoint;
                        std::string digits = roundDigits(d.mantissa, precision + 1, decimalPoint);
                        body = exponentBody(digits, 0, precision, alternate, upper);
                    }
                    break;
                }
            }
        }

        std::string integerDigits(std::uint64_t value, unsigned radix, bool upper) {
            const char *digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
            std::string out;
            while (value != 0) {
                out.push_back(digits[value % radix]);
                value /= radix;
            }
            return std::string(out.rbegin(), out.rend());
        }

        template <class Char>
        struct Output {
            std::basic_string<Char> &text;

            void put(Char c) {
                text.push_back(c);
            }

            void repeat(Char c, int count) {
                if (count > 0) {
                    text.append(size_t(count), c);
                }
            }

            void ascii(const std::string &s) {
                for (char c : s) {
                    text.push_back(static_cast<Char>(static_cast<unsigned char>(c)));
                }
            }
        };

        // Writes the padding, the prefix and the body in the order of msvcrt: spaces unless the
        // field is left-justified or zero-padded, then the prefix, then zeros if zero-padded.
        template <class Char, class WriteBody>
        void writeField(Output<Char> &out, int flags, int width, const std::string &prefix,
                        int bodyLength, WriteBody writeBody) {
            int padding = width - int(prefix.size()) - bodyLength;
            if (!(flags & (FlagLeft | FlagLeadZero))) {
                out.repeat(Char(' '), padding);
            }
            out.ascii(prefix);
            if ((flags & FlagLeadZero) && !(flags & FlagLeft)) {
                out.repeat(Char('0'), padding);
            }
            bool ok = writeBody();
            if (ok && (flags & FlagLeft)) {
                out.repeat(Char(' '), padding);
            }
        }

        template <class Char>
        size_t boundedLength(const Char *s, int precision) {
            size_t n = 0;
            while ((precision < 0 || n < size_t(precision)) && s[n] != 0) {
                ++n;
            }
            return n;
        }

        template <class Char>
        FormatResult<Char> formatAny(const Char *p, GuestArguments &args) {
            constexpr bool wideFormat = sizeof(Char) == 2;
            FormatResult<Char> result;
            Output<Char> out{result.text};

            while (*p) {
                if (*p != Char('%')) {
                    out.put(*p++);
                    continue;
                }
                ++p;

                Specification spec;
                for (;; ++p) {
                    if (*p == Char('-')) {
                        spec.flags |= FlagLeft;
                    } else if (*p == Char('+')) {
                        spec.flags |= FlagPlus;
                    } else if (*p == Char(' ')) {
                        spec.flags |= FlagSpace;
                    } else if (*p == Char('#')) {
                        spec.flags |= FlagAlternate;
                    } else if (*p == Char('0')) {
                        spec.flags |= FlagLeadZero;
                    } else {
                        break;
                    }
                }

                if (*p == Char('*')) {
                    spec.width = int(args.nextUInt32());
                    if (spec.width < 0) {
                        spec.flags |= FlagLeft;
                        spec.width = -spec.width;
                    }
                    ++p;
                } else {
                    while (*p >= Char('0') && *p <= Char('9')) {
                        spec.width = spec.width * 10 + int(*p++ - Char('0'));
                    }
                }

                if (*p == Char('.')) {
                    ++p;
                    spec.precision = 0;
                    if (*p == Char('*')) {
                        spec.precision = int(args.nextUInt32());
                        // A negative precision is taken as omitted.
                        if (spec.precision < 0) {
                            spec.precision = -1;
                        }
                        ++p;
                    } else {
                        while (*p >= Char('0') && *p <= Char('9')) {
                            spec.precision = spec.precision * 10 + int(*p++ - Char('0'));
                        }
                    }
                }

                for (;;) {
                    if (*p == Char('h')) {
                        spec.shortInteger = true;
                        spec.forceNarrow = true;
                        ++p;
                    } else if (*p == Char('l')) {
                        if (p[1] == Char('l')) {
                            spec.longLong = true;
                            p += 2;
                        } else {
                            spec.forceWide = true;
                            ++p;
                        }
                    } else if (*p == Char('w')) {
                        spec.forceWide = true;
                        ++p;
                    } else if (*p == Char('I')) {
                        if (p[1] == Char('6') && p[2] == Char('4')) {
                            spec.longLong = true;
                            p += 3;
                        } else if (p[1] == Char('3') && p[2] == Char('2')) {
                            p += 3;
                        } else {
                            ++p;
                        }
                    } else if (*p == Char('j')) {
                        spec.longLong = true;
                        ++p;
                    } else if (*p == Char('L') || *p == Char('z') || *p == Char('t')) {
                        // L denotes long double, which is double in msvcrt. z and t denote
                        // 32-bit sizes on i386.
                        ++p;
                    } else {
                        break;
                    }
                }

                Char conversion = *p;
                if (conversion == 0) {
                    // A format that ends inside a specification produces nothing more.
                    break;
                }
                ++p;

                switch (conversion) {
                    case Char('d'):
                    case Char('i'):
                    case Char('u'):
                    case Char('o'):
                    case Char('x'):
                    case Char('X'):
                    case Char('p'): {
                        bool isSigned = conversion == Char('d') || conversion == Char('i');
                        unsigned radix = conversion == Char('o') ? 8
                                         : (conversion == Char('u') || isSigned) ? 10
                                                                                 : 16;
                        bool upper = conversion == Char('X') || conversion == Char('p');
                        if (conversion == Char('p')) {
                            spec.precision = 8;
                            spec.longLong = false;
                            spec.shortInteger = false;
                        }

                        std::uint64_t magnitude;
                        bool negative = false;
                        if (spec.longLong) {
                            std::uint64_t raw = args.nextUInt64();
                            if (isSigned && std::int64_t(raw) < 0) {
                                negative = true;
                                magnitude = 0 - raw;
                            } else {
                                magnitude = raw;
                            }
                        } else {
                            std::uint32_t raw = args.nextUInt32();
                            if (spec.shortInteger) {
                                if (isSigned) {
                                    std::int32_t v = std::int16_t(raw & 0xFFFF);
                                    negative = v < 0;
                                    magnitude = negative ? std::uint64_t(-std::int64_t(v)) : v;
                                } else {
                                    magnitude = raw & 0xFFFF;
                                }
                            } else if (isSigned && std::int32_t(raw) < 0) {
                                negative = true;
                                magnitude = std::uint64_t(-std::int64_t(std::int32_t(raw)));
                            } else {
                                magnitude = raw;
                            }
                        }

                        std::string digits = integerDigits(magnitude, radix, upper);
                        if (spec.precision >= 0) {
                            // A precision disables zero padding of integers.
                            spec.flags &= ~FlagLeadZero;
                            if (int(digits.size()) < spec.precision) {
                                digits.insert(0, size_t(spec.precision) - digits.size(), '0');
                            }
                        } else if (digits.empty()) {
                            digits = "0";
                        }
                        if (radix == 8 && (spec.flags & FlagAlternate) &&
                            (digits.empty() || digits[0] != '0')) {
                            digits.insert(0, 1, '0');
                        }

                        std::string prefix;
                        if (isSigned) {
                            if (negative) {
                                prefix = "-";
                            } else if (spec.flags & FlagPlus) {
                                prefix = "+";
                            } else if (spec.flags & FlagSpace) {
                                prefix = " ";
                            }
                        } else if (radix == 16 && (spec.flags & FlagAlternate) && magnitude) {
                            // Measured: %#p yields 0X in capitals, like %#X.
                            prefix = upper ? "0X" : "0x";
                        }
                        writeField(out, spec.flags, spec.width, prefix, int(digits.size()),
                                   [&] {
                                       out.ascii(digits);
                                       return true;
                                   });
                        break;
                    }

                    case Char('e'):
                    case Char('E'):
                    case Char('f'):
                    case Char('g'):
                    case Char('G'):
                    case Char('a'):
                    case Char('A'): {
                        std::string prefix, body;
                        formatDouble(args.nextUInt64(), char(conversion), spec, prefix, body);
                        writeField(out, spec.flags, spec.width, prefix, int(body.size()), [&] {
                            out.ascii(body);
                            return true;
                        });
                        break;
                    }

                    case Char('c'):
                    case Char('C'): {
                        // The default width follows the format: c names the character type of
                        // the format and C the other type.
                        bool wideArgument = conversion == Char('c') ? wideFormat : !wideFormat;
                        if (spec.forceWide) {
                            wideArgument = true;
                        } else if (spec.forceNarrow) {
                            wideArgument = false;
                        }
                        std::uint32_t raw = args.nextUInt32();
                        Char c;
                        if (wideArgument) {
                            auto unit = char16_t(raw & 0xFFFF);
                            if (!wideFormat && unit > 0xFF) {
                                // Measured: a character that the C locale cannot represent
                                // produces no output and no failure.
                                break;
                            }
                            c = Char(unit);
                        } else {
                            c = Char(static_cast<unsigned char>(raw & 0xFF));
                        }
                        writeField(out, spec.flags, spec.width, std::string(), 1, [&] {
                            out.put(c);
                            return true;
                        });
                        break;
                    }

                    case Char('s'):
                    case Char('S'): {
                        bool wideArgument = conversion == Char('s') ? wideFormat : !wideFormat;
                        if (spec.forceWide) {
                            wideArgument = true;
                        } else if (spec.forceNarrow) {
                            wideArgument = false;
                        }
                        auto address = reinterpret_cast<const void *>(
                            static_cast<std::uintptr_t>(args.nextUInt32()));
                        if (wideArgument) {
                            auto s = address ? static_cast<const char16_t *>(address) : u"(null)";
                            size_t length = boundedLength(s, spec.precision);
                            writeField(out, spec.flags, spec.width, std::string(), int(length),
                                       [&] {
                                           for (size_t i = 0; i < length; ++i) {
                                               if (!wideFormat && s[i] > 0xFF) {
                                                   result.failed = true;
                                                   return false;
                                               }
                                               out.put(Char(s[i]));
                                           }
                                           return true;
                                       });
                        } else {
                            auto s = address ? static_cast<const char *>(address) : "(null)";
                            size_t length = boundedLength(s, spec.precision);
                            writeField(out, spec.flags, spec.width, std::string(), int(length),
                                       [&] {
                                           for (size_t i = 0; i < length; ++i) {
                                               out.put(
                                                   Char(static_cast<unsigned char>(s[i])));
                                           }
                                           return true;
                                       });
                        }
                        break;
                    }

                    case Char('n'):
                        // Measured: msvcrt rejects %n and returns -1.
                        result.failed = true;
                        break;

                    default:
                        // Measured: %% and an unknown conversion character produce the character
                        // itself, without padding and without consuming an argument.
                        out.put(conversion);
                        break;
                }

                if (result.failed) {
                    break;
                }
            }
            return result;
        }

    }

    FormatResult<char> formatNarrow(const char *format, GuestArguments &arguments) {
        return formatAny<char>(format, arguments);
    }

    FormatResult<char16_t> formatWide(const char16_t *format, GuestArguments &arguments) {
        return formatAny<char16_t>(format, arguments);
    }

    template <class Char>
    static int storeTruncatedAny(const FormatResult<Char> &result, Char *buffer, size_t count) {
        size_t length = result.text.size();
        size_t stored = length < count ? length : count;
        std::copy(result.text.begin(), result.text.begin() + stored, buffer);
        if (length < count) {
            buffer[length] = 0;
        }
        if (result.failed || length > count) {
            return -1;
        }
        return int(length);
    }

    int storeTruncated(const FormatResult<char> &result, char *buffer, size_t count) {
        return storeTruncatedAny(result, buffer, count);
    }

    int storeTruncated(const FormatResult<char16_t> &result, char16_t *buffer, size_t count) {
        return storeTruncatedAny(result, buffer, count);
    }

}
