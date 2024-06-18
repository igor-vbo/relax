#include <gtest/gtest.h>

#include <algorithm>
#include <queue>

#include "queue.h"
#include "test/test.h"

namespace Test {
    //////////////////////////////////////////////////////////////////
    class TestMutexFreeQueue : public ::testing::Test {
    public:
        using value_t = uint64_t;

        TestMutexFreeQueue() { }

        TestMutexFreeQueue(const TestMutexFreeQueue& other) = delete;
        TestMutexFreeQueue(TestMutexFreeQueue&& other) noexcept = delete;
        TestMutexFreeQueue& operator=(const TestMutexFreeQueue& other) = delete;
        TestMutexFreeQueue& operator=(TestMutexFreeQueue&& other) noexcept = delete;

        void testPushPerThreadOrder(uint64_t sample_size, uint32_t nthreads);

        void testPopPerThreadOrder(uint64_t sample_size, uint32_t nthreads);

        void testPushPop(uint64_t sample_size, uint32_t nthreads);

        bool checkAllValues(const std::vector<std::vector<value_t>>& values, value_t min, uint64_t sample_size);

    public:
        static constexpr uint32_t m_sample_size = 5000000;
    };

    //--------------------------------------------------------------//
    bool TestMutexFreeQueue::checkAllValues(const std::vector<std::vector<value_t>>& values,
                                            value_t min,
                                            uint64_t sample_size) {
        std::vector<value_t> all_values;
        all_values.reserve(sample_size);

        for (const auto& vec : values) {
            all_values.insert(all_values.end(), vec.begin(), vec.end());
        }

        std::sort(all_values.begin(), all_values.end(), std::less<value_t>());

        bool result = true;

        const value_t max = min + sample_size;
        value_t expected = min;
        for (value_t i = 0; i < all_values.size(); ++i) {
            result &= (all_values[i] == expected);
            ++expected;
        }

        result &= (max == expected);

        return result;
    }

    //--------------------------------------------------------------//
    void TestMutexFreeQueue::testPushPerThreadOrder(uint64_t sample_size, uint32_t nthreads) {
        sample_size = sample_size - (sample_size % nthreads);
        const uint32_t per_thread_sample_size = sample_size / nthreads;

        Relax::MutexFreeQueue<value_t> mfqueue;
        RunThreads(
            nthreads,
            [&mfqueue](uint32_t thread_id, uint32_t nthreads, uint32_t per_thread_sample_size) -> int {
                (void)nthreads;

                const uint64_t start = thread_id * per_thread_sample_size;
                const uint64_t end = start + per_thread_sample_size;
                Generator<uint64_t> g(start, 1, end);

                for (uint32_t i = 0; i < per_thread_sample_size; ++i) {
                    mfqueue.push(g.nextUp());
                }

                return 0;
            },
            per_thread_sample_size);

        std::vector<Generator<value_t>> generators;
        for (uint32_t tid = 0; tid < nthreads; ++tid) {
            const uint64_t start = tid * per_thread_sample_size;
            const uint64_t end = start + per_thread_sample_size;
            generators.emplace_back(start, 1, end);
        }

        for (uint64_t i = 0; i < sample_size; ++i) {
            value_t value;
            const bool is_pop = mfqueue.pop(value);
            ASSERT_TRUE(is_pop);

            const uint32_t tid = value / per_thread_sample_size;
            const uint64_t expected = generators[tid].nextUp();
            ASSERT_EQ(expected, value);
        }

        for (uint32_t i = 0; i < nthreads; ++i) {
            ASSERT_TRUE(generators[i].empty());
        }
    }

    //--------------------------------------------------------------//
    void TestMutexFreeQueue::testPopPerThreadOrder(uint64_t sample_size, uint32_t nthreads) {
        sample_size = sample_size - (sample_size % nthreads);

        Relax::MutexFreeQueue<value_t> mfqueue;
        // ST push
        for (value_t i = 1; i <= sample_size; ++i) {
            mfqueue.push(i);
        }

        // pops
        auto run_result = RunThreads(
            nthreads,
            [&mfqueue](uint32_t thread_id,
                       uint32_t nthreads,
                       uint64_t sample_size) -> std::pair<bool, std::vector<value_t>> {
                (void)nthreads;
                (void)thread_id;

                std::vector<value_t> values;
                values.reserve(2 * sample_size / nthreads);

                bool result = true;

                value_t prev_value = 0;

                value_t value;
                while (mfqueue.pop(value)) {
                    result &= (prev_value < value);
                    values.emplace_back(value);
                    prev_value = value;
                }

                return {result, values};
            },
            sample_size);

        value_t ret;
        ASSERT_FALSE(mfqueue.pop(ret));

        for (uint32_t i = 0; i < nthreads; ++i) {
            const bool is_order_correct = run_result.first[i].first;
            ASSERT_TRUE(is_order_correct);
        }

        std::vector<std::vector<value_t>> all_values;
        for (auto& thread_return : run_result.first) {
            all_values.emplace_back(std::move(thread_return.second));
        }

        ASSERT_TRUE(checkAllValues(all_values, 1, sample_size));
    }

