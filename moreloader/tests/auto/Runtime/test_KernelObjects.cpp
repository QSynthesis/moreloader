#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <chrono>
#include <cstdlib>
#include <memory>
#include <thread>
#include <vector>

#include <boost/test/unit_test.hpp>

#include <moreloader/Runtime/KernelObjects.h>

using namespace more::loader;

namespace {

    using Objects = std::vector<std::shared_ptr<WaitableObject>>;

    std::uint32_t poll(const Objects &objects, bool waitAll = false) {
        return waitForObjects(objects, waitAll, 0);
    }

}

BOOST_AUTO_TEST_SUITE(test_KernelObjects)

BOOST_AUTO_TEST_CASE(test_constants) {
    BOOST_TEST(currentProcessHandle == 0xFFFFFFFFu);
    BOOST_TEST(currentThreadHandle == 0xFFFFFFFEu);
    BOOST_TEST(WaitObject0 == 0u);
    BOOST_TEST(WaitTimeout == 0x102u);
    BOOST_TEST(WaitFailed == 0xFFFFFFFFu);
    BOOST_TEST(infiniteTimeout == 0xFFFFFFFFu);
}

BOOST_AUTO_TEST_CASE(test_auto_reset_event) {
    auto event = std::make_shared<EventObject>(false, false);
    BOOST_TEST((event->type() == KernelObject::Type::Event));
    BOOST_TEST(poll({event}) == WaitTimeout);
    event->set();
    BOOST_TEST(poll({event}) == WaitObject0);
    // A satisfied wait resets the event.
    BOOST_TEST(poll({event}) == WaitTimeout);
    event->set();
    event->reset();
    BOOST_TEST(poll({event}) == WaitTimeout);
}

BOOST_AUTO_TEST_CASE(test_manual_reset_event) {
    auto event = std::make_shared<EventObject>(true, true);
    BOOST_TEST(poll({event}) == WaitObject0);
    BOOST_TEST(poll({event}) == WaitObject0);
    event->reset();
    BOOST_TEST(poll({event}) == WaitTimeout);
}

BOOST_AUTO_TEST_CASE(test_semaphore) {
    auto semaphore = std::make_shared<SemaphoreObject>(1, 3);
    BOOST_TEST((semaphore->type() == KernelObject::Type::Semaphore));
    BOOST_TEST(poll({semaphore}) == WaitObject0);
    BOOST_TEST(poll({semaphore}) == WaitTimeout);

    BOOST_TEST(semaphore->release(2).value_or(-1) == 0);
    // The count would exceed the maximum of 3.
    BOOST_TEST(!semaphore->release(2).has_value());
    BOOST_TEST(semaphore->release(1).value_or(-1) == 2);
    BOOST_TEST(!semaphore->release(0).has_value());
    BOOST_TEST(!semaphore->release(-1).has_value());
    for (int i = 0; i < 3; ++i) {
        BOOST_TEST(poll({semaphore}) == WaitObject0);
    }
    BOOST_TEST(poll({semaphore}) == WaitTimeout);
}

BOOST_AUTO_TEST_CASE(test_wait_for_any_returns_the_lowest_index) {
    auto first = std::make_shared<EventObject>(false, false);
    auto second = std::make_shared<EventObject>(false, true);
    auto third = std::make_shared<EventObject>(false, true);
    BOOST_TEST(poll({first, second, third}) == WaitObject0 + 1);
    // Only the returned object was acquired.
    BOOST_TEST(poll({first, second, third}) == WaitObject0 + 2);
    BOOST_TEST(poll({first, second, third}) == WaitTimeout);
}

BOOST_AUTO_TEST_CASE(test_wait_for_all_is_atomic) {
    auto first = std::make_shared<EventObject>(false, true);
    auto second = std::make_shared<SemaphoreObject>(0, 1);
    BOOST_TEST(poll({first, second}, true) == WaitTimeout);
    // A failed wait for all acquires nothing.
    BOOST_TEST(first->isSignaled());
    second->release(1);
    BOOST_TEST(poll({first, second}, true) == WaitObject0);
    BOOST_TEST(!first->isSignaled());
    BOOST_TEST(!second->isSignaled());
}

BOOST_AUTO_TEST_CASE(test_timeout_and_wakeup) {
    auto event = std::make_shared<EventObject>(false, false);
    auto start = std::chrono::steady_clock::now();
    BOOST_TEST(waitForObjects({event}, false, 50) == WaitTimeout);
    bool waitedLongEnough =
        std::chrono::steady_clock::now() - start >= std::chrono::milliseconds(50);
    BOOST_TEST(waitedLongEnough);

    std::thread setter([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        event->set();
    });
    BOOST_TEST(waitForObjects({event}, false, infiniteTimeout) == WaitObject0);
    setter.join();
}

