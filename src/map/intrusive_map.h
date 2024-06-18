#pragma once

#include <cassert>
#include <queue>

#include "types.h"

namespace Relax {
    //////////////////////////////////////////////////////////////////
    template<class K, class T>
    class IntrusiveMap {
        // ptr: 0bXXXXX...XXXY
        // Y - color (0 - black, 1 - red)

        using value_t = T;
        // static_assert(std::is_pointer<T>(), "");
        static_assert(noexcept(std::declval<K>() < std::declval<K>()), "");
        static_assert(noexcept(std::declval<K>() != std::declval<K>()), "");
        static_assert(noexcept(std::declval<K>() == std::declval<K>()), "");

    public:
        typedef K key_type;
        typedef T mapped_type;
        typedef T* pointer_type;
        typedef T& reference;
        typedef const T& const_reference;
        typedef size_t size_type;

    public:
        class iterator;

        IntrusiveMap();

        IntrusiveMap(const IntrusiveMap& other) = delete;
        IntrusiveMap(IntrusiveMap&& other) noexcept = delete;
        IntrusiveMap& operator=(const IntrusiveMap& other) = delete;
        IntrusiveMap& operator=(IntrusiveMap&& other) noexcept = delete;

        iterator find(const key_type& key) noexcept;

        std::pair<iterator, bool> emplace(const key_type& key, pointer_type value);

        std::pair<iterator, bool> insert(pointer_type value) noexcept;

        size_t erase(const key_type& key) noexcept;

        iterator erase(iterator iter) noexcept;

        void clear() noexcept;

        void clearWithDestruct() noexcept;

        size_t size() const noexcept;

    public:
        class iterator : public std::iterator<std::input_iterator_tag, pointer_type> {
            friend class IntrusiveMap<K, T>;

            iterator(pointer_type node)
              : m_node(node) { }

        public:
            iterator(const iterator& it)
              : m_node(it.m_node) { }
            ~iterator() = default;

            iterator& operator=(const iterator& it) noexcept {
                m_node = it.m_node;
                return *this;
            }

            pointer_type operator*() const noexcept { return m_node; }
            pointer_type operator->() const noexcept { return m_node; }

            iterator& operator++() noexcept {
                m_node = next(m_node);
                return *this;
            }
            iterator operator++(int) noexcept {
                iterator it(*this);
                ++(*this);
                return it;
            }

            bool operator==(const iterator& other) const noexcept { return m_node == other.m_node; }
            bool operator!=(const iterator& other) const noexcept { return m_node != other.m_node; }

        private:
            pointer_type m_node;
        };

        iterator begin() const {
            if (nullptr == m_root)
                return end();
            return iterator(maxLeft(m_root));
        }

        iterator end() const { return iterator(nullptr); }

    public:
        bool checkRB() noexcept;

    private:
        static pointer_type next(pointer_type node) noexcept;

        static inline pointer_type maxLeft(pointer_type node) noexcept;

        static void erase_swap(pointer_type one, pointer_type other) noexcept;

    private:
        static pointer_type uncle(pointer_type const parent) noexcept;

        static inline void pred_rotate(pointer_type const parent,
                                       pointer_type const node,
                                       pointer_type const grandpa);

        static inline void rotate_left(pointer_type const parent, pointer_type const node);

        static inline void rotate_right(pointer_type const parent, pointer_type const node);

        static inline bool isChildsBlack(pointer_type node);

    private:
        static inline size_t color(pointer_type node);

        static inline bool is_node_black(pointer_type node);

        static inline bool is_node_red(pointer_type node);

        static inline void set_parent_save_color(pointer_type node, pointer_type parent);

        static inline pointer_type red(pointer_type node);

        static inline pointer_type black(pointer_type ptr);

        static inline pointer_type pure(pointer_type ptr);

        static inline void assert_pure(pointer_type ptr);

    private:
        pointer_type m_root;

        size_t m_size;
    };

    //--------------------------------------------------------------//
    template<class K, class T>
    IntrusiveMap<K, T>::IntrusiveMap()
      : m_root(nullptr)
      , m_size(0) { }

    //--------------------------------------------------------------//
    template<class K, class T>
    typename IntrusiveMap<K, T>::iterator IntrusiveMap<K, T>::find(const key_type& key) noexcept {
        if (nullptr == m_root) {
            return end();
        }

        pointer_type node = m_root;
        while (key != node->m_key) {
            node = pure((key < node->m_key) ? node->m_left : node->m_right);

            if (nullptr == node)
                return end();
        }

        return iterator(node);
    }