    //--------------------------------------------------------------//
    void TestMutexFreeQueue::testPushPop(uint64_t sample_size, uint32_t nthreads) {
        sample_size = sample_size - (sample_size % nthreads);

        Relax::MutexFreeQueue<value_t> mfqueue;

        auto run_result = RunThreads(
            nthreads,
            [&mfqueue](uint32_t thread_id,
                       uint32_t nthreads,
                       uint32_t sample_size) -> std::pair<bool, std::vector<value_t>> {
                (void)nthreads;
                (void)thread_id;
                constexpr uint32_t generator_offset = 1;

                bool result = true;
                std::vector<value_t> values;

                const uint32_t per_thread = sample_size * 2 / nthreads;
                const uint32_t tid = thread_id % 2;
                if (0 == tid) {
                    const uint64_t start = generator_offset + thread_id / 2 * per_thread;
                    const uint64_t end = start + per_thread;
                    Generator<uint64_t> g(start, 1, end);

                    while (!g.empty()) {
                        mfqueue.push(g.nextUp());
                    }
                }
                else {
                    values.reserve(per_thread);
                    std::vector<value_t> prev_values(nthreads / 2, 0);
                    value_t value;
                    for (uint32_t i = 0; i < per_thread; ++i) {
                        while (!mfqueue.pop(value)) {
                            std::this_thread::yield();
                        }

                        const uint32_t index = (value - generator_offset) / per_thread;
                        result &= (prev_values[index] < value);
                        values.emplace_back(value);
                        prev_values[index] = value;
                    }
                }

                return {result, values};
            },
            sample_size);

        value_t ret;
        ASSERT_FALSE(mfqueue.pop(ret));

        for (uint32_t i = 0; i < nthreads; ++i) {
            const bool is_order_correct = run_result.first[i].first;
            ASSERT_TRUE(is_order_correct);
        }

        std::vector<std::vector<value_t>> all_values;
        for (auto& thread_return : run_result.first) {
            all_values.emplace_back(std::move(thread_return.second));
        }

        ASSERT_TRUE(checkAllValues(all_values, 1, sample_size));
    }

    //--------------------------------------------------------------//

    //////////////////////////////////////////////////////////////////
    TEST_F(TestMutexFreeQueue, MTPushPerThreadOrder_2) {
        constexpr uint32_t sample_size = m_sample_size;
        constexpr uint32_t nthreads = 2;

        testPushPerThreadOrder(sample_size, nthreads);
    }

    //--------------------------------------------------------------//
    TEST_F(TestMutexFreeQueue, MTPushPerThreadOrder_8) {
        constexpr uint32_t sample_size = m_sample_size;
        constexpr uint32_t nthreads = 8;

        testPushPerThreadOrder(sample_size, nthreads);
    }

    //--------------------------------------------------------------//
    TEST_F(TestMutexFreeQueue, MTPushPerThreadOrder_24) {
        constexpr uint32_t sample_size = m_sample_size;
        constexpr uint32_t nthreads = 24;

        testPushPerThreadOrder(sample_size, nthreads);
    }

    //--------------------------------------------------------------//
    TEST_F(TestMutexFreeQueue, MTPushPerThreadOrder_1024) {
        constexpr uint32_t sample_size = m_sample_size;
        constexpr uint32_t nthreads = 1024;

        testPushPerThreadOrder(sample_size, nthreads);
    }

    //////////////////////////////////////////////////////////////////
    TEST_F(TestMutexFreeQueue, MTPopPerThreadOrder_2) {
        constexpr uint32_t sample_size = m_sample_size;
        constexpr uint32_t nthreads = 2;

        testPopPerThreadOrder(sample_size, nthreads);
    }

    //--------------------------------------------------------------//
    TEST_F(TestMutexFreeQueue, MTPopPerThreadOrder_8) {
        constexpr uint32_t sample_size = m_sample_size;
        constexpr uint32_t nthreads = 8;

        testPopPerThreadOrder(sample_size, nthreads);
    }

    //--------------------------------------------------------------//
    TEST_F(TestMutexFreeQueue, MTPopPerThreadOrder_24) {
        constexpr uint32_t sample_size = m_sample_size;
        constexpr uint32_t nthreads = 24;

        testPopPerThreadOrder(sample_size, nthreads);
    }

    //--------------------------------------------------------------//
    TEST_F(TestMutexFreeQueue, MTPopPerThreadOrder_1024) {
        constexpr uint32_t sample_size = m_sample_size;
        constexpr uint32_t nthreads = 1024;

        testPopPerThreadOrder(sample_size, nthreads);
    }

    //////////////////////////////////////////////////////////////////
    TEST_F(TestMutexFreeQueue, MTPushPopPerThreadOrder_2) {
        constexpr uint32_t sample_size = m_sample_size;
        constexpr uint32_t nthreads = 2;

        testPushPop(sample_size, nthreads);
    }

    //--------------------------------------------------------------//
    TEST_F(TestMutexFreeQueue, MTPushPopPerThreadOrder_8) {
        constexpr uint32_t sample_size = m_sample_size;
        constexpr uint32_t nthreads = 8;

        testPushPop(sample_size, nthreads);
    }

    //--------------------------------------------------------------//
    TEST_F(TestMutexFreeQueue, MTPushPopPerThreadOrder_24) {
        constexpr uint32_t sample_size = m_sample_size;
        constexpr uint32_t nthreads = 24;

        testPushPop(sample_size, nthreads);
    }

    //--------------------------------------------------------------//
    TEST_F(TestMutexFreeQueue, MTPushPopPerThreadOrder_1024) {
        constexpr uint32_t sample_size = m_sample_size;
        constexpr uint32_t nthreads = 1024;

        testPushPop(sample_size, nthreads);
    }

}  // namespace Test
