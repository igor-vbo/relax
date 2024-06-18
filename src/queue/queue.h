#pragma once

#include "intrusive_queue.h"

namespace Relax {

#pragma region MutexFreeQueue

    //////////////////////////////////////////////////////////////////
    // TODO: Alloc
    template<class V>
    class MutexFreeQueue {
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
        typedef typename IntrusiveMutexFreeQueue<V>::size_type size_type;

    public:
        MutexFreeQueue();
        ~MutexFreeQueue();

        MutexFreeQueue(const MutexFreeQueue& other) = delete;
        MutexFreeQueue(MutexFreeQueue&& other) noexcept = delete;
        MutexFreeQueue& operator=(const MutexFreeQueue& other) = delete;
        MutexFreeQueue& operator=(MutexFreeQueue&& other) noexcept = delete;

        bool empty() const noexcept;

        size_type size() const noexcept;

        void push(const value_type& value);

        void push(value_type&& value);

        template<typename... Args>
        void emplace(Args&&... args);

        bool pop(value_type& ref);

        // TODO: void swap(MutexFreeQueue& queue) noexcept;

        void clear();

    private:
        IntrusiveMutexFreeQueue<Node> m_queue;
    };

    //--------------------------------------------------------------//
    template<class V>
    MutexFreeQueue<V>::MutexFreeQueue()
      : m_queue() { }

    //--------------------------------------------------------------//
    template<class V>
    MutexFreeQueue<V>::~MutexFreeQueue() {
        clear();
    }

    //--------------------------------------------------------------//
    template<class V>
    inline bool MutexFreeQueue<V>::empty() const noexcept {
        return m_queue.empty();
    }

    //--------------------------------------------------------------//
    template<class V>
    inline typename MutexFreeQueue<V>::size_type MutexFreeQueue<V>::size() const noexcept {
        return m_queue.size();
    }

    //--------------------------------------------------------------//
    template<class V>
    inline void MutexFreeQueue<V>::push(const value_type& value) {
        Node* node = new Node(value);
        return m_queue.push(node);
    }

    //--------------------------------------------------------------//
    template<class V>
    inline void MutexFreeQueue<V>::push(value_type&& value) {
        return emplace(std::move(value));
    }

    //--------------------------------------------------------------//
    template<class V>
    template<typename... Args>
    inline void MutexFreeQueue<V>::emplace(Args&&... args) {
        Node* node = new Node(std::forward<Args>(args)...);
        return m_queue.push(node);
    }

    //--------------------------------------------------------------//
    template<class V>
    bool MutexFreeQueue<V>::pop(value_type& ref) {
        Node* node = m_queue.pop();
        if (nullptr == node)
            return false;

        try {
            new (&ref) value_type(std::move(node->m_value));
        }
        catch (...) {
            m_queue.push(node);
            throw;
        }

        delete node;
        return true;
    }

    //--------------------------------------------------------------//
    template<class V>
    void MutexFreeQueue<V>::clear() {
        Node* val = m_queue.pop();
        while (val) {
            delete val;
            val = m_queue.pop();
        }
    }

    //--------------------------------------------------------------//
    template<class V>
    MutexFreeQueue<V>::Node::Node(const V& value)
      : m_next(nullptr)
      , m_value(value) { }

    //--------------------------------------------------------------//
    template<class V>
    template<typename... Args>
    MutexFreeQueue<V>::Node::Node(Args&&... args)
      : m_next(nullptr)
      , m_value(std::forward<Args>(args)...) { }

    //////////////////////////////////////////////////////////////////

    //////////////////////////////////////////////////////////////////

#pragma endregion MutexFreeQueue

}  // namespace Relax
