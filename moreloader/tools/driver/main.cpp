#include <cstdio>
#include <cstring>
#include <string>

#include <moreloader/CRT/CRTExports.h>
#include <moreloader/Image/PEFile.h>
#include <moreloader/Runtime/ExportRegistry.h>
#include <moreloader/Runtime/Process.h>
#include <moreloader/Support/Diagnostics.h>
#include <moreloader/WinAPI/WinAPIExports.h>

using namespace more::loader;

static void printUsage() {
    std::fprintf(stderr,
                 "Usage: moreloader [options] <moresampler.exe> [arguments...]\n"
                 "\n"
                 "Runs the 32-bit moresampler 0.8.4 on Linux. The arguments after the executable\n"
                 "are passed to it. Absolute host paths among them are presented to it as paths\n"
                 "on drive Z:.\n"
                 "\n"
                 "Options:\n"
                 "  --trace-imports   Report every call of an imported function\n"
                 "  --trace-stubs     Report calls that the loader implements only partially\n"
                 "  --debug-strings   Report the strings passed to OutputDebugStringA\n"
                 "  --check-heap      Report writes beyond the end of blocks of the msvcrt heap\n"
                 "  --help            Show this help\n");
}

int main(int argc, char *argv[]) {
    ProcessOptions options;
    int i = 1;
    for (; i < argc && std::strncmp(argv[i], "--", 2) == 0; ++i) {
        if (std::strcmp(argv[i], "--") == 0) {
            ++i;
            break;
        }
        if (std::strcmp(argv[i], "--trace-imports") == 0) {
            options.traceImports = true;
        } else if (std::strcmp(argv[i], "--trace-stubs") == 0) {
            setDiagnosticEnabled(DiagnosticCategory::Stubs, true);
        } else if (std::strcmp(argv[i], "--debug-strings") == 0) {
            setDiagnosticEnabled(DiagnosticCategory::DebugStrings, true);
        } else if (std::strcmp(argv[i], "--check-heap") == 0) {
            setDiagnosticEnabled(DiagnosticCategory::HeapCheck, true);
        } else if (std::strcmp(argv[i], "--help") == 0) {
            printUsage();
            return 0;
        } else {
            diagnostic("unknown option %s", argv[i]);
            printUsage();
            return 255;
        }
    }
    if (i >= argc) {
        printUsage();
        return 255;
    }
    options.imagePath = argv[i++];
    for (; i < argc; ++i) {
        options.arguments.push_back(argv[i]);
    }

    std::string errorMessage;
    auto file = PEFile::read(options.imagePath, errorMessage);
    if (!file) {
        diagnostic("%s: %s", options.imagePath.c_str(), errorMessage.c_str());
        return 255;
    }

    ExportRegistry registry;
    registerWinAPIExports(registry);
    registerCRTExports(registry);

    int code = process().run(*file, registry, options, errorMessage);
    diagnostic("%s: %s", options.imagePath.c_str(), errorMessage.c_str());
    return code;
}
