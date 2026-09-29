#include <unistd.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <thread>

#include <boost/test/unit_test.hpp>

#include <moreloader/Runtime/GuestThread.h>
#include <moreloader/Runtime/KernelObjects.h>
#include <moreloader/Support/FloatingPoint.h>

using namespace more::loader;

namespace {

    std::uint32_t addressOf(const void *p) {
        return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p));
    }

    // The state that a new guest thread observes, recorded by the thread itself.
    struct Observation {
        bool isGuestThread = false;
        std::uint32_t fsSelf = 0;
        std::uint32_t tebAddress = 0;
        TEB32 teb{};
        std::uint32_t threadID = 0;
        std::uint32_t local = 0;
        unsigned short controlWord = 0;
        std::uint32_t mxcsr = 0;
        std::uint32_t lastError = 0;
    };

    std::uint32_t MORE_STDCALL observeThread(void *parameter) {
        auto observation = static_cast<Observation *>(parameter);
        GuestThread *thread = GuestThread::current();
        observation->isGuestThread = thread != nullptr;
        if (!thread) {
            return 1;
        }
        asm volatile("movl %%fs:0x18, %0" : "=r"(observation->fsSelf));
        observation->controlWord = x87ControlWord();
        asm volatile("stmxcsr %0" : "=m"(observation->mxcsr));
        setLastError(0x1234);
        observation->lastError = lastError();
        observation->tebAddress = addressOf(thread->teb());
        observation->teb = *thread->teb();
        observation->threadID = thread->threadID();
        observation->local = addressOf(&observation);
        return 7;
    }

    std::atomic<bool> s_started{false};

    std::uint32_t MORE_STDCALL markStarted(void *) {
        s_started = true;
        return 0;
    }

    std::atomic<bool> s_continuedAfterExit{false};

    std::uint32_t MORE_STDCALL exitEarly(void *) {
        GuestThread::current()->exit(9);
        s_continuedAfterExit = true;
        return 0;
    }

    bool waitFor(const std::shared_ptr<ThreadObject> &thread) {
        return waitForObjects({thread}, false, 5000) == WaitObject0;
    }

}

BOOST_AUTO_TEST_SUITE(test_GuestThread)

BOOST_AUTO_TEST_CASE(test_new_thread_state) {
    // A Linux process starts with 0x37F. The new thread must not inherit it.
    unsigned short saved = x87ControlWord();
    setX87ControlWord(0x37F);
    Observation observation;
    auto thread = GuestThread::create(observeThread, &observation, 0, false);
    setX87ControlWord(saved);
    BOOST_TEST_REQUIRE(thread.has_value());
    BOOST_TEST_REQUIRE(waitFor(*thread));

    BOOST_TEST_REQUIRE(observation.isGuestThread);
    BOOST_TEST(observation.threadID == (*thread)->threadID());
    BOOST_TEST(observation.fsSelf == observation.tebAddress);
    BOOST_TEST(observation.teb.Self == observation.tebAddress);
    BOOST_TEST(observation.teb.ExceptionList == 0xFFFFFFFFu);
    BOOST_TEST(observation.teb.UniqueProcess == std::uint32_t(::getpid()));
    BOOST_TEST(observation.teb.UniqueThread == observation.threadID);
    BOOST_TEST(observation.teb.StackLimit < observation.local);
    BOOST_TEST(observation.local < observation.teb.StackBase);
    // The host stack has at least the 2 MiB that moresampler reserves.
    BOOST_TEST(observation.teb.StackBase - observation.teb.StackLimit >= 0x200000u);
    BOOST_TEST(observation.controlWord == windowsControlWord);
    BOOST_TEST(observation.mxcsr == windowsMXCSR);
    BOOST_TEST(observation.lastError == 0x1234u);
    BOOST_TEST(observation.teb.LastErrorValue == 0x1234u);
}

BOOST_AUTO_TEST_CASE(test_suspended_thread_waits_for_resume) {
    s_started = false;
    auto thread = GuestThread::create(markStarted, nullptr, 0, true);
    BOOST_TEST_REQUIRE(thread.has_value());
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    BOOST_TEST(!s_started);
    BOOST_TEST(waitForObjects({*thread}, false, 0) == WaitTimeout);
    BOOST_TEST((*thread)->resume() == 1u);
    BOOST_TEST_REQUIRE(waitFor(*thread));
    BOOST_TEST(s_started);
}

BOOST_AUTO_TEST_CASE(test_exit_ends_the_thread) {
    s_continuedAfterExit = false;
    auto thread = GuestThread::create(exitEarly, nullptr, 0, false);
    BOOST_TEST_REQUIRE(thread.has_value());
    BOOST_TEST_REQUIRE(waitFor(*thread));
    BOOST_TEST(!s_continuedAfterExit);
}

BOOST_AUTO_TEST_CASE(test_thread_ids) {
    std::uint32_t first = allocateThreadID();
    std::uint32_t second = allocateThreadID();
    BOOST_TEST(first % 4 == 0u);
    BOOST_TEST(second == first + 4);
    BOOST_TEST(first >= 0x100u);
}

BOOST_AUTO_TEST_CASE(test_host_thread_is_no_guest_thread) {
    // Boost.Test does not support assertions from other threads, so the thread records results.
    GuestThread *current = reinterpret_cast<GuestThread *>(1);
    std::uint32_t error = 1;
    std::thread host([&] {
        current = GuestThread::current();
        setLastError(5);
        error = lastError();
    });
    host.join();
    BOOST_TEST(current == nullptr);
    BOOST_TEST(error == 0u);
}

BOOST_AUTO_TEST_SUITE_END()
