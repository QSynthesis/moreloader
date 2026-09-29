#include <string>
#include <vector>

#include <boost/test/unit_test.hpp>

#include <moreloader/Runtime/ExportRegistry.h>

using namespace more::loader;

namespace {

    int testlib_Answer() {
        return 42;
    }

    int testlib_Counter = 7;

    int a;
    int b;

}

BOOST_AUTO_TEST_SUITE(test_ExportRegistry)

BOOST_AUTO_TEST_CASE(test_normalize_library) {
    BOOST_TEST(ExportRegistry::normalizeLibrary("KERNEL32.dll") == "kernel32");
    BOOST_TEST(ExportRegistry::normalizeLibrary("msvcrt.DLL") == "msvcrt");
    BOOST_TEST(ExportRegistry::normalizeLibrary("Shell32") == "shell32");
    // Only a trailing .dll is removed, and a name that is only .dll is kept.
    BOOST_TEST(ExportRegistry::normalizeLibrary("a.dll.dll") == "a.dll");
    BOOST_TEST(ExportRegistry::normalizeLibrary(".dll") == ".dll");
    BOOST_TEST(ExportRegistry::normalizeLibrary("x.dllx") == "x.dllx");
}

BOOST_AUTO_TEST_CASE(test_find_ignores_letter_case_of_library_only) {
    ExportRegistry registry;
    registry.add("KERNEL32.dll", "GetVersion", &a);

    for (const char *library : {"KERNEL32.dll", "kernel32.dll", "kernel32", "Kernel32.DLL"}) {
        BOOST_TEST_CONTEXT(library) {
            auto found = registry.find(library, "GetVersion");
            BOOST_TEST_REQUIRE(found.has_value());
            BOOST_TEST(found->address == static_cast<void *>(&a));
            BOOST_TEST((found->kind == ExportKind::Function));
        }
    }
    // Export names are compared exactly.
    BOOST_TEST(!registry.find("kernel32", "getversion").has_value());
    BOOST_TEST(!registry.find("kernel32", "GetVersionA").has_value());
    BOOST_TEST(!registry.find("user32", "GetVersion").has_value());
}

BOOST_AUTO_TEST_CASE(test_later_addition_replaces) {
    ExportRegistry registry;
    registry.add("msvcrt.dll", "malloc", &a);
    registry.add("MSVCRT", "malloc", &b, ExportKind::Data);
    auto found = registry.find("msvcrt.dll", "malloc");
    BOOST_TEST_REQUIRE(found.has_value());
    BOOST_TEST(found->address == static_cast<void *>(&b));
    BOOST_TEST((found->kind == ExportKind::Data));
}

BOOST_AUTO_TEST_CASE(test_libraries) {
    ExportRegistry registry;
    BOOST_TEST(registry.libraries().empty());
    BOOST_TEST(!registry.hasLibrary("kernel32"));
    registry.add("USER32.dll", "MessageBoxW", &a);
    registry.add("kernel32.dll", "GetVersion", &a);
    registry.add("KERNEL32.dll", "GetACP", &b);
    BOOST_TEST(registry.hasLibrary("Kernel32.dll"));
    BOOST_TEST(registry.hasLibrary("user32"));
    BOOST_TEST(!registry.hasLibrary("shell32"));
    std::vector<std::string> expected = {"kernel32", "user32"};
    BOOST_TEST(registry.libraries() == expected, boost::test_tools::per_element());
}

BOOST_AUTO_TEST_CASE(test_register_macros) {
    ExportRegistry registry;
    MORE_REGISTER(registry, testlib, Answer);
    MORE_REGISTER_DATA(registry, testlib, Counter);

    auto function = registry.find("testlib.dll", "Answer");
    BOOST_TEST_REQUIRE(function.has_value());
    BOOST_TEST((function->kind == ExportKind::Function));
    BOOST_TEST(reinterpret_cast<int (*)()>(function->address)() == 42);

    auto data = registry.find("TESTLIB", "Counter");
    BOOST_TEST_REQUIRE(data.has_value());
    BOOST_TEST((data->kind == ExportKind::Data));
    BOOST_TEST(*static_cast<int *>(data->address) == 7);
}

BOOST_AUTO_TEST_SUITE_END()
