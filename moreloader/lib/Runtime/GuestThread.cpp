#include "GuestThread.h"

#include <pthread.h>
#include <signal.h>
#include <sys/mman.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <cstring>

#include <moreloader/Support/Diagnostics.h>
#include <moreloader/Support/FloatingPoint.h>

#include "LDT.h"
#include "Process.h"

namespace more::loader {

    namespace {

        thread_local GuestThread *t_current = nullptr;

        std::atomic<std::uint32_t> s_nextThreadID{0x100};

        // The stack reserve of moresampler.
        constexpr std::uint32_t minimumStackSize = 0x200000;

        struct StartInfo {
            GuestThreadStart start;
            void *parameter;
            std::shared_ptr<ThreadObject> object;
        };

        std::uint32_t addressOf(const void *p) {
            return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p));
        }

    }

    std::uint32_t allocateThreadID() {
        return s_nextThreadID.fetch_add(4);
    }

    GuestThread *GuestThread::current() {
        return t_current;
    }

    GuestThread::GuestThread(std::shared_ptr<ThreadObject> object) : m_object(std::move(object)) {
    }

    GuestThread::~GuestThread() = default;

    void GuestThread::attach() {
        void *teb = ::mmap(nullptr, sizeof(TEB32), PROT_READ | PROT_WRITE,
                           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (teb == MAP_FAILED) {
            fatal("cannot allocate a TEB");
        }
        m_teb = static_cast<TEB32 *>(teb);

        pthread_attr_t attributes;
        void *stackAddress = nullptr;
        std::size_t stackSize = 0;
        if (::pthread_getattr_np(::pthread_self(), &attributes) == 0) {
            ::pthread_attr_getstack(&attributes, &stackAddress, &stackSize);
            ::pthread_attr_destroy(&attributes);
        }

        // An empty exception registration chain is terminated by 0xFFFFFFFF.
        m_teb->ExceptionList = 0xFFFFFFFF;
        m_teb->StackBase = addressOf(static_cast<char *>(stackAddress) + stackSize);
        m_teb->StackLimit = addressOf(stackAddress);
        m_teb->Self = addressOf(m_teb);
        m_teb->UniqueProcess = static_cast<std::uint32_t>(::getpid());
        m_teb->UniqueThread = m_object->threadID();
        m_teb->ProcessEnvironmentBlock = process().pebAddress();

        // Static TLS: a copy of the template followed by the zero fill, referenced by index 0
        // of the TLS array, which is the index that the loader stores into the image.
        const ProcessTLSTemplate &tls = process().tlsTemplate();
        if (tls.present) {
            std::uint32_t dataSize = tls.endOfRawData - tls.startOfRawData;
            std::uint32_t size = dataSize + tls.sizeOfZeroFill;
            m_tlsBlock = std::calloc(1, size ? size : 1);
            std::memcpy(m_tlsBlock,
                        reinterpret_cast<const void *>(static_cast<std::uintptr_t>(tls.startOfRawData)),
                        dataSize);
            m_tlsArray = static_cast<std::uint32_t *>(std::calloc(1, sizeof(std::uint32_t)));
            m_tlsArray[0] = addressOf(m_tlsBlock);
            m_teb->ThreadLocalStoragePointer = addressOf(m_tlsArray);
        }

        auto selector = allocateDataSegment(addressOf(m_teb), sizeof(TEB32) - 1);
        if (!selector) {
            fatal("cannot allocate an LDT entry for the TEB of thread %u",
                  unsigned(m_object->threadID()));
        }
        m_selector = *selector;
        loadFS(m_selector);

        setX87ControlWord(windowsControlWord);
        setMXCSR(windowsMXCSR);

        // An alternate stack, so that a stack overflow of the guest can still be reported.
        std::size_t signalStackSize = 64 * 1024;
        m_signalStack = std::malloc(signalStackSize);
        stack_t signalStack{};
        signalStack.ss_sp = m_signalStack;
        signalStack.ss_size = signalStackSize;
        ::sigaltstack(&signalStack, nullptr);

        t_current = this;
    }

    void GuestThread::detach() {
        stack_t disable{};
        disable.ss_flags = SS_DISABLE;
        ::sigaltstack(&disable, nullptr);
        std::free(m_signalStack);

        t_current = nullptr;
        loadFS(0);
        releaseDataSegment(m_selector);
        std::free(m_tlsArray);
        std::free(m_tlsBlock);
        ::munmap(m_teb, sizeof(TEB32));
        m_teb = nullptr;
    }

    GuestThread &GuestThread::attachInitialThread() {
        auto object = std::make_shared<ThreadObject>(allocateThreadID(), 0);
        // The initial thread exists until the process exits.
        auto thread = new GuestThread(std::move(object));
        thread->attach();
        return *thread;
    }

    std::optional<std::shared_ptr<ThreadObject>>
        GuestThread::create(GuestThreadStart start, void *parameter, std::uint32_t stackSize,
                            bool suspended) {
        auto object = std::make_shared<ThreadObject>(allocateThreadID(), suspended ? 1 : 0);
        auto info = new StartInfo{start, parameter, object};

        pthread_attr_t attributes;
        ::pthread_attr_init(&attributes);
        ::pthread_attr_setstacksize(&attributes, std::max(stackSize, minimumStackSize));
        ::pthread_attr_setdetachstate(&attributes, PTHREAD_CREATE_DETACHED);
        pthread_t handle;
        int error = ::pthread_create(&handle, &attributes, hostThreadMain, info);
        ::pthread_attr_destroy(&attributes);
        if (error != 0) {
            delete info;
            return std::nullopt;
        }
        process().threadStarted();
        return object;
    }

    void *GuestThread::hostThreadMain(void *argument) {
        StartInfo info = *static_cast<StartInfo *>(argument);
        delete static_cast<StartInfo *>(argument);

        auto thread = new GuestThread(info.object);
        thread->attach();

        // A thread created suspended runs nothing, not even the TLS callbacks, before its
        // first resumption.
        info.object->waitUntilResumed();
        process().runTLSCallbacks(DLLThreadAttach);

        // _endthreadex returns here through the jump buffer. Between setjmp and the call of the
        // guest there are no objects with destructors, and neither are there in the frames of
        // the guest and of the wrapper that longjmp skips.
        volatile std::uint32_t exitCode;
        if (setjmp(thread->m_exitJump) == 0) {
            exitCode = info.start(info.parameter);
        } else {
            exitCode = thread->m_exitCode;
        }

        process().runTLSCallbacks(DLLThreadDetach);
        thread->detach();
        process().threadExited();
        info.object->markExited(exitCode);
        delete thread;
        return nullptr;
    }

    void GuestThread::exit(std::uint32_t exitCode) {
        if (this == process().initialThread()) {
            // Windows ends the process when its last thread exits. moresampler does not end its
            // initial thread with _endthreadex.
            process().exit(exitCode);
        }
        m_exitCode = exitCode;
        std::longjmp(m_exitJump, 1);
    }

    std::uint32_t lastError() {
        GuestThread *thread = GuestThread::current();
        return thread ? thread->teb()->LastErrorValue : 0;
    }

    void setLastError(std::uint32_t error) {
        if (GuestThread *thread = GuestThread::current()) {
            thread->teb()->LastErrorValue = error;
        }
    }

}