    //--------------------------------------------------------------//
    template<class K, class T>
    std::pair<typename IntrusiveMap<K, T>::iterator, bool> IntrusiveMap<K, T>::emplace(const key_type& key,
                                                                                       pointer_type value) {
        if constexpr (noexcept(value->m_key = key)) {
            value->m_key = key;
        }
        else {
            try {
                value->m_key = key;
            }
            catch (...) {
                return end();
            }
        }

        return insert(value);
    }

    //--------------------------------------------------------------//
    template<class K, class T>
    std::pair<typename IntrusiveMap<K, T>::iterator, bool> IntrusiveMap<K, T>::insert(
        pointer_type const value) noexcept {
        const key_type& key = value->m_key;

        if (nullptr == m_root) {
            m_root = value;
            value->m_left = nullptr;
            value->m_right = nullptr;
            value->m_parent = nullptr;
            ++m_size;
            return std::pair<iterator, bool>(iterator(value), true);
        }

        pointer_type node = m_root;
        while (true) {
            if (key == node->m_key)
                return std::pair<iterator, bool>(iterator(node), false);

            pointer_type const next = pure((key < node->m_key) ? node->m_left : node->m_right);

            if (nullptr == next)
                break;
            else
                node = next;
        }

        if (key < node->m_key)
            node->m_left = value;
        else
            node->m_right = value;

        value->m_parent = red(node);
        value->m_left = nullptr;
        value->m_right = nullptr;
        ++m_size;
        const iterator result_iterator = iterator(value);

        if (is_node_black(node)) {
            return std::pair<iterator, bool>(result_iterator, true);
        }

        // repair
        pointer_type parent = node;
        node = value;

        // grandfather definitely exists and is black (parent is red)
        pointer_type grandpa = pure(parent->m_parent);
        pointer_type uncle = pure((pure(grandpa->m_left) == parent) ? grandpa->m_right : grandpa->m_left);
        // while (is_ptr_red(uncle))
        while ((nullptr != uncle) && is_node_red(uncle)) {
            assert(nullptr != grandpa && is_node_black(grandpa));
            parent->m_parent = grandpa;  // black
            uncle->m_parent = grandpa;   // black

            if (nullptr == grandpa->m_parent) {
                return std::pair<iterator, bool>(result_iterator, true);
            }

            pointer_type const grandgrandpa = pure(grandpa->m_parent);
            grandpa->m_parent = red(grandgrandpa);

            if (is_node_black(grandgrandpa)) {
                return std::pair<iterator, bool>(result_iterator, true);
            }

            parent = grandgrandpa;
            node = grandpa;
            grandpa = pure(grandgrandpa->m_parent);
            uncle = pure((pure(grandpa->m_left) == parent) ? grandpa->m_right : grandpa->m_left);
        }

        // uncle is black
        if (pure(parent->m_right) == node && pure(grandpa->m_left) == parent) {
            pred_rotate(parent, node, grandpa);
            rotate_left(parent, node);
            node->m_parent = red(node->m_parent);
            // parent->m_parent = red(parent->m_parent);
            std::swap(parent, node);
        }
        else if (pure(parent->m_left) == node && pure(grandpa->m_right) == parent) {
            pred_rotate(parent, node, grandpa);
            rotate_right(parent, node);
            node->m_parent = red(node->m_parent);
            // parent->m_parent = red(parent->m_parent);
            std::swap(parent, node);
        }

        // case 5 (cascade)
        // pred_rotate(grandpa, parent, grandgrandpa);
        {
            assert(is_node_black(grandpa));
            pointer_type const grandgrandpa = grandpa->m_parent;  // pure (black)
            parent->m_parent = grandgrandpa;
            if (nullptr != grandgrandpa) {
                if (pure(grandgrandpa->m_left) == grandpa)
                    grandgrandpa->m_left = parent;
                else
                    grandgrandpa->m_right = parent;
            }
            else {
                assert(grandpa == m_root);
                m_root = parent;
            }
        }
        if (pure(parent->m_left) == node)  // && pure(grandpa->m_left) == parent)
        {
            rotate_right(grandpa, parent);
        }
        else  // pure(parent->m_right) == node) && pure(grandpa->m_right) == parent)
        {
            rotate_left(grandpa, parent);
        }

        return std::pair<iterator, bool>(result_iterator, true);
    }

    //--------------------------------------------------------------//
    template<class K, class T>
    size_t IntrusiveMap<K, T>::erase(const key_type& key) noexcept {
        iterator iter = find(key);
        if (nullptr == iter.m_node)
            return 0;

        assert(iter.m_node->m_key == key);
        erase(iter);

        return 1;
    }