BOOST_AUTO_TEST_CASE(test_thread_object) {
    auto thread = std::make_shared<ThreadObject>(0x104, 2);
    BOOST_TEST((thread->type() == KernelObject::Type::Thread));
    BOOST_TEST(thread->threadID() == 0x104u);
    BOOST_TEST(poll({thread}) == WaitTimeout);

    bool resumed = false;
    std::thread waiter([&] {
        thread->waitUntilResumed();
        resumed = true;
    });
    BOOST_TEST(thread->resume() == 2u);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    BOOST_TEST(!resumed);
    BOOST_TEST(thread->resume() == 1u);
    waiter.join();
    BOOST_TEST(resumed);
    // Resuming a running thread leaves the count at zero.
    BOOST_TEST(thread->resume() == 0u);
    BOOST_TEST(thread->resume() == 0u);

    thread->markExited(3);
    // The exit signals the object permanently.
    BOOST_TEST(poll({thread}) == WaitObject0);
    BOOST_TEST(poll({thread}) == WaitObject0);
}

BOOST_AUTO_TEST_CASE(test_file_object) {
    char path[] = "/tmp/moreloader-file-XXXXXX";
    int descriptor = ::mkstemp(path);
    BOOST_TEST_REQUIRE(descriptor >= 0);
    int writeOnly = ::open(path, O_WRONLY);
    BOOST_TEST_REQUIRE(writeOnly >= 0);
    {
        FileObject file(writeOnly, true);
        BOOST_TEST((file.type() == KernelObject::Type::File));
        BOOST_TEST(file.descriptor() == writeOnly);
        // A shared lock requires a readable descriptor, which the object opens separately.
        int lock = file.lockDescriptor();
        BOOST_TEST(lock != writeOnly);
        BOOST_TEST((::fcntl(lock, F_GETFL) & O_ACCMODE) == O_RDWR);
        BOOST_TEST(file.lockDescriptor() == lock);
        struct flock range{};
        range.l_type = F_RDLCK;
        range.l_whence = SEEK_SET;
        range.l_len = 1;
        BOOST_TEST(::fcntl(lock, F_SETLK, &range) == 0);
    }
    // An owned descriptor is closed with the object.
    BOOST_TEST(::fcntl(writeOnly, F_GETFD) == -1);
    {
        FileObject file(descriptor);
    }
    BOOST_TEST(::fcntl(descriptor, F_GETFD) != -1);
    ::close(descriptor);
    ::unlink(path);
}

BOOST_AUTO_TEST_CASE(test_find_object) {
    FindObject find({
        {"/a/x.wav", u"x.wav"},
        {"/a/y.wav", u"y.wav"}
    });
    BOOST_TEST((find.type() == KernelObject::Type::Find));
    auto first = find.next();
    BOOST_TEST_REQUIRE(first.has_value());
    BOOST_TEST(first->hostPath == "/a/x.wav");
    BOOST_TEST((first->name == u"x.wav"));
    BOOST_TEST(find.next()->hostPath == "/a/y.wav");
    BOOST_TEST(!find.next().has_value());
    BOOST_TEST(!find.next().has_value());
}

BOOST_AUTO_TEST_CASE(test_handle_table) {
    HandleTable table;
    auto event = std::make_shared<EventObject>(false, false);
    std::uint32_t first = table.insert(event);
    std::uint32_t second = table.insert(std::make_shared<SemaphoreObject>(0, 1));
    BOOST_TEST(first == 0x100u);
    BOOST_TEST(second == 0x104u);
    BOOST_TEST(table.get(first) == event);
    BOOST_TEST(table.get<EventObject>(first) == event);
    BOOST_TEST(!table.get<SemaphoreObject>(first));
    BOOST_TEST(table.get<SemaphoreObject>(second) != nullptr);
    BOOST_TEST(!table.get(0x108));

    BOOST_TEST(table.close(first));
    BOOST_TEST(!table.close(first));
    BOOST_TEST(!table.get(first));
    // Handle values are not reused.
    BOOST_TEST(table.insert(event) == 0x108u);
}

BOOST_AUTO_TEST_CASE(test_process_handle_table_is_one_instance) {
    BOOST_TEST(&handleTable() == &handleTable());
}

BOOST_AUTO_TEST_SUITE_END()
