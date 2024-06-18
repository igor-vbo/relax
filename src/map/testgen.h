#pragma once

#include <gtest/gtest.h>

#include <algorithm>
#include <fstream>
#include <list>
#include <map>
#include <thread>
#include <unordered_set>

#include "common.h"

#define key_t uint32_t

namespace Test {
    //////////////////////////////////////////////////////////////////
    struct TestValue {
        TestValue* m_left;

        TestValue* m_right;

        TestValue* m_parent;

        key_t m_key;
    };

    //////////////////////////////////////////////////////////////////
    struct TestCommand;

    typedef void (*TestGenerator)(std::vector<TestCommand>& sample, uint32_t sample_size);

    typedef void (*TestGeneratorBucketed)(std::vector<TestCommand>& sample,
                                          uint32_t sample_size,
                                          uint32_t max_value,
                                          uint32_t nbuckets);

    //////////////////////////////////////////////////////////////////
    struct TestCommand {
        key_t m_key;

        bool m_is_add;

        template<class T>
        inline bool Exec(T& map, typename T::mapped_type& value) const {
            if (m_is_add)
                return map.emplace(m_key, value).second;
            else
                return (0 == map.erase(m_key));
        }
    };

    //////////////////////////////////////////////////////////////////
    inline void AddTestGeneratorBucketed(std::vector<TestCommand>& sample,
                                         uint32_t sample_size,
                                         uint32_t max_value,
                                         uint32_t nbuckets) {
        assert(0 == (sample_size % nbuckets));
        (void)max_value;
        sample.resize(sample_size);
        for (uint32_t i = 0; i < sample_size; ++i) {
            sample[i] = {i, true};
        }

        Rand64 rand;

        const uint32_t bucket_size = sample_size / nbuckets;
        for (uint32_t bucket = 0; bucket < nbuckets; ++bucket) {
            const uint32_t bucket_start = bucket * bucket_size;
            for (uint32_t i = 0; i < bucket_size; ++i) {
                uint64_t random = rand.get();
                uint32_t key = bucket_start + (random % (bucket_size - i));

                std::swap(sample[i], sample[key]);
            }
        }
    }

    //////////////////////////////////////////////////////////////////
    inline void AddRemoveTestGeneratorBucketed(std::vector<TestCommand>& sample,
                                               uint32_t sample_size,
                                               uint32_t max_value,
                                               uint32_t nbuckets) {
        // TODO: buckets
        (void)nbuckets;
        // RandMod64 rand;
        Rand64 rand;

        uint64_t vector = 0;

        for (uint32_t i = 0; i < sample_size; ++i) {
            const uint32_t key = rand.get() % max_value;
            const uint64_t shift = (uint64_t)1 << key;

            const bool is_in_map = (vector & shift);

            sample[i] = {key, !is_in_map};

            vector ^= shift;
        }
    }

    //////////////////////////////////////////////////////////////////
    template<class T>
    inline std::vector<T*> GenValues(uint32_t size) {
        std::vector<T*> res;
        res.reserve(size);

        for (uint32_t i = 0; i < size; ++i) {
            res.emplace_back(new T());
        }

        return res;
    }

    //--------------------------------------------------------------//
    template<class T>
    void KillValues(std::vector<T*>& values) {
        for (T*& val : values) {
            delete val;
        }
    }
}  // namespace Test
