#pragma once

#include <immintrin.h>

#include <algorithm>
#include <atomic>

#include "common.h"

namespace Relax {

#pragma region IntrusiveMutexFreeStack

    //////////////////////////////////////////////////////////////////
    /*
     *
     */
    template<Chainable V>
    class IntrusiveMutexFreeStack {
    public:
        typedef V* pointer_type;
        typedef V value_type;
        typedef V& reference;
        typedef const V& const_reference;
        typedef size_t size_type;

    public:
        IntrusiveMutexFreeStack();

        IntrusiveMutexFreeStack(const IntrusiveMutexFreeStack& other) = delete;
        IntrusiveMutexFreeStack(IntrusiveMutexFreeStack&& other) noexcept = delete;
        IntrusiveMutexFreeStack& operator=(const IntrusiveMutexFreeStack& other) = delete;
        IntrusiveMutexFreeStack& operator=(IntrusiveMutexFreeStack&& other) noexcept = delete;

        bool empty() const noexcept;

        size_type size() const noexcept;

        void push(pointer_type ptr) noexcept;

        pointer_type pop() noexcept;

        // TODO: void swap(IntrusiveMutexFreeStack& stack) noexcept;

    private:
        inline void setPopPauseCnt(uint32_t old, uint32_t actual);

        inline bool markHead(pointer_type& head);

        static inline bool isMarked(pointer_type ptr);

        static inline pointer_type marked(pointer_type ptr);

        pointer_type lockHead() noexcept;

    private:
        alignas(CACHELINE_SIZE) std::atomic<V*> m_head;

        // TODO:
        alignas(CACHELINE_SIZE) std::atomic<size_t> m_size;

        alignas(CACHELINE_SIZE) std::atomic<uint32_t> m_pop_pause_cnt = 1;
    };

    //--------------------------------------------------------------//
    template<Chainable V>
    void IntrusiveMutexFreeStack<T>::setPopPauseCnt(uint32_t old, uint32_t actual) {
        if (actual <= old) {
            m_pop_pause_cnt.store(std::max(1u, old - 1), std::memory_order_relaxed);
        }
        else {
            m_pop_pause_cnt.store(actual, std::memory_order_relaxed);
        }
    }

    //-----------------------------------------------------------------------//
    template<Chainable V>
    bool IntrusiveMutexFreeStack<T>::markHead(pointer_type& head) {
        return m_head.compare_exchange_strong(head,
                                              marked(head),
                                              std::memory_order_acq_rel,
                                              std::memory_order_relaxed);
    }

    //-----------------------------------------------------------------------//
    template<Chainable V>
    bool IntrusiveMutexFreeStack<T>::isMarked(pointer_type ptr) {
        return reinterpret_cast<uintptr_t>(ptr) & 0b1;
    }

    //-----------------------------------------------------------------------//
    template<Chainable V>
    typename IntrusiveMutexFreeStack<T>::pointer_type IntrusiveMutexFreeStack<T>::marked(pointer_type ptr) {
        return reinterpret_cast<pointer_type>(reinterpret_cast<uintptr_t>(ptr) | 0b1);
    }

    //--------------------------------------------------------------//
    template<Chainable V>
    IntrusiveMutexFreeStack<T>::IntrusiveMutexFreeStack()
      : m_head(nullptr)
      , m_size(0) { }

    //--------------------------------------------------------------//
    template<Chainable V>
    inline bool IntrusiveMutexFreeStack<T>::empty() const noexcept {
        return 0 == m_size.load(std::memory_order_relaxed);
    }

    //--------------------------------------------------------------//
    template<Chainable V>
    inline typename IntrusiveMutexFreeStack<T>::size_type IntrusiveMutexFreeStack<T>::size() const noexcept {
        return m_size.load(std::memory_order_relaxed);
    }

    //--------------------------------------------------------------//
    template<Chainable V>
    typename IntrusiveMutexFreeStack<T>::pointer_type IntrusiveMutexFreeStack<T>::lockHead() noexcept {
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

        setPopPauseCnt(old_pop_pause_cnt, pop_pause_cnt);

        return head;
    }

    //--------------------------------------------------------------//
    template<Chainable V>
    inline void IntrusiveMutexFreeStack<T>::push(pointer_type ptr) noexcept {
        if (nullptr == ptr)
            return;

        // __atomic_store_n(&ptr->m_next, nullptr, __ATOMIC_RELEASE);

        pointer_type const head = lockHead();

        __atomic_store_n(&ptr->m_next, head, __ATOMIC_RELAXED);

        m_head.store(ptr, std::memory_order_release);

        // m_size.fetch_add(1, std::memory_order_relaxed);
    }

