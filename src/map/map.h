#pragma once

#include "intrusive_map.h"
#include "types.h"

namespace Relax {
    //////////////////////////////////////////////////////////////////

    struct FakeLock {
        inline void lock() {};
        inline void unlock() {};
    };

    //////////////////////////////////////////////////////////////////

    template<class K, class T, class Lock = FakeLock>
    class Map {
    public:
        struct Node : TIntrusiveMappableBase<K, Node> {
            template<typename... Args>
            Node(K&& key, Args&&... args)
              : TIntrusiveMappableBase<K, Node>(std::forward<K>(key))
              , m_value(std::forward<Args>(args)...) { }

            template<typename... Args>
            Node(const K& key, Args&&... args)
              : Node(K{key}, std::forward<Args>(args)...) { }

            T m_value;
        };

    public:
        typedef K key_type;
        typedef T mapped_type;
        typedef T* pointer_type;
        typedef T& reference;
        typedef const T& const_reference;
        typedef size_t size_type;

    public:
        class iterator;

        Map()
          : m_tree() { }

        ~Map() { clear(); }

        Map(const Map& other) = delete;
        Map(Map&& other) noexcept = delete;
        Map& operator=(const Map& other) = delete;
        Map& operator=(Map&& other) noexcept = delete;

        template<typename... Args>
        std::pair<iterator, bool> emplace(const key_type& key, Args&&... args);

        template<typename... Args>
        std::pair<iterator, bool> emplace(key_type&& key, Args&&... args);

        std::pair<iterator, bool> insert(key_type key, mapped_type value);

        std::pair<iterator, bool> insert(const std::pair<key_type, mapped_type>& value);

        size_type erase(key_type key);

        void clear() noexcept;

        size_type size() const noexcept;

    public:
        class iterator : public std::iterator<std::input_iterator_tag, mapped_type> {
            friend class Map<K, T, Lock>;

            iterator(typename IntrusiveMap<Node>::iterator it)
              : m_it(it) { }

        public:
            iterator(const iterator& it)
              : m_it(it.m_it) { }
            ~iterator() = default;

            iterator& operator=(const iterator& it) {
                m_it = it.m_it;
                return *this;
            }

            std::pair<key_type&, mapped_type&> operator*() const noexcept { return {m_it->m_key, m_it->m_value}; }
            pointer_type operator->() const { return &m_it->m_value; }

            iterator& operator++() {
                ++m_it;
                return *this;
            }
            iterator operator++(int) {
                iterator it(*this);
                ++m_it;
                return it;
            }

            bool operator==(const iterator& other) const { return m_it == other.m_it; }
            bool operator!=(const iterator& other) const { return m_it != other.m_it; }

        private:
            typename IntrusiveMap<Node>::iterator m_it;
        };

        iterator begin() const { return iterator(m_tree.begin()); }
        iterator end() const { return iterator(m_tree.end()); }

    public:
        bool checkRB() { return m_tree.checkRB(); }

    private:
        IntrusiveMap<Node> m_tree;

    private:
        Lock m_lock;
    };

    //--------------------------------------------------------------//
    template<class K, class T, class L>
    template<typename... Args>
    std::pair<typename Map<K, T, L>::iterator, bool> Map<K, T, L>::emplace(const key_type& key, Args&&... args) {
        Map<K, T, L>::Node* node = new Node(key, std::forward<Args>(args)...);

        // no guard
        // for simple remove of fake lock by optimizer
        m_lock.lock();

        const auto res = m_tree.insert(node);

        m_lock.unlock();

        return std::pair<iterator, bool>(iterator(res.first), res.second);
    }

    //--------------------------------------------------------------//
    template<class K, class T, class L>
    template<typename... Args>
    std::pair<typename Map<K, T, L>::iterator, bool> Map<K, T, L>::emplace(key_type&& key, Args&&... args) {
        Map<K, T, L>::Node* node = new Node(std::forward<key_type>(key), std::forward<Args>(args)...);

        // no guard
        // for simple remove of fake lock by optimizer
        m_lock.lock();

        const auto res = m_tree.insert(node);

        m_lock.unlock();

        return std::pair<iterator, bool>(iterator(res.first), res.second);
    }

    //--------------------------------------------------------------//
    template<class K, class T, class L>
    std::pair<typename Map<K, T, L>::iterator, bool> Map<K, T, L>::insert(key_type const key,
                                                                          mapped_type const value) {
        Map<K, T, L>::Node* node = new Node(key, value);

        // no guard
        // for simple remove of fake lock by optimizer
        m_lock.lock();

        const auto res = m_tree.insert(node);

        m_lock.unlock();

        return std::pair<iterator, bool>(iterator(res.first), res.second);
    }

    //--------------------------------------------------------------//
    template<class K, class T, class L>
    std::pair<typename Map<K, T, L>::iterator, bool> Map<K, T, L>::insert(
        const std::pair<key_type, mapped_type>& value) {
        Map<K, T, L>::Node* node = new Node(std::pair<key_type, mapped_type>(value));

        // no guard
        // for simple remove of fake lock by optimizer
        m_lock.lock();

        const auto res = m_tree.insert(node);

        m_lock.unlock();

        return std::pair<iterator, bool>(iterator(res.first), res.second);
    }

    //--------------------------------------------------------------//
    template<class K, class T, class L>
    size_t Map<K, T, L>::erase(key_type key) {
        // no guard
        // for simple remove of fake lock by optimizer
        m_lock.lock();

        const auto iter = m_tree.find(key);

        if (m_tree.end() == iter) {
            m_lock.unlock();

            return 0;
        }

        const auto res = m_tree.erase(key);
        m_lock.unlock();

        Node* const node = *iter;
        delete node;

        assert(1 == res);

        return 1;
    }

    //--------------------------------------------------------------//
    template<class K, class T, class L>
    void Map<K, T, L>::clear() noexcept {
        m_tree.clearWithDestruct();
    }

    //--------------------------------------------------------------//
    template<class K, class T, class L>
    size_t Map<K, T, L>::size() const noexcept {
        return m_tree.size();
    }

    //--------------------------------------------------------------//

}  // namespace Relax
