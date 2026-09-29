#ifndef MORELOADER_TESTS_PROCESSTOOLS_H
#define MORELOADER_TESTS_PROCESSTOOLS_H

#include <sys/wait.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <functional>
#include <string>

/// Helpers for tests of code that writes to the standard error or terminates the process.
namespace testing {

    /// Runs \a body with the standard error of the process redirected to a temporary file.
    ///
    /// \return the bytes written to the standard error meanwhile
    inline std::string captureStderr(const std::function<void()> &body) {
        std::fflush(stderr);
        char path[] = "/tmp/moreloader-test-XXXXXX";
        int file = ::mkstemp(path);
        ::unlink(path);
        int saved = ::dup(STDERR_FILENO);
        ::dup2(file, STDERR_FILENO);
        body();
        std::fflush(stderr);
        ::dup2(saved, STDERR_FILENO);
        ::close(saved);

        std::string output;
        ::lseek(file, 0, SEEK_SET);
        char buffer[4096];
        ssize_t n;
        while ((n = ::read(file, buffer, sizeof buffer)) > 0) {
            output.append(buffer, std::size_t(n));
        }
        ::close(file);
        return output;
    }

    /// Outcome of a child process.
    struct ChildResult {
        /// Exit code, or -1 if the child was terminated by a signal.
        int exitCode = -1;

        /// Bytes that the child wrote to its standard error.
        std::string errorOutput;
    };

    /// Runs \a body in a child process created by \c fork and waits for it. If \a body returns,
    /// the child exits with code 0.
    inline ChildResult runInChild(const std::function<void()> &body) {
        int pipeEnds[2];
        if (::pipe(pipeEnds) != 0) {
            return {};
        }
        std::fflush(nullptr);
        pid_t child = ::fork();
        if (child == 0) {
            ::close(pipeEnds[0]);
            ::dup2(pipeEnds[1], STDERR_FILENO);
            body();
            std::fflush(nullptr);
            ::_exit(0);
        }
        ::close(pipeEnds[1]);
        ChildResult result;
        char buffer[4096];
        ssize_t n;
        while ((n = ::read(pipeEnds[0], buffer, sizeof buffer)) > 0) {
            result.errorOutput.append(buffer, std::size_t(n));
        }
        ::close(pipeEnds[0]);
        int status = 0;
        ::waitpid(child, &status, 0);
        if (WIFEXITED(status)) {
            result.exitCode = WEXITSTATUS(status);
        }
        return result;
    }

}

#endif // MORELOADER_TESTS_PROCESSTOOLS_H