    //--------------------------------------------------------------//
    template<Chainable V>
    inline typename IntrusiveMutexFreeStack<T>::pointer_type IntrusiveMutexFreeStack<T>::pop() noexcept {
        pointer_type const head = lockHead();

        if (nullptr == head)
            return nullptr;

        pointer_type const next = __atomic_load_n(&head->m_next, __ATOMIC_RELAXED);

        m_head.store(next, std::memory_order_release);

        __atomic_store_n(&head->m_next, nullptr, __ATOMIC_RELAXED);

        return head;
    }

    //--------------------------------------------------------------//

#pragma endregion IntrusiveMutexFreeStack

#pragma region MutexFreeStack

    //////////////////////////////////////////////////////////////////
    // TODO: Alloc
    template<class T>
    class MutexFreeStack {
        struct Node {
            Node() = delete;

            Node(const V& value);

            template<typename... Args>
            Node(Args&&... args);

        public:
            Node* m_next;
            V m_value;
        };

    public:
        typedef V value_type;
        typedef V& reference;
        typedef const V& const_reference;
        typedef typename IntrusiveMutexFreeStack<T>::size_type size_type;

    public:
        MutexFreeStack();
        ~MutexFreeStack();

        MutexFreeStack(const MutexFreeStack& other) = delete;
        MutexFreeStack(MutexFreeStack&& other) noexcept = delete;
        MutexFreeStack& operator=(const MutexFreeStack& other) = delete;
        MutexFreeStack& operator=(MutexFreeStack&& other) noexcept = delete;

        bool empty() const noexcept;

        size_type size() const noexcept;

        void push(const value_type& value);

        void push(value_type&& value);

        template<typename... Args>
        void emplace(Args&&... args);

        bool pop(value_type& ref);

        // TODO: void swap(IntrusiveMutexFreeStack& stack) noexcept;

        void clear();

    private:
        IntrusiveMutexFreeStack<Node> m_stack;
    };

    //--------------------------------------------------------------//
    template<class T>
    MutexFreeStack<T>::MutexFreeStack()
      : m_stack() { }

    //--------------------------------------------------------------//
    template<class T>
    MutexFreeStack<T>::~MutexFreeStack() {
        clear();
    }

    //--------------------------------------------------------------//
    template<class T>
    inline bool MutexFreeStack<T>::empty() const noexcept {
        return m_stack.empty();
    }

    //--------------------------------------------------------------//
    template<class T>
    inline typename MutexFreeStack<T>::size_type MutexFreeStack<T>::size() const noexcept {
        return m_stack.size();
    }

    //--------------------------------------------------------------//
    template<class T>
    inline void MutexFreeStack<T>::push(const value_type& value) {
        Node* node = new Node(value);
        return m_stack.push(node);
    }

    //--------------------------------------------------------------//
    template<class T>
    inline void MutexFreeStack<T>::push(value_type&& value) {
        return emplace(std::move(value));
    }

    //--------------------------------------------------------------//
    template<class T>
    template<typename... Args>
    inline void MutexFreeStack<T>::emplace(Args&&... args) {
        Node* node = new Node(std::forward<Args>(args)...);
        return m_stack.push(node);
    }

    //--------------------------------------------------------------//
    template<class T>
    bool MutexFreeStack<T>::pop(value_type& ref) {
        Node* node = m_stack.pop();
        if (nullptr == node)
            return false;

        try {
            new (&ref) value_type(std::move(node->m_value));
        }
        catch (...) {
            m_stack.push(node);
            throw;
        }

        delete node;
        return true;
    }

    //--------------------------------------------------------------//
    template<class T>
    void MutexFreeStack<T>::clear() {
        Node* val = m_stack.pop();
        while (val) {
            delete val;
            val = m_stack.pop();
        }
    }

    //--------------------------------------------------------------//
    template<class T>
    MutexFreeStack<T>::Node::Node(const V& value)
      : m_next(nullptr)
      , m_value(value) { }

    //--------------------------------------------------------------//
    template<class T>
    template<typename... Args>
    MutexFreeStack<T>::Node::Node(Args&&... args)
      : m_next(nullptr)
      , m_value(std::forward<Args>(args)...) { }

    //////////////////////////////////////////////////////////////////

    //////////////////////////////////////////////////////////////////

#pragma endregion MutexFreeStack

}  // namespace Relax
