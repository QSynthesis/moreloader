#ifndef MORELOADER_RUNTIME_GUESTTHREAD_H
#define MORELOADER_RUNTIME_GUESTTHREAD_H

#include <csetjmp>
#include <cstdint>
#include <memory>
#include <optional>

#include <moreloader/Runtime/GuestLayout.h>
#include <moreloader/Runtime/KernelObjects.h>
#include <moreloader/Support/MoreLoaderSupportGlobal.h>

namespace more::loader {

    /// Entry point of a guest thread, as passed to \c _beginthreadex.
    using GuestThreadStart = std::uint32_t(MORE_STDCALL *)(void *);

    /// The state of a host thread that runs guest code.
    ///
    /// Before any guest code runs on a thread, the thread receives a TEB of its own, an LDT
    /// entry for that TEB loaded into FS, the x87 control word and MXCSR of Windows, a copy of
    /// the static TLS template of the image and an alternate signal stack for fault reports.
    class GuestThread {
    public:
        /// Returns the guest thread of the calling host thread, or \c nullptr if the calling
        /// thread does not run guest code.
        static GuestThread *current();

        inline TEB32 *teb() const {
            return m_teb;
        }

        inline std::uint32_t threadID() const {
            return m_object->threadID();
        }

        inline const std::shared_ptr<ThreadObject> &object() const {
            return m_object;
        }

        /// Records the address of the import whose wrapper is running, for fault reports.
        inline void setCurrentImport(const char *name) {
            m_currentImport = name;
        }

        inline const char *currentImport() const {
            return m_currentImport;
        }

        /// Prepares the calling host thread, which is the initial thread of the process, to run
        /// guest code. The TLS callbacks are run by the caller.
        static GuestThread &attachInitialThread();

        /// Creates a guest thread that calls \a start with \a parameter.
        ///
        /// The host stack has at least \a stackSize bytes and at least the 2 MiB that
        /// moresampler reserves in its header. If \a suspended, the thread runs no guest code,
        /// including the TLS callbacks, until \c ResumeThread.
        ///
        /// \return the thread object, or \c std::nullopt if the host cannot create a thread
        static std::optional<std::shared_ptr<ThreadObject>>
            create(GuestThreadStart start, void *parameter, std::uint32_t stackSize, bool suspended);

        /// Ends the calling guest thread with \a exitCode, as \c _endthreadex does. The TLS
        /// callbacks run with \c DLL_THREAD_DETACH before the thread object is signaled.
        [[noreturn]] void exit(std::uint32_t exitCode);

        GuestThread(const GuestThread &) = delete;
        GuestThread &operator=(const GuestThread &) = delete;

    private:
        explicit GuestThread(std::shared_ptr<ThreadObject> object);
        ~GuestThread();

        void attach();
        void detach();

        static void *hostThreadMain(void *argument);

        std::shared_ptr<ThreadObject> m_object;
        TEB32 *m_teb = nullptr;
        std::uint16_t m_selector = 0;
        void *m_tlsBlock = nullptr;
        std::uint32_t *m_tlsArray = nullptr;
        void *m_signalStack = nullptr;
        const char *m_currentImport = nullptr;

        std::jmp_buf m_exitJump;
        std::uint32_t m_exitCode = 0;
    };

    /// Returns the last-error value of the calling guest thread.
    std::uint32_t lastError();

    /// Sets the last-error value of the calling guest thread, which is stored in its TEB.
    void setLastError(std::uint32_t error);

    /// Returns a new thread identifier. Identifiers are multiples of 4, as on Windows.
    std::uint32_t allocateThreadID();

}

#endif // MORELOADER_RUNTIME_GUESTTHREAD_H
