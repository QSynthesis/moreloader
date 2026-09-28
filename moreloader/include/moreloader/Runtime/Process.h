#ifndef MORELOADER_RUNTIME_PROCESS_H
#define MORELOADER_RUNTIME_PROCESS_H

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include <moreloader/Image/MappedImage.h>
#include <moreloader/Image/PEFile.h>
#include <moreloader/Runtime/ExportRegistry.h>
#include <moreloader/Runtime/GuestLayout.h>

namespace more::loader {

    class GuestThread;

    /// Options of a run of the guest, taken from the command line of the driver.
    struct ProcessOptions {
        /// Host path of the executable.
        std::string imagePath;

        /// Arguments after the executable, in the form given on the host.
        std::vector<std::string> arguments;

        /// Whether every call of an imported function is reported.
        bool traceImports = false;
    };

    /// The static TLS template of the image. Addresses are virtual addresses in the image.
    struct ProcessTLSTemplate {
        bool present = false;
        std::uint32_t startOfRawData = 0;
        std::uint32_t endOfRawData = 0;
        std::uint32_t sizeOfZeroFill = 0;
    };

    /// The emulated process: the mapped image, its command line and module path, and the
    /// process-wide state of the Windows functions.
    class Process {
    public:
        /// Maps the image, resolves its imports from \a registry, prepares the initial thread,
        /// runs the TLS callbacks and calls the entry point.
        ///
        /// \return only if the image cannot be loaded, with the exit code 255 and a description
        ///         in \a errorMessage. Otherwise the process exits through exit().
        int run(const PEFile &file, const ExportRegistry &registry, const ProcessOptions &options,
                std::string &errorMessage);

        /// Runs the exit sequence of \c ExitProcess: the TLS callbacks with
        /// \c DLL_PROCESS_DETACH on the calling thread, then the termination of the host process
        /// with \a exitCode. The C runtime runs its own exit handlers and flushes its streams
        /// before.
        ///
        /// \note Windows terminates the other threads before the callbacks run. The loader
        ///       cannot terminate host threads, therefore the callbacks are skipped if another
        ///       guest thread is still running.
        [[noreturn]] void exit(std::uint32_t exitCode);

        /// Terminates the host process immediately, as \c TerminateProcess does.
        [[noreturn]] void terminate(std::uint32_t exitCode);

        /// Calls each TLS callback of the image with \a reason on the calling thread.
        void runTLSCallbacks(DLLReason reason);

        inline MappedImage &image() {
            return *m_image;
        }

        inline const ExportRegistry &registry() const {
            return *m_registry;
        }

        inline std::uint32_t pebAddress() const {
            return m_pebAddress;
        }

        inline const ProcessTLSTemplate &tlsTemplate() const {
            return m_tlsTemplate;
        }

        inline GuestThread *initialThread() const {
            return m_initialThread;
        }

        /// Returns the guest path of the executable, such as <tt>Z:\\tools\\moresampler.exe</tt>.
        inline const std::u16string &modulePath() const {
            return m_modulePath;
        }

        /// Returns the command line as \c GetCommandLineW returns it. The storage lives as long
        /// as the process.
        inline const std::u16string &commandLine() const {
            return m_commandLine;
        }

        /// Returns the command line as \c _acmdln holds it, in UTF-8.
        inline const std::string &narrowCommandLine() const {
            return m_narrowCommandLine;
        }

        /// Returns the arguments as \c __getmainargs provides them, in UTF-8, including the
        /// program name.
        inline const std::vector<std::string> &arguments() const {
            return m_arguments;
        }

        /// Allocates a TLS slot.
        ///
        /// \return the slot index, or \c std::nullopt if all 64 slots are allocated
        std::optional<std::uint32_t> allocateTLSSlot();

        /// Returns whether \a slot was allocated by allocateTLSSlot().
        bool isTLSSlotAllocated(std::uint32_t slot);

        /// Adds a function that run() calls after the command line and the initial thread are
        /// prepared and before any guest code runs. The C runtime initializes its data exports
        /// this way.
        void addStartupHook(void (*hook)());

        /// Counts the guest threads other than the initial thread that are running.
        void threadStarted();
        void threadExited();

    private:
        std::unique_ptr<MappedImage> m_image;
        const ExportRegistry *m_registry = nullptr;
        std::uint32_t m_pebAddress = 0;
        ProcessTLSTemplate m_tlsTemplate;
        std::vector<std::uint32_t> m_tlsCallbacks;
        std::uint32_t m_entryPoint = 0;
        GuestThread *m_initialThread = nullptr;

        std::u16string m_modulePath;
        std::u16string m_commandLine;
        std::string m_narrowCommandLine;
        std::vector<std::string> m_arguments;

        std::mutex m_tlsMutex;
        std::uint64_t m_tlsSlots = 0;

        std::atomic<int> m_runningThreads{0};

        std::vector<void (*)()> m_startupHooks;
    };

    /// The process of the loader. There is exactly one.
    Process &process();

}

#endif // MORELOADER_RUNTIME_PROCESS_H
