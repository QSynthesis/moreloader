#ifndef MORELOADER_RUNTIME_KERNELOBJECTS_H
#define MORELOADER_RUNTIME_KERNELOBJECTS_H

#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace more::loader {

    /// Pseudo handles of the current process and thread, as returned by \c GetCurrentProcess and
    /// \c GetCurrentThread.
    constexpr std::uint32_t currentProcessHandle = 0xFFFFFFFF;
    constexpr std::uint32_t currentThreadHandle = 0xFFFFFFFE;

    /// Results of the wait functions.
    enum WaitResult : std::uint32_t {
        WaitObject0 = 0,
        WaitTimeout = 0x102,
        WaitFailed = 0xFFFFFFFF,
    };

    /// Timeout value that never expires.
    constexpr std::uint32_t infiniteTimeout = 0xFFFFFFFF;

    /// An object that a handle refers to.
    class KernelObject {
    public:
        enum class Type {
            Event,
            Semaphore,
            Thread,
            File,
            Find,
        };

        virtual ~KernelObject();

        virtual Type type() const = 0;
    };

    /// An object that can be waited for.
    ///
    /// The state of every waitable object is guarded by one process-wide mutex, returned by
    /// waitMutex(). A change of state is made with that mutex held and followed by
    /// notifyWaiters(). This makes a wait for several objects at once atomic.
    class WaitableObject : public KernelObject {
    public:
        /// Returns whether a wait would be satisfied now. Called with the wait mutex held.
        virtual bool isSignaled() const = 0;

        /// Takes the signal after a satisfied wait, as an auto-reset event and a semaphore
        /// require. Called with the wait mutex held.
        virtual void acquire();
    };

    /// The mutex that guards the state of all waitable objects.
    std::mutex &waitMutex();

    /// Wakes all waiting threads so that they reevaluate their objects. Called after a change
    /// of state, with or without the wait mutex held.
    void notifyWaiters();

    /// Waits until one or, if \a waitAll, all of \a objects are signaled, or \a timeout
    /// milliseconds have passed.
    ///
    /// \return \c WaitObject0 plus the index of the signaled object, which is the lowest index
    ///         among several, \c WaitObject0 if all are signaled and \a waitAll, or
    ///         \c WaitTimeout
    std::uint32_t waitForObjects(const std::vector<std::shared_ptr<WaitableObject>> &objects,
                                 bool waitAll, std::uint32_t timeout);

    class EventObject : public WaitableObject {
    public:
        EventObject(bool manualReset, bool signaled);

        Type type() const override;
        bool isSignaled() const override;
        void acquire() override;

        void set();
        void reset();

    private:
        bool m_manualReset;
        bool m_signaled;
    };

    class SemaphoreObject : public WaitableObject {
    public:
        SemaphoreObject(std::int32_t initial, std::int32_t maximum);

        Type type() const override;
        bool isSignaled() const override;
        void acquire() override;

        /// Adds \a count to the count of the semaphore.
        ///
        /// \return the previous count, or \c std::nullopt if the count would exceed the maximum
        std::optional<std::int32_t> release(std::int32_t count);

    private:
        std::int32_t m_count;
        std::int32_t m_maximum;
    };

    /// A thread of the guest. The object is signaled when the thread has exited.
    class ThreadObject : public WaitableObject {
    public:
        ThreadObject(std::uint32_t threadID, std::uint32_t suspendCount);

        Type type() const override;
        bool isSignaled() const override;

        inline std::uint32_t threadID() const {
            return m_threadID;
        }

        /// Records the exit of the thread and wakes its waiters.
        void markExited(std::uint32_t exitCode);

        /// Decrements the suspend count, as \c ResumeThread does.
        ///
        /// \return the previous suspend count
        std::uint32_t resume();

        /// Blocks the calling thread, which is the thread itself, until its suspend count is
        /// zero.
        void waitUntilResumed();

    private:
        std::uint32_t m_threadID;
        std::uint32_t m_suspendCount;
        bool m_exited = false;
        std::uint32_t m_exitCode = 0;
    };

    /// A file descriptor of the host, as \c _get_osfhandle returns it for \c LockFileEx.
    class FileObject : public KernelObject {
    public:
        explicit FileObject(int descriptor);

        Type type() const override;

        ~FileObject();

        inline int descriptor() const {
            return m_descriptor;
        }

        /// Returns a descriptor of the same file for byte-range locks.
        ///
        /// Windows grants shared and exclusive locks on a handle regardless of its access, while
        /// \c fcntl requires a readable descriptor for a shared lock and a writable one for an
        /// exclusive lock. moresampler locks a file that it opened for writing only. The
        /// descriptor is therefore a separate descriptor opened for reading and writing on first
        /// use, or the original descriptor if the file cannot be opened so. Locks of an open file
        /// description conflict with those of other descriptions as Windows locks conflict with
        /// those of other handles.
        int lockDescriptor();

    private:
        int m_descriptor;
        int m_lockDescriptor = -1;
        std::mutex m_mutex;
    };

    /// The state of a directory enumeration of \c FindFirstFileW.
    class FindObject : public KernelObject {
    public:
        struct Entry {
            std::string hostPath;
            std::u16string name;
        };

        explicit FindObject(std::vector<Entry> entries);

        Type type() const override;

        /// Returns the next entry, or \c std::nullopt at the end.
        std::optional<Entry> next();

    private:
        std::vector<Entry> m_entries;
        std::size_t m_next = 0;
    };

    /// The handles of the process.
    ///
    /// Handle values are multiples of 4 from 0x100 and are not reused, so that a handle that
    /// was closed never refers to a newer object.
    class HandleTable {
    public:
        std::uint32_t insert(std::shared_ptr<KernelObject> object);

        /// Returns the object of \a handle, or \c nullptr if the handle is not open.
        std::shared_ptr<KernelObject> get(std::uint32_t handle) const;

        /// Returns the object of \a handle if it has the type \a T.
        template <class T>
        std::shared_ptr<T> get(std::uint32_t handle) const {
            return std::dynamic_pointer_cast<T>(get(handle));
        }

        /// Closes \a handle.
        ///
        /// \return whether the handle was open
        bool close(std::uint32_t handle);

    private:
        mutable std::mutex m_mutex;
        std::map<std::uint32_t, std::shared_ptr<KernelObject>> m_objects;
        std::uint32_t m_next = 0x100;
    };

    /// The handle table of the process.
    HandleTable &handleTable();

}

#endif // MORELOADER_RUNTIME_KERNELOBJECTS_H
