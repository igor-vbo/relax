#pragma once

#ifdef __linux__
#define LIN 1
#endif

#if defined(__APPLE__) && defined(__MACH__)
#define MAC 1
#endif

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
#define WIN 1
#endif

#if WIN
    #include <immintrin.h>
#endif

#if WIN
#define CPU_PAUSE() _mm_pause();
#else
#define CPU_PAUSE() __asm volatile("pause" :::)
#endif

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
