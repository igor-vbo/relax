#include <gtest/gtest.h>

#include <queue>

#define RELAX_MF_QUEUE_VERIFICATION 1
#include "queue.h"
#include "test/test.h"

namespace Test {
    //////////////////////////////////////////////////////////////////
    class TestMutexFreeQueueVerification : public ::testing::Test {
    public:
        struct Node {
            uint64_t m_id;
            uint64_t m_pop_cnt;
        };

        using value_t = Node;

        TestMutexFreeQueueVerification() { }

        TestMutexFreeQueueVerification(const TestMutexFreeQueueVerification& other) = delete;
        TestMutexFreeQueueVerification(TestMutexFreeQueueVerification&& other) noexcept = delete;
        TestMutexFreeQueueVerification& operator=(const TestMutexFreeQueueVerification& other) = delete;
        TestMutexFreeQueueVerification& operator=(TestMutexFreeQueueVerification&& other) noexcept = delete;

        void verification(uint64_t sample_size, uint32_t nthreads, uint32_t nsections);
    };

    //--------------------------------------------------------------//
    void TestMutexFreeQueueVerification::verification(uint64_t sample_size, uint32_t nthreads, uint32_t nsections) {
        sample_size = sample_size - (sample_size % nthreads);
        const uint32_t per_thread_sample_size = sample_size / nthreads;

        Relax::MutexFreeQueue<value_t> mfqueue;
        std::vector<std::vector<value_t>> pops_by_thread =
            RunThreads(
                nthreads,
                [&mfqueue](uint32_t thread_id,
                           uint32_t nthreads,
                           uint32_t sample_size,
                           uint32_t per_thread_sample_size,
                           uint32_t nsections) -> std::vector<value_t> {
                    (void)nthreads;
                    (void)sample_size;
                    const uint32_t per_section = per_thread_sample_size / nsections;
                    std::vector<value_t> res;
                    res.reserve(2 * per_thread_sample_size);

                    const uint64_t start = thread_id * per_thread_sample_size;
                    const uint64_t end = start + per_thread_sample_size;
                    Generator<uint64_t> g(start, 1, end);

                    for (uint32_t section = 1; section < nsections; ++section) {
                        for (uint32_t i = 0; i < per_section; ++i) {
                            mfqueue.push({g.nextUp(), UINT64_MAX});
                        }

                        value_t ret;
                        while (mfqueue.pop(ret)) {
                            res.emplace_back(ret);
                        }
                    }

                    while (!g.empty()) {
                        mfqueue.push({g.nextUp(), UINT64_MAX});
                    }

                    value_t ret;
                    while (mfqueue.pop(ret)) {
                        res.emplace_back(ret);
                    }

                    return res;
                },
                sample_size,
                per_thread_sample_size,
                nsections)
                .first;

        size_t total_size = 0;
        for (const auto& thread_trace : pops_by_thread) {
            total_size += thread_trace.size();
        }

        ASSERT_EQ(sample_size, total_size);

        std::vector<uint64_t> total_trace;
        total_trace.resize(sample_size);
        for (const auto& thread_trace : pops_by_thread) {
            for (const auto& node : thread_trace) {
                total_trace[node.m_pop_cnt] = node.m_id;
            }
        }
        pops_by_thread.clear();

        std::vector<Generator<uint64_t>> generators;
        for (uint32_t tid = 0; tid < nthreads; ++tid) {
            const uint64_t start = tid * per_thread_sample_size;
            const uint64_t end = start + per_thread_sample_size;
            generators.emplace_back(start, 1, end);
        }

        for (uint64_t i = 0; i < sample_size; ++i) {
            const uint64_t value = total_trace[i];
            const uint32_t tid = value / per_thread_sample_size;
            const uint64_t expected = generators[tid].nextUp();
            ASSERT_EQ(expected, value);
        }

        for (uint32_t i = 0; i < nthreads; ++i) {
            ASSERT_TRUE(generators[i].empty());
        }
    }

    //--------------------------------------------------------------//

    //////////////////////////////////////////////////////////////////

    //--------------------------------------------------------------//
    TEST_F(TestMutexFreeQueueVerification, verification) {
        constexpr uint32_t sample_size = 10000000;
        constexpr uint32_t nthreads = 8;
        constexpr uint32_t nsections = 100000;

        verification(sample_size, nthreads, nsections);
    }

}  // namespace Test
