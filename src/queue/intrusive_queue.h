#pragma once

#include <atomic>

#include "common.h"
#include "types.h"

namespace Relax {

#pragma region IntrusiveMutexFreeQueue

// TODO: concept
#if 0
    template<typename T>
    concept Nextable = requires(T value) {
        { (value->m_next) } -> T*;
    };
#endif

    //////////////////////////////////////////////////////////////////
    template<class V>
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
    template<class V>
    void IntrusiveMutexFreeQueue<V>::setPopPauseCnt(uint32_t old, uint32_t actual) {
        if (actual <= old) {
            m_pop_pause_cnt.store(std::max(1u, old - 1), std::memory_order_relaxed);
        }
        else {
            m_pop_pause_cnt.store(actual, std::memory_order_relaxed);
        }
    }

    //-----------------------------------------------------------------------//
    template<class T>
    bool IntrusiveMutexFreeQueue<T>::markHead(pointer_type& head) {
        return m_head.compare_exchange_strong(head,
                                              marked(head),
                                              std::memory_order_relaxed,
                                              std::memory_order_relaxed);
    }

    //-----------------------------------------------------------------------//
    template<class T>
    bool IntrusiveMutexFreeQueue<T>::isMarked(pointer_type ptr) {
        return reinterpret_cast<uintptr_t>(ptr) & 0b1;
    }

    //-----------------------------------------------------------------------//
    template<class T>
    typename IntrusiveMutexFreeQueue<T>::pointer_type IntrusiveMutexFreeQueue<T>::marked(pointer_type ptr) {
        return reinterpret_cast<pointer_type>(reinterpret_cast<uintptr_t>(ptr) | 0b1);
    }

    //--------------------------------------------------------------//
    template<class V>
    IntrusiveMutexFreeQueue<V>::IntrusiveMutexFreeQueue()
      : m_head(nullptr)
      , m_tail(nullptr)
      , m_size(0) { }

    //--------------------------------------------------------------//
    template<class V>
    inline bool IntrusiveMutexFreeQueue<V>::empty() const noexcept {
        return 0 == m_size.load(std::memory_order_relaxed);
    }

    //--------------------------------------------------------------//
    template<class V>
    inline typename IntrusiveMutexFreeQueue<V>::size_type IntrusiveMutexFreeQueue<V>::size() const noexcept {
        return m_size.load(std::memory_order_relaxed);
    }

    //--------------------------------------------------------------//
    template<class V>
    inline void IntrusiveMutexFreeQueue<V>::push(pointer_type ptr) noexcept {
        if (nullptr == ptr)
            return;

        __atomic_store_n(&ptr->m_next, nullptr, __ATOMIC_RELEASE);

        pointer_type tail = m_tail.exchange(ptr, std::memory_order_acq_rel);

        if (nullptr == tail)
            m_head.store(ptr, std::memory_order_relaxed);
        else {
            __atomic_store_n(&tail->m_next, ptr, __ATOMIC_RELEASE);
        }

        // m_size.fetch_add(1, std::memory_order_relaxed);
    }

    //--------------------------------------------------------------//
    template<class V>
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
        pointer_type next = __atomic_load_n(&head->m_next, __ATOMIC_ACQUIRE);

#if RELAX_MF_QUEUE_VERIFICATION
        head->m_value.m_pop_cnt = m_pop_cnt.fetch_add(1, std::memory_order_relaxed);
#endif

        if (nullptr == next) {
            pointer_type tail = head;
            if (!(m_tail.compare_exchange_strong(tail, nullptr, std::memory_order_relaxed))) {
                // already inserted by other thread
                do {
                    CPU_PAUSE();
                    next = __atomic_load_n(&head->m_next, __ATOMIC_ACQUIRE);
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

        // m_size.fetch_sub(1, std::memory_order_relaxed);
        setPopPauseCnt(old_pop_pause_cnt, pop_pause_cnt);

        return head;
    }

    //--------------------------------------------------------------//
    template<class V>
    inline bool IntrusiveMutexFreeQueue<V>::pop(pointer_type& ptr) noexcept {
        ptr = pop();
        return (nullptr != ptr);
    }

    //--------------------------------------------------------------//
    template<class V>
    inline void IntrusiveMutexFreeQueue<V>::clear() noexcept {
        while (pop())
            ;
    }

    //--------------------------------------------------------------//

#pragma endregion IntrusiveMutexFreeQueue

    //////////////////////////////////////////////////////////////////

}  // namespace Relax
