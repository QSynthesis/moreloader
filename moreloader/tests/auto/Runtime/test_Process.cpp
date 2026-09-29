#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <string>
#include <vector>

#include <boost/test/unit_test.hpp>

#include <moreloader/Image/PEFile.h>
#include <moreloader/Runtime/ExportRegistry.h>
#include <moreloader/Runtime/GuestLayout.h>
#include <moreloader/Runtime/GuestThread.h>
#include <moreloader/Runtime/Process.h>
#include <moreloader/Support/Diagnostics.h>
#include <moreloader/Support/MoreLoaderSupportGlobal.h>

#include "PEBuilder.h"
#include "ProcessTools.h"

using namespace more::loader;
using namespace testing;

namespace {

    constexpr std::uint32_t imageBase = 0x36000000;

    // The TLS template in the data section: eight bytes of data followed by eight of zero fill.
    constexpr std::uint32_t tlsDataRVA = 0x2380;
    constexpr std::uint32_t tlsIndexRVA = 0x2390;

    std::uint32_t addressOf(const void *p) {
        return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p));
    }

    template <class T>
    const T *at(std::uint32_t address) {
        return reinterpret_cast<const T *>(static_cast<std::uintptr_t>(address));
    }

    // Calls of the TLS callback, recorded in the child process.
    std::vector<std::uint32_t> s_callbackReasons;
    std::uint32_t s_callbackModule = 0;
    std::uint32_t s_callbackReserved = 0;

    void MORE_WINAPI tlsCallback(std::uint32_t module, std::uint32_t reason,
                                 std::uint32_t reserved) {
        if (reason == DLLProcessDetach) {
            std::fprintf(stderr, "detach\n");
            return;
        }
        s_callbackReasons.push_back(reason);
        s_callbackModule = module;
        s_callbackReserved = reserved;
    }

    // The import that the entry point calls with the PEB. Returns a bit for each failed check,
    // which becomes the exit code of the process.
    std::uint32_t MORE_WINAPI testlib_Check(std::uint32_t peb) {
        std::uint32_t failures = 0;
        Process &p = process();
        if (at<PEB32>(peb)->ImageBaseAddress != imageBase || peb != p.pebAddress()) {
            failures |= 1;
        }
        GuestThread *thread = GuestThread::current();
        if (!thread) {
            return failures | 64;
        }
        std::uint32_t self;
        asm volatile("movl %%fs:0x18, %0" : "=r"(self));
        if (thread != p.initialThread() || self != addressOf(thread->teb()) ||
            thread->teb()->ProcessEnvironmentBlock != peb) {
            failures |= 2;
        }
        std::vector<std::string> arguments = {"Z:\\tmp\\moreloader-process-test.exe", "Z:\\tmp",
                                              "C4", "/N/i/y"};
        if (p.arguments() != arguments ||
            p.narrowCommandLine() != "Z:\\tmp\\moreloader-process-test.exe Z:\\tmp C4 /N/i/y" ||
            p.modulePath() != u"Z:\\tmp\\moreloader-process-test.exe") {
            failures |= 4;
        }
        // The index variable is 0 and the thread has a copy of the template with the zero fill.
        const std::uint32_t *array = at<std::uint32_t>(thread->teb()->ThreadLocalStoragePointer);
        const std::uint8_t *block = at<std::uint8_t>(array[0]);
        static const std::uint8_t expected[16] = {1, 2, 3, 4, 5, 6, 7, 8};
        if (*at<std::uint32_t>(imageBase + tlsIndexRVA) != 0 ||
            std::memcmp(block, expected, 16) != 0 || addressOf(block) == imageBase + tlsDataRVA) {
            failures |= 8;
        }
        // The callback ran once, before the entry point, with a non-null reserved argument.
        if (s_callbackReasons != std::vector<std::uint32_t>{DLLProcessAttach} ||
            s_callbackModule != imageBase || s_callbackReserved == 0) {
            failures |= 16;
        }
        if (p.image().base() != imageBase || !p.tlsTemplate().present) {
            failures |= 32;
        }
        return failures;
    }

    // An image whose entry point passes the PEB to the import \a function of testlib.dll and
    // returns its result.
    PEFile entryCalling(const std::string &function) {
        PEBuilder builder;
        builder.imageBase = imageBase;
        builder.entryPointRVA = 0x1000;

        BuilderSection data{".data", 0x2000, 0x400, {}, dataSection};
        ImportPlacement imports = placeImports(data, 0,
                                               {
                                                   {"testlib.dll", {function}}
        });
        builder.setDirectory(ImportDirectory, imports.directoryRVA, imports.directorySize);
        for (std::uint8_t i = 0; i < 8; ++i) {
            data.data.resize(std::max<std::size_t>(data.data.size(), tlsDataRVA - 0x2000 + 8));
            data.data[tlsDataRVA - 0x2000 + i] = std::uint8_t(i + 1);
        }
        std::uint32_t tls = placeTLS(data, 0x300, imageBase, imageBase + tlsDataRVA,
                                     imageBase + tlsDataRVA + 8, imageBase + tlsIndexRVA, 8,
                                     {addressOf(reinterpret_cast<const void *>(&tlsCallback))});
        put32(data.data, tlsIndexRVA - 0x2000, 0xFFFFFFFF);
        builder.setDirectory(TLSDirectory, tls, 24);

        // mov eax, [esp+4], push eax, call [slot], ret. The loader calls the entry point with
        // the C calling convention, and the import removes its argument.
        std::uint32_t slot = imageBase + imports.slots["testlib.dll!" + function];
        std::vector<std::uint8_t> code = {0x8B, 0x44, 0x24, 0x04, 0x50, 0xFF, 0x15};
        code.resize(code.size() + 4);
        std::memcpy(code.data() + 7, &slot, 4);
        code.push_back(0xC3);

        builder.sections = {
            {".text", 0x1000, std::uint32_t(code.size()), code, codeSection},
            data
        };
        std::string error;
        auto file = PEFile::parse(builder.build(), error);
        BOOST_REQUIRE_MESSAGE(file.has_value(), error);
        return *file;
    }

    ProcessOptions options() {
        ProcessOptions result;
        result.imagePath = "/tmp/moreloader-process-test.exe";
        result.arguments = {"/tmp", "C4", "/N/i/y"};
        return result;
    }

}

