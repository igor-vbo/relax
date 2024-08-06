#pragma once

#include <atomic>

#include "sync/atomic.h"
#include "common.h"
#include "types.h"

namespace Relax {

#pragma region IntrusiveMutexFreeQueue

    //////////////////////////////////////////////////////////////////
    template<Chainable V>
    class IntrusiveMutexFreeQueue {
    public:
        typedef V* pointer_type;
        typedef V value_type;
        typedef V& reference;
        typedef const V& const_reference;
        typedef size_t size_type;

    public:
        IntrusiveMutexFreeQueue();

        IntrusiveMutexFreeQueue(const IntrusiveMutexFreeQueue& other) = delete;
        IntrusiveMutexFreeQueue(IntrusiveMutexFreeQueue&& other) noexcept = delete;
        IntrusiveMutexFreeQueue& operator=(const IntrusiveMutexFreeQueue& other) = delete;
        IntrusiveMutexFreeQueue& operator=(IntrusiveMutexFreeQueue&& other) noexcept = delete;

        bool empty() const noexcept;

        size_type size() const noexcept;

        void push(pointer_type ptr) noexcept;

        pointer_type pop() noexcept;

        bool pop(pointer_type& ptr) noexcept;

        // TODO: void swap(IntrusiveMutexFreeQueue& queue) noexcept;

        void clear() noexcept;

    private:
        inline void setPopPauseCnt(uint32_t old, uint32_t actual);

        inline bool markHead(pointer_type& head);

        static inline bool isMarked(pointer_type ptr);

        static inline pointer_type marked(pointer_type ptr);

    private:
        alignas(CACHELINE_SIZE) std::atomic<V*> m_head;

        alignas(CACHELINE_SIZE) std::atomic<V*> m_tail;

        alignas(CACHELINE_SIZE) std::atomic<uint32_t> m_pop_pause_cnt = 1;

        // TODO:
        alignas(CACHELINE_SIZE) std::atomic<size_t> m_size;

#if RELAX_MF_QUEUE_VERIFICATION
        std::atomic<size_t> m_pop_cnt = 0;
#endif
    };

    //--------------------------------------------------------------//
    template<Chainable V>
    void IntrusiveMutexFreeQueue<V>::setPopPauseCnt(uint32_t old, uint32_t actual) {
        if (actual <= old) {
            m_pop_pause_cnt.store(std::max(1u, old - 1), std::memory_order_relaxed);
        }
        else {
            m_pop_pause_cnt.store(actual, std::memory_order_relaxed);
        }
    }

    //-----------------------------------------------------------------------//
    template<Chainable V>
    bool IntrusiveMutexFreeQueue<V>::markHead(pointer_type& head) {
        return m_head.compare_exchange_strong(head,
                                              marked(head),
                                              std::memory_order_relaxed,
                                              std::memory_order_relaxed);
    }

    //-----------------------------------------------------------------------//
    template<Chainable V>
    bool IntrusiveMutexFreeQueue<V>::isMarked(pointer_type ptr) {
        return reinterpret_cast<uintptr_t>(ptr) & 0b1;
    }

    //-----------------------------------------------------------------------//
    template<Chainable V>
    typename IntrusiveMutexFreeQueue<V>::pointer_type IntrusiveMutexFreeQueue<V>::marked(pointer_type ptr) {
        return reinterpret_cast<pointer_type>(reinterpret_cast<uintptr_t>(ptr) | 0b1);
    }

    //--------------------------------------------------------------//
    template<Chainable V>
    IntrusiveMutexFreeQueue<V>::IntrusiveMutexFreeQueue()
      : m_head(nullptr)
      , m_tail(nullptr)
      , m_size(0) { }

    //--------------------------------------------------------------//
    template<Chainable V>
    inline bool IntrusiveMutexFreeQueue<V>::empty() const noexcept {
        return 0 == m_size.load(std::memory_order_relaxed);
    }

    //--------------------------------------------------------------//
    template<Chainable V>
    inline typename IntrusiveMutexFreeQueue<V>::size_type IntrusiveMutexFreeQueue<V>::size() const noexcept {
        return m_size.load(std::memory_order_relaxed);
    }

    //--------------------------------------------------------------//
    template<Chainable V>
    inline void IntrusiveMutexFreeQueue<V>::push(pointer_type ptr) noexcept {
        if (nullptr == ptr)
            return;

        NAtomic::store(&ptr->m_next, (pointer_type)nullptr, std::memory_order_release);

        pointer_type tail = m_tail.exchange(ptr, std::memory_order_acq_rel);

        if (nullptr == tail)
            m_head.store(ptr, std::memory_order_relaxed);
        else {
            NAtomic::store(&tail->m_next, ptr, std::memory_order_release);
        }

        m_size.fetch_add(1, std::memory_order_relaxed);
    }

    //--------------------------------------------------------------//
    template<Chainable V>
    inline typename IntrusiveMutexFreeQueue<V>::pointer_type IntrusiveMutexFreeQueue<V>::pop() noexcept {
        uint32_t pop_pause_cnt = 0;
        const uint32_t old_pop_pause_cnt = m_pop_pause_cnt.load(std::memory_order_relaxed);
        pointer_type head = m_head.load(std::memory_order_relaxed);

        while ((nullptr != head) && (isMarked(head) || (!markHead(head)))) {
            if (0 == pop_pause_cnt) {
                pop_pause_cnt = old_pop_pause_cnt;
                for (uint32_t i = 0; i < old_pop_pause_cnt; ++i)
                    CPU_PAUSE();
            }
            else {
                CPU_PAUSE();
                ++pop_pause_cnt;
            }

            head = m_head.load(std::memory_order_relaxed);
        }

        if (nullptr == head) {
            setPopPauseCnt(old_pop_pause_cnt, pop_pause_cnt);
            return nullptr;
        }

        // m_head is marked (locked)
        pointer_type next = NAtomic::load(&head->m_next, std::memory_order_acquire);

#if RELAX_MF_QUEUE_VERIFICATION
        head->m_value.m_pop_cnt = m_pop_cnt.fetch_add(1, std::memory_order_relaxed);
#endif

        if (nullptr == next) {
            pointer_type tail = head;
            if (!(m_tail.compare_exchange_strong(tail, nullptr, std::memory_order_relaxed))) {
                // already inserted by other thread
                do {
                    CPU_PAUSE();
                    next = NAtomic::load(&head->m_next, std::memory_order_acquire);
                } while (nullptr == next);

                m_head.store(next, std::memory_order_relaxed);
            }
            else {
                pointer_type marked_head = marked(head);
                m_head.compare_exchange_strong(marked_head, nullptr, std::memory_order_relaxed);
            }
        }
        else {
            m_head.store(next, std::memory_order_relaxed);
        }

        setPopPauseCnt(old_pop_pause_cnt, pop_pause_cnt);

        m_size.fetch_sub(1, std::memory_order_relaxed);

        return head;
    }

    //--------------------------------------------------------------//
    template<Chainable V>
    inline bool IntrusiveMutexFreeQueue<V>::pop(pointer_type& ptr) noexcept {
        ptr = pop();
        return (nullptr != ptr);
    }

    //--------------------------------------------------------------//
    template<Chainable V>
    inline void IntrusiveMutexFreeQueue<V>::clear() noexcept {
        while (pop())
            ;
    }

    //--------------------------------------------------------------//

#pragma endregion IntrusiveMutexFreeQueue

    //////////////////////////////////////////////////////////////////

}  // namespace Relax