    //--------------------------------------------------------------//
    template<class K, class T>
    typename IntrusiveMap<K, T>::iterator IntrusiveMap<K, T>::erase(iterator iter) noexcept {
        if (nullptr == iter.m_node)
            return iter;

        assert(nullptr != m_root);
        --m_size;

        const iterator next_iter = iterator(next(iter.m_node));
        pointer_type node = iter.m_node;
        if ((nullptr != node->m_left) && (nullptr != node->m_right)) {
            pointer_type const min_right = maxLeft(pure(node->m_right));
            if (nullptr == node->m_parent)
                m_root = min_right;

            erase_swap(node, min_right);
        }

        pointer_type parent = pure(node->m_parent);
        if (is_node_red(node)) {
            assert((nullptr == node->m_left) && (nullptr == node->m_right));
            if (node == parent->m_left)
                parent->m_left = nullptr;
            else
                parent->m_right = nullptr;

            return next_iter;
        }

        pointer_type const child = (nullptr != node->m_left) ? node->m_left : node->m_right;

        if (nullptr != child)  // if (is_node_red(child))
        {
            assert(is_node_red(child));
            if (nullptr != parent) {
                if (node == parent->m_left)
                    parent->m_left = child;
                else
                    parent->m_right = child;
            }
            else {
                m_root = child;
            }
            child->m_parent = parent;  // black

            return next_iter;
        }

        assert((nullptr == node->m_left) && (nullptr == node->m_right));
        assert(nullptr == child);

        // case 1
        if (nullptr == parent)  //(m_root == node)
        {
            m_root = nullptr;
            assert(0 == m_size);
            return next_iter;
        }

        if (node == parent->m_left)
            parent->m_left = nullptr;
        else
            parent->m_right = nullptr;

        // repair
        pointer_type brother = (nullptr == parent->m_left) ? parent->m_right : parent->m_left;
        while (true) {
            assert(nullptr != brother);

            // case 2
            if (is_node_red(brother)) {
                assert(is_node_black(parent));

                pointer_type const grandpa = pure(parent->m_parent);
                brother->m_parent = grandpa;
                if (nullptr != grandpa) {
                    if (pure(grandpa->m_left) == parent)
                        grandpa->m_left = brother;
                    else
                        grandpa->m_right = brother;
                }
                else {
                    m_root = brother;
                }

                if (brother == parent->m_right) {
                    rotate_left(parent, brother);
                    assert(is_node_black(brother));
                    brother = parent->m_right;
                }
                else {
                    rotate_right(parent, brother);
                    assert(is_node_black(brother));
                    brother = parent->m_left;
                }

                // post
                assert(is_node_red(parent));
            }
            assert(is_node_black(brother));

            const bool is_childs_black = isChildsBlack(brother);
            if (!is_childs_black) {
                break;
            }

            if (is_node_black(parent)) {
                // case 3
                brother->m_parent = red(brother->m_parent);

                pointer_type const grandpa = pure(parent->m_parent);
                if (nullptr == grandpa) {
                    return next_iter;
                }

                brother = (parent == grandpa->m_left) ? grandpa->m_right : grandpa->m_left;

                parent = grandpa;
            }
            else {
                // case 4
                assert(is_node_red(parent));
                brother->m_parent = red(brother->m_parent);
                parent->m_parent = black(parent->m_parent);
                return next_iter;
            }
        }

        const size_t old_parent_color = color(parent);
        pointer_type left_brother_child = pure(brother->m_left);
        pointer_type right_brother_child = pure(brother->m_right);
        if (brother == parent->m_right) {
            // case 5
            if (nullptr == right_brother_child || is_node_black(right_brother_child)) {
                assert(is_node_red(brother->m_left));

                pred_rotate(brother, left_brother_child, parent);

                rotate_right(brother, left_brother_child);

                brother = left_brother_child;
            }
            assert(is_node_black(brother));
            assert(is_node_red(brother->m_right));

            // case 6
            {
                const pointer_type grandpa = pure(parent->m_parent);
                brother->m_parent = (pointer_type)((size_t)grandpa | old_parent_color);
                if (nullptr != grandpa) {
                    if (pure(grandpa->m_left) == parent)
                        grandpa->m_left = brother;
                    else
                        grandpa->m_right = brother;
                }
                else {
                    m_root = brother;
                }
            }

            parent->m_right = brother->m_left;
            if (nullptr != brother->m_left)
                set_parent_save_color(brother->m_left, parent);

            parent->m_parent = brother;  // black
            brother->m_left = parent;
            brother->m_right->m_parent = black(brother->m_right->m_parent);
        }
        else {
            pointer_type right_brother_child = pure(brother->m_right);
            // case 5
            if (nullptr == left_brother_child || is_node_black(left_brother_child)) {
                assert(is_node_red(brother->m_right));

                pred_rotate(brother, right_brother_child, parent);

                rotate_left(brother, right_brother_child);

                brother = right_brother_child;
            }
            assert(is_node_black(brother));
            assert(is_node_red(brother->m_left));

            // case 6
            {
                const pointer_type grandpa = pure(parent->m_parent);
                brother->m_parent = (pointer_type)((size_t)grandpa | old_parent_color);
                if (nullptr != grandpa) {
                    if (pure(grandpa->m_left) == parent)
                        grandpa->m_left = brother;
                    else
                        grandpa->m_right = brother;
                }
                else {
                    m_root = brother;
                }
            }

            parent->m_left = brother->m_right;
            if (nullptr != brother->m_right)
                set_parent_save_color(brother->m_right, parent);

            parent->m_parent = brother;  // black
            brother->m_right = parent;
            brother->m_left->m_parent = black(brother->m_left->m_parent);
        }

        return next_iter;
    }

