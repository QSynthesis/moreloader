#ifndef MORELOADER_TESTS_EXECUTABLEIMPORTS_H
#define MORELOADER_TESTS_EXECUTABLEIMPORTS_H

#include <string>
#include <vector>

#include <boost/test/unit_test.hpp>

#include <moreloader/Image/PEFile.h>

/// Access to the executables that the user provides. Their paths are passed to CMake as
/// MORE_TEST_MORESAMPLER_EXE and MORE_TEST_RESAMPLER_EXE. A test that reads them is skipped if a
/// path is empty or the file cannot be read, and the report lists it as skipped.
namespace testing {

    /// Path of moresampler.exe, or an empty string if none was configured.
    inline std::string moresamplerPath() {
        return MORE_TEST_MORESAMPLER_EXE;
    }

    /// Path of the resampler.exe of UTAU, or an empty string if none was configured.
    inline std::string resamplerPath() {
        return MORE_TEST_RESAMPLER_EXE;
    }

    /// Returns the imports of the executable at \a path, or an empty vector if it cannot be read.
    inline std::vector<more::loader::PEImport> importsOf(const std::string &path) {
        if (path.empty()) {
            return {};
        }
        std::string error;
        auto file = more::loader::PEFile::read(path, error);
        return file ? file->imports() : std::vector<more::loader::PEImport>();
    }

    /// Precondition of a test case that requires the executable at \a path.
    struct ExecutableAvailable {
        std::string path;

        inline boost::test_tools::assertion_result
            operator()(boost::unit_test::test_unit_id) const {
            boost::test_tools::assertion_result result(!importsOf(path).empty());
            result.message() << "no readable executable configured (" << path << ")";
            return result;
        }
    };

}

#endif // MORELOADER_TESTS_EXECUTABLEIMPORTS_H
