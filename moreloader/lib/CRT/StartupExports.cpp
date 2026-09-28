#include "CRTExports_p.h"

#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

#include <moreloader/Runtime/Process.h>
#include <moreloader/Support/Diagnostics.h>

extern char **environ;

namespace more::loader::msvcrt {

    namespace {

        using Initializer = void (*)();
        using ExitHandler = std::uint32_t;

        // Data exports.
        char *msvcrt__acmdln = nullptr;
        char **msvcrt___initenv = nullptr;
        std::int32_t msvcrt___mb_cur_max = 1;

        std::int32_t s_appType = 0;
        std::uint32_t s_userMathErrorHandler = 0;

        // The arguments of __getmainargs, alive until the process exits.
        std::vector<std::string> s_argumentStorage;
        std::vector<char *> s_argv;
        std::vector<char *> s_envp;

        // The table of _onexit. Handlers run in reverse order of registration.
        std::recursive_mutex s_exitMutex;
        std::vector<std::uint32_t> s_exitHandlers;

        // The locks of _lock and _unlock. msvcrt has 36 predefined locks.
        std::recursive_mutex s_locks[64];

        void runExitHandlers() {
            std::lock_guard<std::recursive_mutex> lock(s_exitMutex);
            while (!s_exitHandlers.empty()) {
                std::uint32_t handler = s_exitHandlers.back();
                s_exitHandlers.pop_back();
                if (handler) {
                    reinterpret_cast<void (*)()>(handler)();
                }
            }
        }

        // -----------------------------------------------------------------------------------

        /// Provides the arguments and the environment in UTF-8. Wildcard expansion, which
        /// MinGW requests if linked with CRT_glob, is not performed.
        std::int32_t MORE_CDECL msvcrt___getmainargs(std::int32_t *argc, char ***argv,
                                                    char ***envp, std::int32_t expandWildcards,
                                                    void *startInfo) {
            if (s_argv.empty()) {
                s_argumentStorage = process().arguments();
                for (std::string &argument : s_argumentStorage) {
                    s_argv.push_back(argument.data());
                }
                s_argv.push_back(nullptr);
                for (char **entry = environ; entry && *entry; ++entry) {
                    s_envp.push_back(*entry);
                }
                s_envp.push_back(nullptr);
            }
            if (expandWildcards && isDiagnosticEnabled(DiagnosticCategory::Stubs)) {
                diagnostic("__getmainargs: wildcard expansion is not performed");
            }
            *argc = std::int32_t(s_argv.size() - 1);
            *argv = s_argv.data();
            *envp = s_envp.data();
            msvcrt___initenv = s_envp.data();
            return 0;
        }

        void MORE_CDECL msvcrt___set_app_type(std::int32_t type) {
            s_appType = type;
        }

        /// Records the handler. The mathematical functions forwarded to the host never call
        /// it.
        void MORE_CDECL msvcrt___setusermatherr(std::uint32_t handler) {
            s_userMathErrorHandler = handler;
        }

        std::int32_t MORE_CDECL msvcrt___lconv_init() {
            return 0;
        }

        void MORE_CDECL msvcrt__initterm(const Initializer *begin, const Initializer *end) {
            for (const Initializer *p = begin; p < end; ++p) {
                if (*p) {
                    (*p)();
                }
            }
        }

        std::uint32_t MORE_CDECL msvcrt__onexit(std::uint32_t handler) {
            std::lock_guard<std::recursive_mutex> lock(s_exitMutex);
            s_exitHandlers.push_back(handler);
            return handler;
        }

        /// Appends \a handler to the table of a module, whose bounds the caller keeps. The
        /// table is an allocation of the C runtime, which the caller never frees itself.
        std::uint32_t MORE_CDECL msvcrt___dllonexit(std::uint32_t handler, std::uint32_t **begin,
                                                   std::uint32_t **end) {
            std::lock_guard<std::recursive_mutex> lock(s_exitMutex);
            std::size_t count = *begin ? std::size_t(*end - *begin) : 0;
            auto table = static_cast<std::uint32_t *>(
                std::realloc(*begin, (count + 1) * sizeof(std::uint32_t)));
            if (!table) {
                return 0;
            }
            table[count] = handler;
            *begin = table;
            *end = table + count + 1;
            return handler;
        }

        void MORE_CDECL msvcrt__cexit() {
            runExitHandlers();
            flushAllStreams();
        }

        void MORE_CDECL msvcrt_exit(std::int32_t code) {
            runExitHandlers();
            flushAllStreams();
            process().exit(std::uint32_t(code));
        }

        /// Ends the process without running the exit handlers or flushing the streams.
        void MORE_CDECL msvcrt__exit(std::int32_t code) {
            process().exit(std::uint32_t(code));
        }

        void MORE_CDECL msvcrt__amsg_exit(std::int32_t code) {
            char message[64];
            int length = std::snprintf(message, sizeof message, "\nruntime error R60%02d\n", code);
            ::write(STDERR_FILENO, message, std::size_t(length));
            process().terminate(255);
        }

        void MORE_CDECL msvcrt__lock(std::int32_t number) {
            if (number >= 0 && number < 64) {
                s_locks[number].lock();
            }
        }

        void MORE_CDECL msvcrt__unlock(std::int32_t number) {
            if (number >= 0 && number < 64) {
                s_locks[number].unlock();
            }
        }

    }

    void initializeStartupData() {
        msvcrt__acmdln = const_cast<char *>(process().narrowCommandLine().c_str());
    }

    void registerStartupExports(ExportRegistry &registry) {
        MORE_REGISTER_DATA(registry, msvcrt, _acmdln);
        MORE_REGISTER_DATA(registry, msvcrt, __initenv);
        MORE_REGISTER_DATA(registry, msvcrt, __mb_cur_max);
        MORE_REGISTER(registry, msvcrt, __getmainargs);
        MORE_REGISTER(registry, msvcrt, __set_app_type);
        MORE_REGISTER(registry, msvcrt, __setusermatherr);
        MORE_REGISTER(registry, msvcrt, __lconv_init);
        MORE_REGISTER(registry, msvcrt, _initterm);
        MORE_REGISTER(registry, msvcrt, _onexit);
        MORE_REGISTER(registry, msvcrt, __dllonexit);
        MORE_REGISTER(registry, msvcrt, _cexit);
        MORE_REGISTER(registry, msvcrt, exit);
        MORE_REGISTER(registry, msvcrt, _exit);
        MORE_REGISTER(registry, msvcrt, _amsg_exit);
        MORE_REGISTER(registry, msvcrt, _lock);
        MORE_REGISTER(registry, msvcrt, _unlock);
    }

}
