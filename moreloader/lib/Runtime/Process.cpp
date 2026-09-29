#include "Process.h"

#include <signal.h>
#include <sys/mman.h>
#include <ucontext.h>
#include <unistd.h>

#include <cstring>
#include <filesystem>

#include <moreloader/Support/CodePage.h>
#include <moreloader/Support/CommandLine.h>
#include <moreloader/Support/Diagnostics.h>
#include <moreloader/Support/MoreLoaderSupportGlobal.h>
#include <moreloader/Support/PathMapping.h>

#include "GuestThread.h"
#include "Thunks.h"

namespace more::loader {

    namespace {

        using TLSCallback = void(MORE_STDCALL *)(std::uint32_t, std::uint32_t, std::uint32_t);
        using EntryPoint = std::uint32_t (*)(std::uint32_t);

        std::uint32_t addressOf(const void *p) {
            return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p));
        }

        const char *signalName(int number) {
            switch (number) {
                case SIGSEGV:
                    return "SIGSEGV";
                case SIGBUS:
                    return "SIGBUS";
                case SIGILL:
                    return "SIGILL";
                case SIGFPE:
                    return "SIGFPE";
                default:
                    return "signal";
            }
        }

        // Reports a fault of the guest or of the loader with the location, and terminates. The
        // guest has no exception handling that the loader emulates, therefore a fault ends the
        // process as an unhandled exception does on Windows.
        void faultHandler(int number, siginfo_t *info, void *context) {
            auto uc = static_cast<ucontext_t *>(context);
            std::uint32_t eip = uc->uc_mcontext.gregs[REG_EIP];
            std::uint32_t esp = uc->uc_mcontext.gregs[REG_ESP];
            GuestThread *thread = GuestThread::current();
            Process &p = process();
            bool inImage = p.image().contains(eip);
            diagnostic("%s at eip 0x%08x%s, address 0x%08x, esp 0x%08x, thread %u, last import %s",
                       signalName(number), eip, inImage ? " (image)" : "",
                       addressOf(info->si_addr), esp, thread ? unsigned(thread->threadID()) : 0u,
                       thread && thread->currentImport() ? thread->currentImport() : "unknown");
            ::_exit(128 + number);
        }

        void installFaultHandlers() {
            struct sigaction action{};
            action.sa_sigaction = faultHandler;
            action.sa_flags = SA_SIGINFO | SA_ONSTACK;
            sigemptyset(&action.sa_mask);
            for (int number : {SIGSEGV, SIGBUS, SIGILL, SIGFPE}) {
                ::sigaction(number, &action, nullptr);
            }
        }

        std::string absoluteHostPath(const std::string &path) {
            std::error_code error;
            auto absolute = std::filesystem::absolute(path, error);
            if (error) {
                return path;
            }
            return absolute.lexically_normal().string();
        }

    }

    int Process::run(const PEFile &file, const ExportRegistry &registry,
                     const ProcessOptions &options, std::string &errorMessage) {
        m_registry = &registry;

        auto image = MappedImage::map(file, errorMessage);
        if (!image) {
            return 255;
        }
        m_image = std::make_unique<MappedImage>(std::move(*image));

        // Imports. An import that is not implemented receives a thunk that reports it when
        // called, so that imports that the guest never calls do not prevent loading.
        for (const PEImport &import : file.imports()) {
            std::string name = import.name.empty() ? "#" + std::to_string(import.ordinal)
                                                   : import.name;
            std::uint32_t address;
            if (auto found = registry.find(import.library, name)) {
                address = addressOf(found->address);
                if (options.traceImports && found->kind == ExportKind::Function) {
                    address = makeTraceThunk(import.library, name, address);
                }
            } else {
                address = makeUnresolvedThunk(import.library, name);
                if (isDiagnosticEnabled(DiagnosticCategory::Stubs)) {
                    diagnostic("unresolved import %s!%s", import.library.c_str(), name.c_str());
                }
            }
            m_image->write32(import.slotRVA, address);
        }

        // Static TLS. The index variable receives 0, the only module with TLS being the image.
        if (const auto &tls = file.tls()) {
            m_tlsTemplate.present = true;
            m_tlsTemplate.startOfRawData = tls->startOfRawData;
            m_tlsTemplate.endOfRawData = tls->endOfRawData;
            m_tlsTemplate.sizeOfZeroFill = tls->sizeOfZeroFill;
            m_tlsCallbacks = tls->callbacks;
            if (tls->indexAddress != 0) {
                m_image->write32(tls->indexAddress - m_image->base(), 0);
            }
        }

        if (!m_image->protect(errorMessage)) {
            return 255;
        }
        m_entryPoint = m_image->base() + file.entryPointRVA();

        void *peb = ::mmap(nullptr, sizeof(PEB32), PROT_READ | PROT_WRITE,
                           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (peb == MAP_FAILED) {
            errorMessage = "cannot allocate the PEB";
            return 255;
        }
        static_cast<PEB32 *>(peb)->ImageBaseAddress = m_image->base();
        m_pebAddress = addressOf(peb);

        // The command line of the guest. The program name is the guest path of the executable,
        // and absolute host paths among the arguments become paths on drive Z:.
        std::string modulePath = guestPathFromHost(absoluteHostPath(options.imagePath));
        m_modulePath = multiByteToWide(modulePath).text;
        m_arguments.push_back(modulePath);
        std::vector<std::u16string> wideArguments = {m_modulePath};
        for (const std::string &argument : options.arguments) {
            std::string guest = guestArgumentFromHost(argument, [](const std::string &path) {
                return ::access(path.c_str(), F_OK) == 0;
            });
            m_arguments.push_back(guest);
            wideArguments.push_back(multiByteToWide(guest).text);
        }
        m_commandLine = joinCommandLine(wideArguments);
        m_narrowCommandLine = wideToMultiByte(m_commandLine).text;

        m_initialThread = &GuestThread::attachInitialThread();
        installFaultHandlers();

        for (auto hook : m_startupHooks) {
            hook();
        }

        runTLSCallbacks(DLLProcessAttach);

        // The entry point of an executable is called by the initial thread with the address of
        // the PEB. mainCRTStartup of MinGW ignores the argument and ends the process with exit.
        std::uint32_t result = reinterpret_cast<EntryPoint>(m_entryPoint)(m_pebAddress);
        exit(result);
    }

    void Process::runTLSCallbacks(DLLReason reason) {
        // Windows passes a non-null reserved argument for the notifications of a statically
        // loaded module at process start and at process exit, and null for thread
        // notifications.
        std::uint32_t reserved =
            (reason == DLLProcessAttach || reason == DLLProcessDetach) ? 1 : 0;
        for (std::uint32_t callback : m_tlsCallbacks) {
            reinterpret_cast<TLSCallback>(callback)(m_image->base(), reason, reserved);
        }
    }

    void Process::exit(std::uint32_t exitCode) {
        if (m_runningThreads.load() == 0) {
            runTLSCallbacks(DLLProcessDetach);
        }
        ::_exit(int(exitCode));
    }

    void Process::terminate(std::uint32_t exitCode) {
        ::_exit(int(exitCode));
    }

    std::optional<std::uint32_t> Process::allocateTLSSlot() {
        std::lock_guard<std::mutex> lock(m_tlsMutex);
        for (std::uint32_t slot = 0; slot < tlsMinimumAvailable; ++slot) {
            if (!(m_tlsSlots & (std::uint64_t(1) << slot))) {
                m_tlsSlots |= std::uint64_t(1) << slot;
                return slot;
            }
        }
        return std::nullopt;
    }

    bool Process::isTLSSlotAllocated(std::uint32_t slot) {
        std::lock_guard<std::mutex> lock(m_tlsMutex);
        return slot < tlsMinimumAvailable && (m_tlsSlots & (std::uint64_t(1) << slot));
    }

    void Process::addStartupHook(void (*hook)()) {
        m_startupHooks.push_back(hook);
    }

    void Process::threadStarted() {
        ++m_runningThreads;
    }

    void Process::threadExited() {
        --m_runningThreads;
    }

    Process &process() {
        static Process instance;
        return instance;
    }

}
