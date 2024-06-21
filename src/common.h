#pragma once

#define CPU_PAUSE() __asm volatile("pause" :::)
#define CACHELINE_SIZE 64

namespace Relax {
    template<typename T>
    concept Chainable = requires(T value) { value.m_next = &value; };

    template<class T>
    struct TChainableBase {
        T* m_next = nullptr;
    };

    template<typename T>
    concept Woody = requires(T value) {
        value.m_parent = &value;
        value.m_left = &value;
        value.m_right = &value;
    };

    template<typename T>
    concept SelfKeyed = requires(T value) {
        { value.m_key < value.m_key } noexcept;
        { value.m_key == value.m_key } noexcept;
    };

    template<typename T>
    concept IntrusiveMappable = Woody<T> && SelfKeyed<T>;

    template<class K, class T>
    struct TIntrusiveMappableBase {
        TIntrusiveMappableBase() = delete;

        template<typename... Args>
        TIntrusiveMappableBase(Args&&... args)
          : m_key(std::forward<Args>(args)...) { }

        T* m_parent = nullptr;
        T* m_left = nullptr;
        T* m_right = nullptr;
        K m_key;
    };
}  // namespace Relax
