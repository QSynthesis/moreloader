#include "KernelObjects.h"

#include <chrono>
#include <condition_variable>

namespace more::loader {

    namespace {

        std::condition_variable &waitCondition() {
            static std::condition_variable condition;
            return condition;
        }

    }

    KernelObject::~KernelObject() = default;

    void WaitableObject::acquire() {
    }

    std::mutex &waitMutex() {
        static std::mutex mutex;
        return mutex;
    }

    void notifyWaiters() {
        waitCondition().notify_all();
    }

    std::uint32_t waitForObjects(const std::vector<std::shared_ptr<WaitableObject>> &objects,
                                 bool waitAll, std::uint32_t timeout) {
        std::unique_lock<std::mutex> lock(waitMutex());

        auto satisfied = [&]() -> std::optional<std::uint32_t> {
            if (waitAll) {
                for (auto &object : objects) {
                    if (!object->isSignaled()) {
                        return std::nullopt;
                    }
                }
                for (auto &object : objects) {
                    object->acquire();
                }
                return WaitObject0;
            }
            for (std::size_t i = 0; i < objects.size(); ++i) {
                if (objects[i]->isSignaled()) {
                    objects[i]->acquire();
                    return WaitObject0 + std::uint32_t(i);
                }
            }
            return std::nullopt;
        };

        if (timeout == infiniteTimeout) {
            for (;;) {
                if (auto result = satisfied()) {
                    return *result;
                }
                waitCondition().wait(lock);
            }
        }

        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout);
        for (;;) {
            if (auto result = satisfied()) {
                // Acquiring the signal changes the state of an auto-reset object, which other
                // waiters do not need to reevaluate, because it only became less signaled.
                return *result;
            }
            if (waitCondition().wait_until(lock, deadline) == std::cv_status::timeout) {
                if (auto result = satisfied()) {
                    return *result;
                }
                return WaitTimeout;
            }
        }
    }

    // ---------------------------------------------------------------------------------------
    // EventObject
    // ---------------------------------------------------------------------------------------

    EventObject::EventObject(bool manualReset, bool signaled)
        : m_manualReset(manualReset), m_signaled(signaled) {
    }

    KernelObject::Type EventObject::type() const {
        return Type::Event;
    }

    bool EventObject::isSignaled() const {
        return m_signaled;
    }

    void EventObject::acquire() {
        if (!m_manualReset) {
            m_signaled = false;
        }
    }

    void EventObject::set() {
        {
            std::lock_guard<std::mutex> lock(waitMutex());
            m_signaled = true;
        }
        notifyWaiters();
    }

    void EventObject::reset() {
        std::lock_guard<std::mutex> lock(waitMutex());
        m_signaled = false;
    }

    // ---------------------------------------------------------------------------------------
    // SemaphoreObject
    // ---------------------------------------------------------------------------------------

    SemaphoreObject::SemaphoreObject(std::int32_t initial, std::int32_t maximum)
        : m_count(initial), m_maximum(maximum) {
    }

    KernelObject::Type SemaphoreObject::type() const {
        return Type::Semaphore;
    }

    bool SemaphoreObject::isSignaled() const {
        return m_count > 0;
    }

    void SemaphoreObject::acquire() {
        --m_count;
    }

    std::optional<std::int32_t> SemaphoreObject::release(std::int32_t count) {
        std::int32_t previous;
        {
            std::lock_guard<std::mutex> lock(waitMutex());
            if (count <= 0 || m_count > m_maximum - count) {
                return std::nullopt;
            }
            previous = m_count;
            m_count += count;
        }
        notifyWaiters();
        return previous;
    }

    // ---------------------------------------------------------------------------------------
    // ThreadObject
    // ---------------------------------------------------------------------------------------

    ThreadObject::ThreadObject(std::uint32_t threadID, std::uint32_t suspendCount)
        : m_threadID(threadID), m_suspendCount(suspendCount) {
    }

    KernelObject::Type ThreadObject::type() const {
        return Type::Thread;
    }

    bool ThreadObject::isSignaled() const {
        return m_exited;
    }

    void ThreadObject::markExited(std::uint32_t exitCode) {
        {
            std::lock_guard<std::mutex> lock(waitMutex());
            m_exited = true;
            m_exitCode = exitCode;
        }
        notifyWaiters();
    }

    std::uint32_t ThreadObject::resume() {
        std::uint32_t previous;
        {
            std::lock_guard<std::mutex> lock(waitMutex());
            previous = m_suspendCount;
            if (m_suspendCount > 0) {
                --m_suspendCount;
            }
        }
        notifyWaiters();
        return previous;
    }

    void ThreadObject::waitUntilResumed() {
        std::unique_lock<std::mutex> lock(waitMutex());
        while (m_suspendCount > 0) {
            waitCondition().wait(lock);
        }
    }

    // ---------------------------------------------------------------------------------------
    // FileObject, FindObject
    // ---------------------------------------------------------------------------------------

    FileObject::FileObject(int descriptor) : m_descriptor(descriptor) {
    }

    KernelObject::Type FileObject::type() const {
        return Type::File;
    }

    FindObject::FindObject(std::vector<Entry> entries) : m_entries(std::move(entries)) {
    }

    KernelObject::Type FindObject::type() const {
        return Type::Find;
    }

    std::optional<FindObject::Entry> FindObject::next() {
        if (m_next >= m_entries.size()) {
            return std::nullopt;
        }
        return m_entries[m_next++];
    }

    // ---------------------------------------------------------------------------------------
    // HandleTable
    // ---------------------------------------------------------------------------------------

    std::uint32_t HandleTable::insert(std::shared_ptr<KernelObject> object) {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::uint32_t handle = m_next;
        m_next += 4;
        m_objects.emplace(handle, std::move(object));
        return handle;
    }

    std::shared_ptr<KernelObject> HandleTable::get(std::uint32_t handle) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_objects.find(handle);
        return it == m_objects.end() ? nullptr : it->second;
    }

    bool HandleTable::close(std::uint32_t handle) {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_objects.erase(handle) != 0;
    }

    HandleTable &handleTable() {
        static HandleTable table;
        return table;
    }

}