BOOST_AUTO_TEST_SUITE(test_Process)

BOOST_AUTO_TEST_CASE(test_run_resolves_imports_and_calls_the_entry_point) {
    PEFile file = entryCalling("Check");
    ChildResult child = runInChild([&] {
        ExportRegistry registry;
        MORE_REGISTER(registry, testlib, Check);
        std::string error;
        process().run(file, registry, options(), error);
        std::fprintf(stderr, "run returned: %s\n", error.c_str());
    });
    // Each bit of the exit code marks a failed check in testlib_Check.
    BOOST_TEST(child.exitCode == 0, child.errorOutput);
    // Process::exit runs the callbacks with DLL_PROCESS_DETACH if no other thread runs.
    BOOST_TEST(child.errorOutput == "detach\n");
}

BOOST_AUTO_TEST_CASE(test_trace_imports) {
    PEFile file = entryCalling("Check");
    ChildResult child = runInChild([&] {
        ExportRegistry registry;
        MORE_REGISTER(registry, testlib, Check);
        ProcessOptions traced = options();
        traced.traceImports = true;
        std::string error;
        process().run(file, registry, traced, error);
    });
    BOOST_TEST(child.exitCode == 0, child.errorOutput);
    BOOST_TEST(child.errorOutput.find("] testlib.dll!Check\n") != std::string::npos,
               child.errorOutput);
}

BOOST_AUTO_TEST_CASE(test_unresolved_import) {
    PEFile file = entryCalling("Missing");
    ChildResult child = runInChild([&] {
        setDiagnosticEnabled(DiagnosticCategory::Stubs, true);
        ExportRegistry registry;
        std::string error;
        process().run(file, registry, options(), error);
    });
    // Loading succeeds and the call terminates the process.
    BOOST_TEST(child.exitCode == 255);
    BOOST_TEST(child.errorOutput.rfind("moreloader: unresolved import testlib.dll!Missing\n", 0) ==
                   0u,
               child.errorOutput);
    BOOST_TEST(child.errorOutput.find(" called testlib.dll!Missing, which is not implemented\n") !=
                   std::string::npos,
               child.errorOutput);
}

BOOST_AUTO_TEST_CASE(test_run_fails_if_the_image_cannot_be_mapped) {
    PEFile file = entryCalling("Check");
    ChildResult child = runInChild([&] {
        // Occupy the range of the image first.
        std::string error;
        auto occupied = MappedImage::map(file, error);
        if (!occupied) {
            std::_Exit(10);
        }
        ExportRegistry registry;
        int code = process().run(file, registry, options(), error);
        std::fprintf(stderr, "%d %s\n", code, error.c_str());
    });
    BOOST_TEST(child.exitCode == 0);
    BOOST_TEST(child.errorOutput.rfind("255 cannot reserve the image range at 0x36000000", 0) == 0u,
               child.errorOutput);
}

BOOST_AUTO_TEST_CASE(test_tls_slots) {
    ChildResult child = runInChild([] {
        std::set<std::uint32_t> slots;
        for (std::uint32_t i = 0; i < tlsMinimumAvailable; ++i) {
            auto slot = process().allocateTLSSlot();
            if (!slot || !process().isTLSSlotAllocated(*slot)) {
                std::_Exit(1);
            }
            slots.insert(*slot);
        }
        if (slots.size() != tlsMinimumAvailable || *slots.rbegin() != tlsMinimumAvailable - 1) {
            std::_Exit(2);
        }
        if (process().allocateTLSSlot().has_value() ||
            process().isTLSSlotAllocated(tlsMinimumAvailable)) {
            std::_Exit(3);
        }
    });
    BOOST_TEST(child.exitCode == 0);
}

BOOST_AUTO_TEST_CASE(test_startup_hooks_run_before_the_entry_point) {
    static int s_hookCalls;
    PEFile file = entryCalling("Check");
    ChildResult child = runInChild([&] {
        process().addStartupHook([] {
            ++s_hookCalls;
            std::fprintf(stderr, "hook %d\n", s_hookCalls);
        });
        ExportRegistry registry;
        MORE_REGISTER(registry, testlib, Check);
        std::string error;
        process().run(file, registry, options(), error);
    });
    BOOST_TEST(child.exitCode == 0, child.errorOutput);
    BOOST_TEST(child.errorOutput == "hook 1\ndetach\n");
}

BOOST_AUTO_TEST_SUITE_END()