    //--------------------------------------------------------------//
    template<class K, class T>
    void IntrusiveMap<K, T>::clear() noexcept {
        m_root = nullptr;
        m_size = 0;
    }

    //--------------------------------------------------------------//
    template<class K, class T>
    void IntrusiveMap<K, T>::clearWithDestruct() noexcept {
        pointer_type node = m_root;
        while (nullptr != node) {
            pointer_type next = node->m_left;
            if (nullptr == next) {
                next = node->m_right;
                if (nullptr == next) {
                    next = pure(node->m_parent);
                    if (nullptr != next) {
                        if (node == next->m_left)
                            next->m_left = nullptr;
                        else
                            next->m_right = nullptr;
                    }
                    delete node;
                }
            }

            node = next;
        }

        m_root = nullptr;
        m_size = 0;
    }

    //--------------------------------------------------------------//
    template<class K, class T>
    size_t IntrusiveMap<K, T>::size() const noexcept {
        return m_size;
    }

    //--------------------------------------------------------------//
    template<class K, class T>
    bool IntrusiveMap<K, T>::checkRB() noexcept {
        if (nullptr == m_root) {
            assert(0 == m_size);
            return true;
        }
        assert(is_node_black(m_root));

        size_t size = 1;

        uint32_t depth = 0;

        std::queue<std::pair<pointer_type, uint32_t>> queue;

        queue.emplace(m_root->m_left, 1);
        queue.emplace(m_root->m_right, 1);

        while (queue.size()) {
            std::pair<pointer_type, uint32_t> p = queue.front();
            queue.pop();
            pointer_type node = p.first;
            uint32_t d = p.second;

            if (nullptr == node) {
                if (0 == depth)
                    depth = d;

                assert(depth == d);
            }
            else {
                assert(nullptr != node->m_parent);
                ++size;
                const bool is_black = is_node_black(node);
                const pointer_type parent = pure(node->m_parent);

                assert(!is_node_red(parent) || is_black);

                if (is_black)
                    ++d;

                queue.emplace(node->m_left, d);
                queue.emplace(node->m_right, d);
            }
        }
        assert(m_size == size);

        return true;
    }

    //--------------------------------------------------------------//
    template<class K, class T>
    T* IntrusiveMap<K, T>::next(pointer_type node) noexcept {
        if (nullptr != node->m_right) {
            return maxLeft(pure(node->m_right));
        }

        pointer_type parent = pure(node->m_parent);
        while (nullptr != parent && parent->m_key <= node->m_key) {
            parent = pure(parent->m_parent);
        }

        return parent;
    }

    //--------------------------------------------------------------//
    template<class K, class T>
    T* IntrusiveMap<K, T>::maxLeft(pointer_type node) noexcept {
        while (nullptr != node->m_left)
            node = pure(node->m_left);

        return node;
    }

