#pragma once

#include <atomic>
#include <cassert>

#include "atomic.h"
#include "common.h"
#include "types.h"

#if LIN
    #include <linux/futex.h>
    #include <sys/syscall.h>
#endif

namespace Relax {

    inline int32_t GetOSError() {
#if WIN
        return GetLastError();
#else
        return errno;
#endif
    }

#pragma region IntrusiveMutexFreeQueue

    // TODO: win: wait on address
    class alignas(CACHELINE_SIZE) TEvent
    {
    public:

        TEvent()
          : m_sync(0)
#if !LIN
          , m_mutex()
          , m_condvar()
#endif
        { }

        TEvent(const TEvent& other) = delete;
        TEvent(TEvent&& other) noexcept = delete;
        TEvent& operator=(const TEvent& other) = delete;
        TEvent& operator=(TEvent&& other) noexcept = delete;

        inline void reinit();

        // TODO: timed
        inline void set();

        inline void wait();

    private:

        int32_t m_sync;

#if !LIN
        std::mutex m_mutex;

        std::condition_variable m_condvar;
#endif
    };

    //---------------------------------------------------------//
    void TEvent::reinit()
    {
#if LIN
        NAtomic::store(&m_sync, 0, std::memory_order_release);
#else
        std::lock_guard<std::mutex> g(m_mutex);
        m_sync = 0;
#endif
    }

    //---------------------------------------------------------//
    void TEvent::set()
    {
#if LIN
        NAtomic::store(&m_sync, 1, std::memory_order_release);
        const auto res = ::syscall(SYS_futex, &m_sync, FUTEX_WAKE_PRIVATE, INT32_MAX, nullptr, nullptr, 0);
        assert((decltype(res))0 <= res);
#else
        std::lock_guard<std::mutex> g(m_mutex);
        m_sync = 1;
        m_condvar.notify_all();
#endif
    }

    //---------------------------------------------------------//
    void TEvent::wait()
    {
#if LIN
        int32_t sync = NAtomic::load(&m_sync, std::memory_order_acquire);
        while (0 == sync) {
            const auto res = ::syscall(SYS_futex, &m_sync, FUTEX_WAIT_PRIVATE, 0, nullptr, nullptr, 0);
            assert(res != -1 || EINVAL != GetOSError());
            assert(res != -1 || ENOSYS != GetOSError());
            // EAGAIN/EWOULDBLOCK/EINTR - ok
            sync = NAtomic::load(&m_sync, std::memory_order_acquire);
        }

#else
        std::unique_lock<std::mutex> g(m_mutex);
        m_condvar.wait(g, [&] {return m_sync == 1;});
#endif
    };
}
