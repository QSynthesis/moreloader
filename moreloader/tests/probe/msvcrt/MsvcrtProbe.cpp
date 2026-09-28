// Measures the behavior of the system msvcrt.dll and of the Windows functions that moreloader
// emulates, and writes the results as golden data for the automatic tests.
//
// Build and run with Build.ps1 in this directory. The program must be a 32-bit program, because
// moresampler loads the 32-bit msvcrt.dll from SysWOW64.
//
//     MsvcrtProbe.exe <output directory> <scratch directory>
//
// Every output file is UTF-8 text with one case per line and tab-separated fields. Strings are
// escaped by escapeBytes(): printable ASCII other than backslash, tab and comma is written as is,
// and every other byte as \xHH. Wide strings are converted to UTF-8 before escaping, except where
// the code units themselves are the subject, in which case they are written as hexadecimal lists.

#include <windows.h>
#include <shellapi.h>

#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

    // ----------------------------------------------------------------------------------------
    // msvcrt.dll entry points
    // ----------------------------------------------------------------------------------------

    struct MsvcrtFILE;

    struct Msvcrt {
        int(__cdecl *vsprintf)(char *, const char *, va_list);
        int(__cdecl *_vsnprintf)(char *, size_t, const char *, va_list);
        int(__cdecl *_vsnwprintf)(wchar_t *, size_t, const wchar_t *, va_list);
        void(__cdecl *qsort)(void *, size_t, size_t, int(__cdecl *)(const void *, const void *));
        int(__cdecl *rand)();
        void(__cdecl *srand)(unsigned);
        uintptr_t(__cdecl *_beginthreadex)(void *, unsigned, unsigned(__stdcall *)(void *), void *,
                                           unsigned, unsigned *);
        MsvcrtFILE *(__cdecl *fopen)(const char *, const char *);
        MsvcrtFILE *(__cdecl *_wfopen)(const wchar_t *, const wchar_t *);
        int(__cdecl *fputs)(const char *, MsvcrtFILE *);
        int(__cdecl *fflush)(MsvcrtFILE *);
        int(__cdecl *fseek)(MsvcrtFILE *, long, int);
        long(__cdecl *ftell)(MsvcrtFILE *);
        int(__cdecl *fgetc)(MsvcrtFILE *);
        char *(__cdecl *fgets)(char *, int, MsvcrtFILE *);
        size_t(__cdecl *fread)(void *, size_t, size_t, MsvcrtFILE *);
        size_t(__cdecl *fwrite)(const void *, size_t, size_t, MsvcrtFILE *);
        int(__cdecl *feof)(MsvcrtFILE *);
        int(__cdecl *fclose)(MsvcrtFILE *);
        int *(__cdecl *_errno)();
        double(__cdecl *atof)(const char *);
        double(__cdecl *tan)(double);
        double(__cdecl *tanh)(double);
        double(__cdecl *acos)(double);
        double(__cdecl *asin)(double);
        double(__cdecl *cosh)(double);
        double(__cdecl *sinh)(double);
        double(__cdecl *log10)(double);
        double(__cdecl *frexp)(double, int *);
        char *(__cdecl *strerror)(int);
        char *(__cdecl *asctime)(const struct tm *);
        int(__cdecl *isalnum)(int);
        int(__cdecl *isalpha)(int);
        int(__cdecl *iscntrl)(int);
        int(__cdecl *isgraph)(int);
        int(__cdecl *islower)(int);
        int(__cdecl *ispunct)(int);
        int(__cdecl *isspace)(int);
        int(__cdecl *isupper)(int);
        int(__cdecl *isxdigit)(int);
        int(__cdecl *tolower)(int);
        int(__cdecl *toupper)(int);
        long(__cdecl *strtol)(const char *, char **, int);
        unsigned long(__cdecl *strtoul)(const char *, char **, int);
        long(__cdecl *wcstol)(const wchar_t *, wchar_t **, int);
        char *(__cdecl *_ultoa)(unsigned long, char *, int);
        size_t(__cdecl *_mbslen)(const unsigned char *);
        int(__cdecl *_stricmp)(const char *, const char *);
        int(__cdecl *_strnicmp)(const char *, const char *, size_t);
    };

    Msvcrt crt;

    template <class T>
    void resolve(HMODULE module, T &function, const char *name) {
        function = reinterpret_cast<T>(GetProcAddress(module, name));
        if (!function) {
            std::fprintf(stderr, "msvcrt.dll does not export %s\n", name);
            std::exit(2);
        }
    }

    void loadMsvcrt() {
        HMODULE m = LoadLibraryA("msvcrt.dll");
        if (!m) {
            std::fprintf(stderr, "LoadLibrary(msvcrt.dll) failed\n");
            std::exit(2);
        }
#define R(name) resolve(m, crt.name, #name)
        R(vsprintf), R(_vsnprintf), R(_vsnwprintf), R(qsort), R(rand), R(srand);
        R(_beginthreadex), R(fopen), R(_wfopen), R(fputs), R(fflush), R(fseek), R(ftell);
        R(fgetc), R(fgets), R(fread), R(fwrite), R(feof), R(fclose), R(_errno), R(atof);
        R(tan), R(tanh), R(acos), R(asin), R(cosh), R(sinh), R(log10), R(frexp);
        R(strerror), R(asctime), R(isalnum), R(isalpha), R(iscntrl), R(isgraph), R(islower);
        R(ispunct), R(isspace), R(isupper), R(isxdigit), R(tolower), R(toupper), R(strtol);
        R(strtoul), R(wcstol), R(_ultoa), R(_mbslen), R(_stricmp), R(_strnicmp);
#undef R
    }

    // ----------------------------------------------------------------------------------------
    // Output helpers
    // ----------------------------------------------------------------------------------------

    std::string escapeBytes(const std::string &bytes) {
        std::string out;
        for (unsigned char c : bytes) {
            if (c >= 0x20 && c < 0x7F && c != '\\' && c != ',') {
                out.push_back(static_cast<char>(c));
            } else {
                char buf[8];
                std::snprintf(buf, sizeof buf, "\\x%02X", c);
                out += buf;
            }
        }
        return out;
    }

    std::string toUtf8(const std::wstring &text) {
        if (text.empty()) {
            return {};
        }
        int n = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                                    nullptr, 0, nullptr, nullptr);
        std::string out(n, '\0');
        WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), out.data(),
                            n, nullptr, nullptr);
        return out;
    }

    std::wstring fromUtf8(const std::string &text) {
        if (text.empty()) {
            return {};
        }
        int n = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                                    nullptr, 0);
        std::wstring out(n, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), out.data(),
                            n);
        return out;
    }

    std::string hexUnits(const wchar_t *units, size_t count) {
        std::string out;
        for (size_t i = 0; i < count; ++i) {
            char buf[8];
            std::snprintf(buf, sizeof buf, "%s%04X", i ? " " : "", unsigned(units[i]));
            out += buf;
        }
        return out;
    }

    std::string hex64(uint64_t value) {
        char buf[24];
        std::snprintf(buf, sizeof buf, "%016llX", static_cast<unsigned long long>(value));
        return buf;
    }

    uint64_t bitsOf(double value) {
        uint64_t bits;
        std::memcpy(&bits, &value, 8);
        return bits;
    }

    double fromBits(uint64_t bits) {
        double value;
        std::memcpy(&value, &bits, 8);
        return value;
    }

    struct Output {
        FILE *file = nullptr;

        Output(const std::string &dir, const char *name) {
            std::string path = dir + "\\" + name;
            file = std::fopen(path.c_str(), "wb");
            if (!file) {
                std::fprintf(stderr, "cannot write %s\n", path.c_str());
                std::exit(2);
            }
        }

        ~Output() {
            std::fclose(file);
        }

        void line(const std::vector<std::string> &fields) {
            for (size_t i = 0; i < fields.size(); ++i) {
                if (i) {
                    std::fputc('\t', file);
                }
                std::fputs(fields[i].c_str(), file);
            }
            std::fputc('\n', file);
        }
    };

    // Deterministic generator, so that a rerun of the probe produces the same cases.
    struct Random {
        uint64_t state = 0x9E3779B97F4A7C15ull;

        uint64_t next() {
            state ^= state << 13;
            state ^= state >> 7;
            state ^= state << 17;
            return state;
        }

        uint32_t below(uint32_t bound) {
            return static_cast<uint32_t>(next() % bound);
        }
    };

    // ----------------------------------------------------------------------------------------
    // printf
    // ----------------------------------------------------------------------------------------

    // One argument of a printf case. The kinds are those of the argument specification in
    // printf.txt: i int32, I int64, f double, s narrow string, S wide string, p pointer-sized
    // integer, which also passes a null string.
    struct Arg {
        char kind;
        int64_t integer = 0;
        uint64_t bits = 0;
        std::string text;
    };

    Arg argInt(int32_t v) {
        Arg a{'i'};
        a.integer = v;
        return a;
    }

    Arg argInt64(int64_t v) {
        Arg a{'I'};
        a.integer = v;
        return a;
    }

    Arg argDouble(double v) {
        Arg a{'f'};
        a.bits = bitsOf(v);
        return a;
    }

    Arg argBits(uint64_t bits) {
        Arg a{'f'};
        a.bits = bits;
        return a;
    }

    Arg argString(const std::string &v) {
        Arg a{'s'};
        a.text = v;
        return a;
    }

    Arg argWide(const std::string &utf8) {
        Arg a{'S'};
        a.text = utf8;
        return a;
    }

    Arg argPointer(uint32_t v) {
        Arg a{'p'};
        a.integer = v;
        return a;
    }

    std::string argSpec(const std::vector<Arg> &args) {
        std::string out;
        for (size_t i = 0; i < args.size(); ++i) {
            const Arg &a = args[i];
            if (i) {
                out += ",";
            }
            out.push_back(a.kind);
            out += ":";
            switch (a.kind) {
                case 'i':
                case 'I':
                case 'p':
                    out += std::to_string(a.integer);
                    break;
                case 'f':
                    out += hex64(a.bits);
                    break;
                default:
                    out += escapeBytes(a.text);
                    break;
            }
        }
        return out;
    }

    // Packs the arguments as the i386 calling convention lays them out on the stack. A va_list
    // of MSVC for x86 is a pointer to that area.
    struct PackedArgs {
        std::vector<uint32_t> words;
        std::vector<std::string> narrow;
        std::vector<std::wstring> wide;

        explicit PackedArgs(const std::vector<Arg> &args) {
            narrow.reserve(args.size());
            wide.reserve(args.size());
            for (const Arg &a : args) {
                switch (a.kind) {
                    case 'i':
                    case 'p':
                        words.push_back(static_cast<uint32_t>(a.integer));
                        break;
                    case 'I':
                        words.push_back(static_cast<uint32_t>(a.integer));
                        words.push_back(static_cast<uint32_t>(uint64_t(a.integer) >> 32));
                        break;
                    case 'f':
                        words.push_back(static_cast<uint32_t>(a.bits));
                        words.push_back(static_cast<uint32_t>(a.bits >> 32));
                        break;
                    case 's':
                        narrow.push_back(a.text);
                        words.push_back(reinterpret_cast<uint32_t>(narrow.back().c_str()));
                        break;
                    case 'S':
                        wide.push_back(fromUtf8(a.text));
                        words.push_back(reinterpret_cast<uint32_t>(wide.back().c_str()));
                        break;
                }
            }
            // Padding against formats that read one argument more than supplied.
            for (int i = 0; i < 8; ++i) {
                words.push_back(0);
            }
        }

        va_list list() {
            return reinterpret_cast<va_list>(words.data());
        }
    };

    struct PrintfCase {
        std::string format;
        std::vector<Arg> args;
        // Whether the case is also run through the wide printf. The wide and narrow families
        // share the conversion of numbers, therefore generated numeric cases are run narrow only.
        bool wide = true;
    };

    void addDoubleCases(std::vector<PrintfCase> &cases) {
        const char *formats[] = {
            "%f",    "%.0f",   "%.1f",  "%.2f",    "%.3f",     "%.10f", "%.17f", "%.20f",
            "%e",    "%.0e",   "%.3e",  "%.15e",   "%.17e",    "%E",    "%g",    "%.0g",
            "%.1g",  "%.3g",   "%.10g", "%.14g",   "%.15g",    "%.17g", "%#g",   "%#.0f",
            "%#.0e", "%+10.4f", "%-12.3e|", "%012.3f", "% g", "%G", "%lf", "%Lf", "%10.3g|",
            "%-+08.2f|",
        };

        std::vector<uint64_t> values = {
            bitsOf(0.0),
            bitsOf(-0.0),
            bitsOf(1.0),
            bitsOf(-1.0),
            bitsOf(0.5),
            bitsOf(1.5),
            bitsOf(2.5),
            bitsOf(0.125),
            bitsOf(0.375),
            bitsOf(2.675),
            bitsOf(1.005),
            bitsOf(9.995),
            bitsOf(0.0005),
            bitsOf(0.00049999999999999999),
            bitsOf(999999.5),
            bitsOf(9.9999999e-5),
            bitsOf(1e15 + 0.5),
            bitsOf(123456789012345678.0),
            bitsOf(1e21),
            bitsOf(1e100),
            bitsOf(1e300),
            bitsOf(1.7976931348623157e308),
            bitsOf(2.2250738585072014e-308),
            0x0000000000000001ull,          // smallest denormal
            0x7FF0000000000000ull,          // +infinity
            0xFFF0000000000000ull,          // -infinity
            0x7FF8000000000000ull,          // quiet NaN
            0xFFF8000000000000ull,          // indefinite
            0x7FF0000000000001ull,          // signaling NaN
            0x7FF4000000000000ull,          // signaling NaN
            0xFFFFFFFFFFFFFFFFull,          // negative quiet NaN with a payload
            bitsOf(3.14159265358979323846),
            bitsOf(44100.0),
            bitsOf(0.1),
            bitsOf(0.3),
            bitsOf(100.0),
        };

        const size_t handPicked = values.size();

        Random random;
        for (int i = 0; i < 100; ++i) {
            // Finite values spread over the exponent range, with an emphasis on the magnitudes
            // of audio and pitch parameters.
            uint64_t mantissa = random.next() & 0x000FFFFFFFFFFFFFull;
            uint64_t exponent;
            if (i < 60) {
                exponent = 1023 - 30 + random.below(60);
            } else {
                exponent = 1 + random.below(2046);
            }
            uint64_t sign = uint64_t(random.below(2)) << 63;
            values.push_back(sign | (exponent << 52) | mantissa);
        }
        for (int i = 0; i < 40; ++i) {
            // Decimal values with few digits, which produce exact halfway cases after rounding.
            int digits = static_cast<int>(random.below(100000));
            int scale = static_cast<int>(random.below(7));
            values.push_back(bitsOf(digits / std::pow(10.0, scale)));
        }

        for (size_t i = 0; i < values.size(); ++i) {
            for (const char *format : formats) {
                cases.push_back({format, {argBits(values[i])}, i < handPicked});
            }
        }

        // Values whose exact decimal expansion has 18 significant digits ending in 5, which lie
        // exactly halfway between two 17-digit representations. The rounding of msvcrt at the
        // 17th digit is decided by these. Each is an integer of 13 to 15 digits plus a multiple
        // of 1/8 or 1/16, both of which are exact in a double of that magnitude.
        for (int i = 0; i < 120; ++i) {
            int integerDigits = 13 + static_cast<int>(random.below(3));
            double integer = std::floor(std::pow(10.0, integerDigits - 1) *
                                        (1.0 + 8.0 * ((random.next() >> 11) * 0x1.0p-53)));
            double fraction;
            switch (integerDigits) {
                case 15:
                    fraction = (1 + 2 * random.below(4)) / 8.0; // 3 fractional digits
                    break;
                case 14:
                    fraction = (1 + 2 * random.below(8)) / 16.0; // 4 fractional digits
                    break;
                default:
                    fraction = (1 + 2 * random.below(16)) / 32.0; // 5 fractional digits
                    break;
            }
            double value = (random.below(2) ? -1 : 1) * (integer + fraction);
            for (const char *format : {"%.16e", "%.17e", "%f"}) {
                cases.push_back({format, {argDouble(value)}, false});
            }
        }
    }

    void addIntegerAndStringCases(std::vector<PrintfCase> &cases) {
        auto add = [&](const char *format, std::vector<Arg> args) {
            cases.push_back({format, std::move(args)});
        };

        for (int32_t v : {0, 1, -1, 42, -42, 2147483647, -2147483647 - 1, 65535, 65536}) {
            for (const char *f : {"%d", "%i", "%u", "%x", "%X", "%o", "%5d|", "%-5d|", "%05d",
                                  "%+d", "% d", "%.3d", "%.0d|", "%#x", "%#X", "%#o", "%hd", "%hu",
                                  "%ld", "%lu", "%I32d", "%hhd", "%c", "%08.3d", "%-+6d|",
                                  "%zd", "%td", "%Id", "%5.0d|"}) {
                add(f, {argInt(v)});
            }
        }
        for (int64_t v : {int64_t(0), int64_t(-1), int64_t(1) << 40, -(int64_t(1) << 62),
                          int64_t(0x7FFFFFFFFFFFFFFFll)}) {
            for (const char *f : {"%lld", "%llu", "%llx", "%I64d", "%I64u", "%I64X", "%jd",
                                  "%20I64d|", "%-20lld|"}) {
                add(f, {argInt64(v)});
            }
        }

        for (const char *f : {"%s", "%.2s", "%10s|", "%-10s|", "%.0s|", "%hs", "%5.1s|"}) {
            add(f, {argString("abc")});
            add(f, {argString("")});
            add(f, {argString("\xE4\xBD\xA0\xE5\xA5\xBD")});
        }
        for (const char *f : {"%S", "%ls", "%ws", "%.1S", "%8S|"}) {
            add(f, {argWide("abc")});
            add(f, {argWide("\xC3\xA9t\xC3\xA9")});
            add(f, {argWide("\xE4\xBD\xA0")});
        }
        for (const char *f : {"%s", "%S", "%10s|", "%.3s|"}) {
            add(f, {argPointer(0)});
        }
        for (const char *f : {"%p", "%20p|", "%-12p|", "%#p", "%x"}) {
            add(f, {argPointer(0x0012FF3C)});
            add(f, {argPointer(0)});
        }
        for (const char *f : {"%C", "%lc", "%wc", "%hc", "%3c|"}) {
            add(f, {argInt(0x41)});
            add(f, {argInt(0xE9)});
            add(f, {argInt(0x4F60)});
        }

        add("%%", {});
        add("100%%|%5%|", {});
        add("%y|%k|", {argInt(1)});
        add("%", {});
        add("abc%", {});
        add("%*d|", {argInt(6), argInt(42)});
        add("%-*d|", {argInt(6), argInt(42)});
        add("%*d|", {argInt(-6), argInt(42)});
        add("%.*f|", {argInt(3), argDouble(3.14159)});
        add("%.*f|", {argInt(-1), argDouble(3.14159)});
        add("%*.*s|", {argInt(8), argInt(2), argString("hello")});
        add("%s=%d, %s=%.3f\n", {argString("rate"), argInt(44100), argString("gain"),
                                 argDouble(0.5)});
        add("%5.1f%%", {argDouble(99.95)});
        add("%I64d %d", {argInt64(1), argInt(2)});
        add("%lld %d", {argInt64(1), argInt(2)});
        add("%Lf %d", {argDouble(1.5), argInt(2)});
        add("%Le|%LG", {argDouble(1.5), argDouble(2.5)});
        add("%hf|%lf|%llf", {argDouble(1.5), argDouble(2.5), argDouble(3.5)});
        add("%a|%A|%.2a", {argDouble(1.0), argDouble(-0.1), argDouble(3.0)});
        add("%n", {argPointer(0)});
    }

    void probePrintf(const std::string &outDir) {
        std::vector<PrintfCase> cases;
        addIntegerAndStringCases(cases);
        addDoubleCases(cases);

        Output out(outDir, "printf.txt");
        for (auto &c : cases) {
            // Narrow printf with the case as written.
            {
                PackedArgs packed(c.args);
                char buffer[4096];
                std::memset(buffer, 0, sizeof buffer);
                int ret = crt.vsprintf(buffer, c.format.c_str(), packed.list());
                out.line({"N", escapeBytes(c.format), argSpec(c.args), escapeBytes(buffer),
                          std::to_string(ret)});
            }
            // Wide printf. In a wide format %s names a wide string, therefore the string kinds
            // of the arguments are swapped to keep each case meaningful.
            if (c.wide) {
                std::vector<Arg> wideArgs = c.args;
                for (Arg &a : wideArgs) {
                    if (a.kind == 's') {
                        a.kind = 'S';
                    } else if (a.kind == 'S') {
                        a.kind = 's';
                    }
                }
                PackedArgs packed(wideArgs);
                wchar_t buffer[4096];
                std::wmemset(buffer, 0, 4096);
                std::wstring format = fromUtf8(c.format);
                int ret = crt._vsnwprintf(buffer, 4095, format.c_str(), packed.list());
                std::string result = ret >= 0 ? hexUnits(buffer, size_t(ret)) : "";
                out.line({"W", escapeBytes(c.format), argSpec(wideArgs), result,
                          std::to_string(ret)});
            }
        }

        // Truncation of _vsnprintf and _vsnwprintf. The buffer is filled with a sentinel, and
        // count + 2 elements are recorded, so that a missing terminator is visible.
        Output trunc(outDir, "snprintf.txt");
        struct TruncCase {
            const char *format;
            std::vector<Arg> args;
        };
        std::vector<TruncCase> truncCases = {
            {"abcdef", {}},
            {"%d", {argInt(12345)}},
            {"%s", {argString("hello")}},
            {"%.3f", {argDouble(2.5)}},
        };
        for (auto &c : truncCases) {
            for (int count : {0, 1, 3, 5, 6, 7, 10}) {
                PackedArgs packed(c.args);
                char buffer[32];
                std::memset(buffer, '#', sizeof buffer);
                int ret = crt._vsnprintf(buffer, count, c.format, packed.list());
                trunc.line({"N", escapeBytes(c.format), argSpec(c.args), std::to_string(count),
                             escapeBytes(std::string(buffer, count + 2)), std::to_string(ret)});

                std::vector<Arg> wideArgs = c.args;
                for (Arg &a : wideArgs) {
                    if (a.kind == 's') {
                        a.kind = 'S';
                    }
                }
                PackedArgs widePacked(wideArgs);
                wchar_t wbuffer[32];
                std::wmemset(wbuffer, L'#', 32);
                std::wstring format = fromUtf8(c.format);
                ret = crt._vsnwprintf(wbuffer, count, format.c_str(), widePacked.list());
                trunc.line({"W", escapeBytes(c.format), argSpec(wideArgs), std::to_string(count),
                            hexUnits(wbuffer, size_t(count) + 2), std::to_string(ret)});
            }
        }
    }

    // ----------------------------------------------------------------------------------------
    // qsort
    // ----------------------------------------------------------------------------------------

    struct Element {
        int key;
        int id;
    };

    std::vector<std::pair<int, int>> s_comparisons;

    int __cdecl compareElements(const void *a, const void *b) {
        auto x = static_cast<const Element *>(a);
        auto y = static_cast<const Element *>(b);
        s_comparisons.emplace_back(x->id, y->id);
        return x->key < y->key ? -1 : (x->key > y->key ? 1 : 0);
    }

    void probeQsort(const std::string &outDir) {
        Output out(outDir, "qsort.txt");
        Random random;

        auto run = [&](const std::string &name, const std::vector<int> &keys) {
            std::vector<Element> elements;
            for (size_t i = 0; i < keys.size(); ++i) {
                elements.push_back({keys[i], static_cast<int>(i)});
            }
            s_comparisons.clear();
            crt.qsort(elements.data(), elements.size(), sizeof(Element), compareElements);

            std::string keyText, orderText, compareText;
            for (size_t i = 0; i < keys.size(); ++i) {
                keyText += (i ? "," : "") + std::to_string(keys[i]);
                orderText += (i ? "," : "") + std::to_string(elements[i].id);
            }
            for (size_t i = 0; i < s_comparisons.size(); ++i) {
                compareText += (i ? "," : "") + std::to_string(s_comparisons[i].first) + ":" +
                               std::to_string(s_comparisons[i].second);
            }
            out.line({name, keyText, orderText, compareText});
        };

        for (int n : {0, 1, 2, 3, 4, 5, 7, 8, 9, 10, 16, 17, 31, 50, 100, 257}) {
            std::vector<int> ascending, descending, equal, fewKeys, manyKeys;
            for (int i = 0; i < n; ++i) {
                ascending.push_back(i);
                descending.push_back(n - i);
                equal.push_back(7);
                fewKeys.push_back(static_cast<int>(random.below(4)));
                manyKeys.push_back(static_cast<int>(random.below(1000000)));
            }
            std::string suffix = "-" + std::to_string(n);
            run("ascending" + suffix, ascending);
            run("descending" + suffix, descending);
            run("equal" + suffix, equal);
            run("fewkeys" + suffix, fewKeys);
            run("manykeys" + suffix, manyKeys);
        }
    }

    // ----------------------------------------------------------------------------------------
    // rand
    // ----------------------------------------------------------------------------------------

    std::string s_threadRand;

    unsigned __stdcall randThread(void *) {
        for (int i = 0; i < 8; ++i) {
            s_threadRand += (i ? "," : "") + std::to_string(crt.rand());
        }
        return 0;
    }

    void probeRand(const std::string &outDir) {
        Output out(outDir, "rand.txt");

        // A new thread starts with its own state, which is expected to be seed 1.
        uintptr_t thread = crt._beginthreadex(nullptr, 0, randThread, nullptr, 0, nullptr);
        WaitForSingleObject(reinterpret_cast<HANDLE>(thread), INFINITE);
        CloseHandle(reinterpret_cast<HANDLE>(thread));
        out.line({"newthread", s_threadRand});

        for (unsigned seed : {0u, 1u, 12345u, 0xFFFFFFFFu}) {
            crt.srand(seed);
            std::string values;
            for (int i = 0; i < 16; ++i) {
                values += (i ? "," : "") + std::to_string(crt.rand());
            }
            out.line({"seed" + std::to_string(seed), values});
        }
    }

    // ----------------------------------------------------------------------------------------
    // File modes
    // ----------------------------------------------------------------------------------------

    std::string readRaw(const std::wstring &path) {
        std::string bytes;
        HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                               OPEN_EXISTING, 0, nullptr);
        if (h == INVALID_HANDLE_VALUE) {
            return "<missing>";
        }
        char buffer[256];
        DWORD n;
        while (ReadFile(h, buffer, sizeof buffer, &n, nullptr) && n) {
            bytes.append(buffer, n);
        }
        CloseHandle(h);
        return escapeBytes(bytes);
    }

    void writeRaw(const std::wstring &path, const std::string &bytes) {
        HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
        DWORD n;
        WriteFile(h, bytes.data(), DWORD(bytes.size()), &n, nullptr);
        CloseHandle(h);
    }

    void probeFopen(const std::string &outDir, const std::string &scratch) {
        Output out(outDir, "fopen.txt");
        const std::string initial = "a\r\nb\rc\r\r\nd\x1A" "e";
        const std::wstring path = fromUtf8(scratch) + L"\\fopen-probe.bin";

        const char *modes[] = {"r",  "rb", "rt", "w",   "wb", "wt",  "w+b", "a",  "ab", "r+",
                               "r+b", "rb+", "rw", "wr", "rwb", "wrb", "rx", "wx", "w+", ""};
        for (const char *mode : modes) {
            for (bool exists : {false, true}) {
                for (bool wide : {false, true}) {
                    DeleteFileW(path.c_str());
                    if (exists) {
                        writeRaw(path, initial);
                    }
                    *crt._errno() = 0;
                    MsvcrtFILE *f;
                    if (wide) {
                        f = crt._wfopen(path.c_str(), fromUtf8(mode).c_str());
                    } else {
                        f = crt.fopen(toUtf8(path).c_str(), mode);
                    }
                    std::vector<std::string> fields = {wide ? "_wfopen" : "fopen",
                                                       escapeBytes(mode), exists ? "1" : "0"};
                    if (!f) {
                        fields.push_back("null");
                        fields.push_back(std::to_string(*crt._errno()));
                        out.line(fields);
                        continue;
                    }
                    fields.push_back("open");
                    fields.push_back(std::to_string(*crt._errno()));

                    // Reading first, from the beginning.
                    std::string read;
                    int c;
                    while ((c = crt.fgetc(f)) != EOF) {
                        read.push_back(static_cast<char>(c));
                    }
                    fields.push_back(escapeBytes(read));
                    fields.push_back(std::to_string(crt.ftell(f)));

                    // Then writing at the end.
                    crt.fseek(f, 0, SEEK_END);
                    int putResult = crt.fputs("x\ny", f);
                    int flushResult = crt.fflush(f);
                    fields.push_back(std::to_string(putResult));
                    fields.push_back(std::to_string(flushResult));
                    crt.fclose(f);
                    fields.push_back(readRaw(path));
                    out.line(fields);
                }
            }
        }

        // Text-mode reading through fgets and fread, and the positions reported by ftell.
        writeRaw(path, initial);
        MsvcrtFILE *f = crt.fopen(toUtf8(path).c_str(), "r");
        std::string lines;
        char buffer[64];
        while (crt.fgets(buffer, sizeof buffer, f)) {
            lines += escapeBytes(buffer) + "|" + std::to_string(crt.ftell(f)) + ";";
        }
        out.line({"fgets", "r", lines, std::to_string(crt.feof(f))});
        crt.fclose(f);

        f = crt.fopen(toUtf8(path).c_str(), "r");
        std::memset(buffer, 0, sizeof buffer);
        size_t n = crt.fread(buffer, 1, 5, f);
        std::string first(buffer, n);
        long position = crt.ftell(f);
        n = crt.fread(buffer, 1, sizeof buffer, f);
        out.line({"fread", "r", escapeBytes(first), std::to_string(position),
                  escapeBytes(std::string(buffer, n)), std::to_string(crt.feof(f))});
        crt.fclose(f);

        // Text-mode writing through fwrite.
        f = crt.fopen(toUtf8(path).c_str(), "w");
        const char data[] = "l1\nl2\r\nl3\n\n";
        size_t written = crt.fwrite(data, 1, sizeof data - 1, f);
        crt.fclose(f);
        out.line({"fwrite", "w", std::to_string(written), readRaw(path)});

        DeleteFileW(path.c_str());
    }

    // ----------------------------------------------------------------------------------------
    // Numbers, strings and classification
    // ----------------------------------------------------------------------------------------

    void probeAtof(const std::string &outDir) {
        Output out(outDir, "atof.txt");
        std::vector<std::string> inputs = {
            "0", "100", "-50", "0.5", "70.0", "800.0", "44100", "1e3", "1E-3", " 12.5",
            "\t-3.25abc", "+.5", "5.", ".", "", "abc", "1e", "1e+", "0x10", "0x1p3", "inf",
            "-INF", "nan", "infinity", "1e400", "1e-400", "4.9e-324", "2.2250738585072011e-308",
            "1.7976931348623157e308", "1.7976931348623159e308", "0.1", "0.3",
            "123456789012345678901234567890", "3.14159265358979323846264338327950288",
            "9007199254740993", "1.00000000000000011102230246251565404236316680908203125",
            "1.000000000000000111022302462515654042363166809082031250000001",
            "2.4703282292062327e-324", "2.4703282292062328e-324", "1,5", "12e3.5",
        };
        Random random;
        for (int i = 0; i < 300; ++i) {
            char buffer[64];
            unsigned long long mantissa = random.next() % 100000000000000000ull;
            int exponent = static_cast<int>(random.below(80)) - 40;
            std::snprintf(buffer, sizeof buffer, "%llue%d", mantissa, exponent);
            inputs.push_back(buffer);
            int intPart = static_cast<int>(random.below(100000));
            unsigned frac = random.below(1000000);
            std::snprintf(buffer, sizeof buffer, "%d.%06u", intPart, frac);
            inputs.push_back(buffer);
        }
        for (auto &s : inputs) {
            out.line({escapeBytes(s), hex64(bitsOf(crt.atof(s.c_str())))});
        }

        Output strtolOut(outDir, "strtol.txt");
        std::vector<std::string> ints = {"0", "42", "-42", "  +17x", "0x1F", "0X1f", "017",
                                         "2147483647", "2147483648", "-2147483649",
                                         "4294967295", "4294967296", "", "abc", "-", "0x",
                                         "99999999999999999999"};
        for (auto &s : ints) {
            for (int base : {0, 10, 16, 8}) {
                char *end = nullptr;
                *crt._errno() = 0;
                long v = crt.strtol(s.c_str(), &end, base);
                int e1 = *crt._errno();
                *crt._errno() = 0;
                char *uend = nullptr;
                unsigned long u = crt.strtoul(s.c_str(), &uend, base);
                int e2 = *crt._errno();
                std::wstring ws = fromUtf8(s);
                wchar_t *wend = nullptr;
                long w = crt.wcstol(ws.c_str(), &wend, base);
                strtolOut.line({escapeBytes(s), std::to_string(base), std::to_string(v),
                                std::to_string(end - s.c_str()), std::to_string(e1),
                                std::to_string(u), std::to_string(uend - s.c_str()),
                                std::to_string(e2), std::to_string(w),
                                std::to_string(wend - ws.c_str())});
            }
        }
    }

    // Calls \a f with \a x and stores the 80-bit register ST0 that it returns.
    void returnRegister(double(__cdecl *f)(double), double x, unsigned char *out) {
        __asm {
            sub esp, 8
            fld x
            fstp qword ptr [esp]
            call f
            add esp, 8
            mov eax, out
            fstp tbyte ptr [eax]
        }
    }

    // Formats an 80-bit extended value as sign and exponent, then the 64-bit significand.
    std::string hexExtended(const unsigned char *bytes) {
        char buffer[32];
        std::snprintf(buffer, sizeof buffer, "%02X%02X:%02X%02X%02X%02X%02X%02X%02X%02X",
                      bytes[9], bytes[8], bytes[7], bytes[6], bytes[5], bytes[4], bytes[3],
                      bytes[2], bytes[1], bytes[0]);
        return buffer;
    }

    void probeMath(const std::string &outDir) {
        Output out(outDir, "math.txt");
        struct Function {
            const char *name;
            double(__cdecl *f)(double);
        };
        Function functions[] = {{"tan", crt.tan},   {"tanh", crt.tanh}, {"acos", crt.acos},
                                {"asin", crt.asin}, {"cosh", crt.cosh}, {"sinh", crt.sinh},
                                {"log10", crt.log10}};

        std::vector<double> inputs = {0.0, -0.0, 0.5, -0.5, 1.0, -1.0, 2.0, 0.1, 1e-10, 1e10,
                                      3.14159265358979323846 / 4, 3.14159265358979323846 / 2,
                                      710.0, -710.0, 1e300, 44100.0, 1.5e-5};
        inputs.push_back(fromBits(0x7FF0000000000000ull));
        inputs.push_back(fromBits(0x7FF8000000000000ull));
        // The arguments with which moresampler calls sinh at 0x433836: an integer times 1/6,
        // the double at 0x4b9488, computed on the x87 stack and stored as a double.
        for (int k = 0; k <= 400; ++k) {
            volatile double sixth = 1.0 / 6.0;
            inputs.push_back(k * sixth);
        }
        Random random;
        for (int i = 0; i < 400; ++i) {
            double unit = (random.next() >> 11) * (1.0 / 9007199254740992.0);
            if (i < 200) {
                inputs.push_back(unit * 2 - 1);
            } else {
                inputs.push_back((unit * 2 - 1) * 20);
            }
        }

        unsigned int control;
        _controlfp_s(&control, 0, 0);
        char cw[16];
        std::snprintf(cw, sizeof cw, "%08X", control);
        out.line({"controlfp", cw});

        for (auto &fn : functions) {
            for (double x : inputs) {
                double y = fn.f(x);
                out.line({fn.name, hex64(bitsOf(x)), hex64(bitsOf(y))});
            }
        }
        for (double x : inputs) {
            int exponent = 0;
            double m = crt.frexp(x, &exponent);
            out.line({"frexp", hex64(bitsOf(x)), hex64(bitsOf(m)), std::to_string(exponent)});
        }

        // The register ST0 as each function returns it, before a store rounds it to a double.
        // The guest continues computing on the x87 stack with the returned register.
        Output st0(outDir, "math-st0.txt");
        for (auto &fn : functions) {
            for (size_t i = 0; i < inputs.size(); ++i) {
                if (fn.f != crt.sinh && i >= 100) {
                    continue;
                }
                unsigned char extended[10];
                returnRegister(fn.f, inputs[i], extended);
                st0.line({fn.name, hex64(bitsOf(inputs[i])), hexExtended(extended)});
            }
        }
    }

    void probeStrings(const std::string &outDir) {
        Output out(outDir, "strings.txt");
        for (int e = -1; e <= 45; ++e) {
            out.line({"strerror", std::to_string(e), escapeBytes(crt.strerror(e))});
        }

        struct tm samples[] = {
            {55, 3, 2, 2, 0, 80, 3, 1, 0},
            {0, 0, 0, 31, 11, 126, 4, 364, 0},
            {9, 8, 17, 28, 8, 126, 1, 270, 0},
        };
        for (auto &t : samples) {
            out.line({"asctime", escapeBytes(crt.asctime(&t))});
        }

        char buffer[40];
        for (unsigned long v : {0ul, 1ul, 255ul, 4294967295ul}) {
            for (int radix : {2, 8, 10, 16, 36}) {
                crt._ultoa(v, buffer, radix);
                out.line({"_ultoa", std::to_string(v), std::to_string(radix), buffer});
            }
        }

        const char *mbs[] = {"", "abc", "\xE4\xBD\xA0\xE5\xA5\xBD", "\xC4\xE3\xBA\xC3", "a\x81"};
        for (const char *s : mbs) {
            out.line({"_mbslen", escapeBytes(s),
                      std::to_string(crt._mbslen(reinterpret_cast<const unsigned char *>(s)))});
        }

        const char *pairs[][2] = {{"abc", "ABC"}, {"abc", "abd"}, {"a", "ab"},
                                  {"[", "a"},     {"_", "A"},     {"\xE9", "\xC9"}};
        for (auto &p : pairs) {
            out.line({"_stricmp", escapeBytes(p[0]), escapeBytes(p[1]),
                      std::to_string(crt._stricmp(p[0], p[1])),
                      std::to_string(crt._strnicmp(p[0], p[1], 1))});
        }
    }

    void probeCtype(const std::string &outDir) {
        Output out(outDir, "ctype.txt");
        for (int c = -128; c <= 255; ++c) {
            std::string flags;
            flags += crt.isalnum(c) ? 'n' : '-';
            flags += crt.isalpha(c) ? 'a' : '-';
            flags += crt.iscntrl(c) ? 'c' : '-';
            flags += crt.isgraph(c) ? 'g' : '-';
            flags += crt.islower(c) ? 'l' : '-';
            flags += crt.ispunct(c) ? 'p' : '-';
            flags += crt.isspace(c) ? 's' : '-';
            flags += crt.isupper(c) ? 'u' : '-';
            flags += crt.isxdigit(c) ? 'x' : '-';
            out.line({std::to_string(c), flags, std::to_string(crt.tolower(c)),
                      std::to_string(crt.toupper(c))});
        }
    }

    // ----------------------------------------------------------------------------------------
    // Windows functions
    // ----------------------------------------------------------------------------------------

    void probeCommandLine(const std::string &outDir) {
        Output out(outDir, "cmdline.txt");
        const wchar_t *lines[] = {
            L"prog a b c",
            L"\"C:\\Program Files\\x.exe\" arg",
            L"prog \"a b\" c",
            L"prog a\\\\b c\\d",
            L"prog \"a\\\"b\"",
            L"prog a\\\\\\\"b",
            L"prog a\\\\\\\\\"b c\" d",
            L"prog \"\"",
            L"prog \"\"\"\"",
            L"prog \"\"\"\"\"\"",
            L"prog \"a\"\"b\"",
            L"prog a\"b\"c",
            L"prog   spaced\t\ttabs  ",
            L"\"prog with \\\"quote\" x",
            L"prog\\sub\\x.exe \"trail\\\\\"",
            L"prog \"unterminated",
            L"prog \u4F60\u597D \"\u00E9 \u00E9\"",
            L"prog in.wav out.wav C4 100 \"\" 0 500 0 0 100 0 !120 AA#5#",
            L" leading",
            L"",
        };
        for (const wchar_t *line : lines) {
            int argc = 0;
            LPWSTR *argv = CommandLineToArgvW(line, &argc);
            std::vector<std::string> fields = {escapeBytes(toUtf8(line)), std::to_string(argc)};
            for (int i = 0; argv && i < argc; ++i) {
                fields.push_back(escapeBytes(toUtf8(argv[i])));
            }
            LocalFree(argv);
            out.line(fields);
        }
    }

    void probeUtf8(const std::string &outDir) {
        Output out(outDir, "utf8.txt");
        const char *inputs[] = {
            "abc", "\xC3\xA9", "\xE4\xBD\xA0", "\xF0\x9F\x8E\xB5", "\x80", "\xC0\xAF",
            "\xE4\xBD" "a", "\xED\xA0\x80", "\xF4\x90\x80\x80", "\xFF\xFE", "a\xE4",
            "\xF0\x9F\x8E", "\xE0\x80\x80", "\xC2", "\xF5\x80\x80\x80",
        };
        for (const char *s : inputs) {
            int len = static_cast<int>(std::strlen(s));
            wchar_t buffer[32];
            int n = MultiByteToWideChar(CP_UTF8, 0, s, len, buffer, 32);
            int strict = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s, len, nullptr, 0);
            DWORD error = strict ? 0 : GetLastError();
            out.line({"mbtowc", escapeBytes(s), std::to_string(n), hexUnits(buffer, n),
                      std::to_string(strict), std::to_string(error)});
        }

        const wchar_t *wides[] = {L"abc", L"\u00E9", L"\U0001F3B5"};
        for (const wchar_t *w : wides) {
            char buffer[32];
            int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, buffer, 32, nullptr, nullptr);
            out.line({"wctomb", hexUnits(w, wcslen(w)), std::to_string(n),
                      escapeBytes(std::string(buffer, n))});
        }
        const wchar_t lone[][3] = {{0xD800, 0}, {L'a', 0xDC00, 0}, {0xDC00, 0xD800, 0}};
        for (auto &w : lone) {
            char buffer[32];
            int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, buffer, 32, nullptr, nullptr);
            out.line({"wctomb", hexUnits(w, wcslen(w)), std::to_string(n),
                      escapeBytes(std::string(buffer, n))});
        }

        // Length reporting with a terminator included and with an insufficient buffer.
        wchar_t shortBuffer[2];
        int n = MultiByteToWideChar(CP_UTF8, 0, "abc", -1, nullptr, 0);
        int m = MultiByteToWideChar(CP_UTF8, 0, "abc", -1, shortBuffer, 2);
        out.line({"mbtowc-length", std::to_string(n), std::to_string(m),
                  std::to_string(GetLastError())});
    }

}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        std::fprintf(stderr, "Usage: MsvcrtProbe <output directory> <scratch directory>\n");
        return 2;
    }
    std::string outDir = argv[1];
    std::string scratch = argv[2];

    loadMsvcrt();

    char module[MAX_PATH];
    GetModuleFileNameA(GetModuleHandleA("msvcrt.dll"), module, MAX_PATH);
    DWORD handle;
    DWORD size = GetFileVersionInfoSizeA(module, &handle);
    std::vector<char> info(size);
    std::string version = "unknown";
    VS_FIXEDFILEINFO *fixed;
    UINT fixedSize;
    if (size && GetFileVersionInfoA(module, 0, size, info.data()) &&
        VerQueryValueA(info.data(), "\\", reinterpret_cast<void **>(&fixed), &fixedSize)) {
        char buffer[64];
        std::snprintf(buffer, sizeof buffer, "%u.%u.%u.%u", HIWORD(fixed->dwFileVersionMS),
                      LOWORD(fixed->dwFileVersionMS), HIWORD(fixed->dwFileVersionLS),
                      LOWORD(fixed->dwFileVersionLS));
        version = buffer;
    }
    {
        Output out(outDir, "source.txt");
        out.line({"msvcrt", module, version});
        out.line({"acp", std::to_string(GetACP())});
    }

    probePrintf(outDir);
    probeQsort(outDir);
    probeRand(outDir);
    probeFopen(outDir, scratch);
    probeAtof(outDir);
    probeMath(outDir);
    probeStrings(outDir);
    probeCtype(outDir);
    probeCommandLine(outDir);
    probeUtf8(outDir);
    return 0;
}
