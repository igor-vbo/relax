#pragma once

//#include <atomic>

#include "common.h"
#include "types.h"

namespace Relax {
    template<class T>
    inline void AssertIsAtomic(const T* const ptr) {
        assert(0 == (reinterpret_cast<size_t>(ptr) % sizeof(T)));
        assert((reinterpret_cast<size_t>(ptr) % CACHELINE_SIZE) <= (CACHELINE_SIZE - sizeof(T)));
    }

    namespace NAtomic {

#if LIN || MAC
        static_assert(__ATOMIC_RELAXED == (int)std::memory_order_relaxed);
        static_assert(__ATOMIC_ACQUIRE == (int)std::memory_order_acquire);
        static_assert(__ATOMIC_RELEASE == (int)std::memory_order_release);
        static_assert(__ATOMIC_CONSUME == (int)std::memory_order_consume);
        static_assert(__ATOMIC_SEQ_CST == (int)std::memory_order_seq_cst);

        template<typename T>
        inline T load(T* ptr, std::memory_order mode) {
            AssertIsAtomic(ptr);
            return __atomic_load_n(ptr, int(mode));
        }

        template<typename T>
        inline void store(T* ptr, T value, std::memory_order mode) {
            AssertIsAtomic(ptr);
            __atomic_store_n(ptr, value, int(mode));
        }

        template<typename T>
        inline T exchange(T* ptr, T value, std::memory_order mode) {
            AssertIsAtomic(ptr);
            return __atomic_exchange_n(ptr, value, int(mode));
        }

        template<typename T>
        inline bool compare_exchange_strong(T* ptr,
                                                   T* expected,
                                                   T desired,
                                                   std::memory_order success_memorder,
                                                   std::memory_order failure_memorder) {
            AssertIsAtomic(ptr);
            return __atomic_compare_exchange_n(ptr,
                                               expected,
                                               desired,
                                               false,
                                               int(success_memorder),
                                               int(failure_memorder));
        }

        template<typename T>
        inline bool compare_exchange_weak(T* ptr,
                                                 T* expected,
                                                 T desired,
                                                 std::memory_order success_memorder,
                                                 std::memory_order failure_memorder) {
            AssertIsAtomic(ptr);
            return __atomic_compare_exchange_n(ptr,
                                               expected,
                                               desired,
                                               true,
                                               int(success_memorder),
                                               int(failure_memorder));
        }
#elif WIN
        // TODO: seqcst
        // msvc: ... Acquire/release semantics are guaranteed on volatile accesses...
        template<typename T>
        inline T load(T* ptr, std::memory_order mode) {
            AssertIsAtomic(ptr);
            return *(reinterpret_cast<volatile T*>(ptr));
        }

        template<typename T>
        inline void store(T* ptr, T value, std::memory_order mode) {
            AssertIsAtomic(ptr);
            *(reinterpret_cast<volatile T*>(ptr)) = value;
        }

        template<typename T>
        inline T exchange(T* ptr, T value, std::memory_order mode) {
            AssertIsAtomic(ptr);
            return _InterlockedExchange(ptr, value);
        }

        template<typename T>
        inline bool compare_exchange_strong(T* ptr,
                                                   T* expected,
                                                   T desired,
                                                   std::memory_order success_memorder,
                                                   std::memory_order failure_memorder) {
            AssertIsAtomic(ptr);
            return desired == _InterlockedCompareExchange(ptr, desired, *expected);
        }

        template<typename T>
        inline bool compare_exchange_weak(T* ptr,
                                                 T* expected,
                                                 T desired,
                                                 std::memory_order success_memorder,
                                                 std::memory_order failure_memorder) {
            return compare_exchange_strong(ptr, expected, desired, success_memorder, failure_memorder);
        }
#else
#error
#endif
    };

}  // namespace Relax
