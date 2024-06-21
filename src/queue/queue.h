#pragma once

#include "intrusive_queue.h"

namespace Relax {

#pragma region MutexFreeQueue

    //////////////////////////////////////////////////////////////////
    // TODO: Alloc
    template<class T>
    class MutexFreeQueue {
        struct Node : TChainableBase<Node> {
            Node() = delete;

            Node(const T& value);

            template<typename... Args>
            Node(Args&&... args);

        public:
            T m_value;
        };

    public:
        typedef T value_type;
        typedef T& reference;
        typedef const T& const_reference;
        typedef typename IntrusiveMutexFreeQueue<Node>::size_type size_type;

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
    template<class T>
    MutexFreeQueue<T>::MutexFreeQueue()
      : m_queue() { }

    //--------------------------------------------------------------//
    template<class T>
    MutexFreeQueue<T>::~MutexFreeQueue() {
        clear();
    }

    //--------------------------------------------------------------//
    template<class T>
    inline bool MutexFreeQueue<T>::empty() const noexcept {
        return m_queue.empty();
    }

    //--------------------------------------------------------------//
    template<class T>
    inline typename MutexFreeQueue<T>::size_type MutexFreeQueue<T>::size() const noexcept {
        return m_queue.size();
    }

    //--------------------------------------------------------------//
    template<class T>
    inline void MutexFreeQueue<T>::push(const value_type& value) {
        Node* node = new Node(value);
        return m_queue.push(node);
    }

    //--------------------------------------------------------------//
    template<class T>
    inline void MutexFreeQueue<T>::push(value_type&& value) {
        return emplace(std::move(value));
    }

    //--------------------------------------------------------------//
    template<class T>
    template<typename... Args>
    inline void MutexFreeQueue<T>::emplace(Args&&... args) {
        Node* node = new Node(std::forward<Args>(args)...);
        return m_queue.push(node);
    }

    //--------------------------------------------------------------//
    template<class T>
    bool MutexFreeQueue<T>::pop(value_type& ref) {
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
    template<class T>
    void MutexFreeQueue<T>::clear() {
        Node* val = m_queue.pop();
        while (val) {
            delete val;
            val = m_queue.pop();
        }
    }

    //--------------------------------------------------------------//
    template<class T>
    MutexFreeQueue<T>::Node::Node(const T& value)
      : m_value(value) { }

    //--------------------------------------------------------------//
    template<class T>
    template<typename... Args>
    MutexFreeQueue<T>::Node::Node(Args&&... args)
      : m_value(std::forward<Args>(args)...) { }

    //////////////////////////////////////////////////////////////////

    //////////////////////////////////////////////////////////////////

#pragma endregion MutexFreeQueue

}  // namespace Relax