    //--------------------------------------------------------------//
    template<class K, class T>
    void IntrusiveMap<K, T>::erase_swap(pointer_type one, pointer_type other) noexcept {
        // one may be root
        assert(nullptr != other->m_parent);
        assert(one->m_key < other->m_key);  // other is maxLeft right son of one
        assert(nullptr == other->m_left);

        pointer_type parent_one = pure(one->m_parent);
        pointer_type parent_other = pure(other->m_parent);

        if (nullptr != parent_one) {
            if (one == parent_one->m_left)
                parent_one->m_left = other;
            else
                parent_one->m_right = other;
        }

        if (nullptr != other->m_right)
            set_parent_save_color(other->m_right, one);

        if (nullptr != one->m_left)
            set_parent_save_color(one->m_left, other);

        const pointer_type old_other_right = other->m_right;
        // const size_t old_other_color = color(other->m_parent);
        pointer_type new_parent_one;

        if (one == parent_other) {
            new_parent_one = (pointer_type)((size_t)other | color(other));

            other->m_right = one;
        }
        else {
            new_parent_one = other->m_parent;

            parent_other->m_left = one;

            if (nullptr != one->m_right)
                set_parent_save_color(one->m_right, other);

            other->m_right = one->m_right;
        }

        other->m_left = one->m_left;
        other->m_parent = one->m_parent;

        one->m_left = nullptr;
        one->m_right = old_other_right;
        one->m_parent = new_parent_one;
    }

    //--------------------------------------------------------------//
    template<class K, class T>
    T* IntrusiveMap<K, T>::uncle(pointer_type const parent) noexcept {
        assert_pure(parent);

        pointer_type const grandpa = pure(parent->m_parent);
        assert(nullptr != grandpa);

        if (parent == pure(grandpa->m_left))
            return grandpa->m_right;
        else
            return grandpa->m_left;
    }

    //--------------------------------------------------------------//
    template<class K, class T>
    void IntrusiveMap<K, T>::pred_rotate(pointer_type const parent,
                                         pointer_type const node,
                                         pointer_type const grandpa) {
        assert_pure(parent);
        assert_pure(node);
        assert_pure(grandpa);
        assert(nullptr != node);
        assert(nullptr != parent);

        node->m_parent = grandpa;  // red?
        if (pure(grandpa->m_left) == parent)
            grandpa->m_left = node;
        else
            grandpa->m_right = node;
    }

    //--------------------------------------------------------------//
    template<class K, class T>
    void IntrusiveMap<K, T>::rotate_left(pointer_type const parent, pointer_type const node) {
        parent->m_right = node->m_left;
        if (nullptr != node->m_left)
            set_parent_save_color(node->m_left, parent);

        parent->m_parent = red(node);
        node->m_left = parent;
    }

    //--------------------------------------------------------------//
    template<class K, class T>
    void IntrusiveMap<K, T>::rotate_right(pointer_type const parent, pointer_type const node) {
        parent->m_left = node->m_right;
        if (nullptr != node->m_right)
            set_parent_save_color(node->m_right, parent);

        parent->m_parent = red(node);
        node->m_right = parent;
    }

    //--------------------------------------------------------------//
    template<class K, class T>
    bool IntrusiveMap<K, T>::isChildsBlack(pointer_type node) {
        return ((nullptr == node->m_left) || is_node_black(node->m_left)) &&
               ((nullptr == node->m_right) || is_node_black(node->m_right));
    }

    //--------------------------------------------------------------//
    template<class K, class T>
    size_t IntrusiveMap<K, T>::color(pointer_type node) {
        return (size_t)node->m_parent & (size_t)1;
    }

    //--------------------------------------------------------------//
    template<class K, class T>
    bool IntrusiveMap<K, T>::is_node_black(pointer_type node) {
        return 0 == ((size_t)node->m_parent & (size_t)1);
    }

    //--------------------------------------------------------------//
    template<class K, class T>
    bool IntrusiveMap<K, T>::is_node_red(pointer_type node) {
        return 0 != ((size_t)node->m_parent & (size_t)1);
    }

    //--------------------------------------------------------------//
    template<class K, class T>
    void IntrusiveMap<K, T>::set_parent_save_color(pointer_type node, pointer_type parent) {
        assert_pure(parent);
        node->m_parent = (pointer_type)((size_t)parent | color(node));
    }

    //--------------------------------------------------------------//
    template<class K, class T>
    T* IntrusiveMap<K, T>::red(pointer_type node) {
        return (pointer_type)((size_t)node | (size_t)1);
    }

    //--------------------------------------------------------------//
    template<class K, class T>
    T* IntrusiveMap<K, T>::black(pointer_type ptr) {
        return (pointer_type)((size_t)ptr & (~((size_t)0b1)));
    }

    //--------------------------------------------------------------//
    template<class K, class T>
    T* IntrusiveMap<K, T>::pure(pointer_type ptr) {
        return (pointer_type)((size_t)ptr & (~((size_t)0b111)));
    }

    //--------------------------------------------------------------//
    template<class K, class T>
    void IntrusiveMap<K, T>::assert_pure(pointer_type ptr) {
        assert(0 == (((size_t)ptr) & (size_t)0b111));
    }
}  // namespace Relax
