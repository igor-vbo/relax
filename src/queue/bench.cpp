#include <gtest/gtest.h>

#include <queue>

#include "queue.h"
#include "test/test.h"

namespace Test {
    //////////////////////////////////////////////////////////////////
    template<class T>
    class SillyMutexQueue {
        struct Node {
            Node* m_next;
            T m_value;

            template<typename... Args>
            Node(Args&&... args)
              : m_next(nullptr)
              , m_value(std::forward<Args>(args)...) { }
        };

    public:
        typedef T value_type;

        SillyMutexQueue()
          : m_head(nullptr)
          , m_tail(nullptr)
          , m_mutex() { }
        ~SillyMutexQueue() { clear(); }

        template<typename... Args>
        void push(Args&&... args) {
            Node* node = new Node(std::forward<Args>(args)...);
            std::unique_lock<std::mutex> g(m_mutex);
            Node* old_tail = m_tail;
            m_tail = node;
            if (nullptr == old_tail) {
                m_head = node;
            }
            else {
                old_tail->m_next = node;
            }
        }

        bool pop(T& ref) {
            std::unique_lock<std::mutex> g(m_mutex);
            if (nullptr == m_head)
                return false;

            Node* node = m_head;
            m_head = m_head->m_next;
            if (nullptr == m_head) {
                m_tail = nullptr;
            }
            g.unlock();

            new (&ref) T(std::move(node->m_value));
            delete node;
            return true;
        }

        void clear() {
            T ret;
            while (pop(ret))
                ;
        }

    private:
        Node* m_head;

        Node* m_tail;

        std::mutex m_mutex;
    };

    //////////////////////////////////////////////////////////////////
    template<class T, class PushType = typename T::value_type>
    std::pair<Duration, Duration> BenchQueueSeqPush(uint64_t sample_size, uint32_t nthreads, uint32_t niterations) {
        std::vector<std::remove_pointer_t<PushType>> prealloced;
        if constexpr (std::is_pointer_v<PushType>) {
            prealloced.resize(sample_size);
        }

        return BenchThreads<T>(
            nthreads,
            niterations,
            [&prealloced](uint32_t thread_id, uint32_t nthreads, T& queue, uint32_t sample_size) -> int {
                (void)prealloced;
                const uint32_t per_thread = sample_size / nthreads;

                const uint32_t start = thread_id * per_thread;
                const uint32_t end = start + per_thread;
                Generator<uint32_t> g(start, 1, end);

                while (!g.empty()) {
                    if constexpr (std::is_pointer_v<PushType>) {
                        queue.push(&(prealloced[g.get()]));
                    }
                    else {
                        queue.push(g.get());
                    }
                }

                return 0;
            },
            sample_size);
    }

    //--------------------------------------------------------------//
    template<class T, class PushType = typename T::value_type>
    std::pair<Duration, Duration> BenchQueueSeqPushPop(uint64_t sample_size,
                                                       uint32_t nthreads,
                                                       uint32_t niterations) {
        std::vector<std::remove_pointer_t<PushType>> prealloced;
        if constexpr (std::is_pointer_v<PushType>) {
            prealloced.resize(sample_size);
        }

        return BenchThreads<T>(
            nthreads,
            niterations,
            [&prealloced](uint32_t thread_id, uint32_t nthreads, T& queue, uint32_t sample_size) -> int {
                (void)prealloced;
                const uint32_t per_thread = sample_size / nthreads;

                const uint32_t start = thread_id * per_thread;
                const uint32_t end = start + per_thread;
                Generator<uint32_t> g(start, 1, end);

                while (!g.empty()) {
                    if constexpr (std::is_pointer_v<PushType>) {
                        queue.push(&(prealloced[g.get()]));
                    }
                    else {
                        queue.push(g.get());
                    }
                }

                PushType ret;
                while (queue.pop(ret))
                    ;

                return 0;
            },
            sample_size);
    }

    //--------------------------------------------------------------//
    template<class T, class PushType = typename T::value_type>
    std::pair<Duration, Duration> BenchQueueSectioned(uint64_t sample_size,
                                                      uint32_t nthreads,
                                                      uint32_t nsections,
                                                      uint32_t niterations) {
        std::vector<std::remove_pointer_t<PushType>> prealloced;
        if constexpr (std::is_pointer_v<PushType>) {
            prealloced.resize(sample_size);
        }

        return BenchThreads<T>(
            nthreads,
            niterations,
            [&prealloced](uint32_t thread_id, uint32_t nthreads, T& queue, uint32_t sample_size, uint32_t nsections)
                -> int {
                (void)prealloced;
                const uint32_t per_thread = sample_size / nthreads;
                const uint32_t per_section = per_thread / nsections;

                const uint32_t start = thread_id * per_thread;
                const uint32_t end = start + per_thread;
                Generator<uint32_t> g(start, 1, end);

                for (uint32_t section = 0; section < nsections; ++section) {
                    for (uint32_t i = 0; i < per_section; ++i) {
                        if constexpr (std::is_pointer_v<PushType>) {
                            queue.push(&(prealloced[g.get()]));
                        }
                        else {
                            queue.push(g.get());
                        }
                    }

                    PushType ret;
                    while (queue.pop(ret))
                        ;
                }

                return 0;
            },
            sample_size,
            nsections);
    }

    //--------------------------------------------------------------//
    template<class T, class PushType = typename T::value_type>
    std::pair<Duration, Duration> BenchQueueParPushPop(uint64_t sample_size,
                                                       uint32_t nthreads,
                                                       uint32_t niterations) {
        std::vector<std::remove_pointer_t<PushType>> prealloced;
        if constexpr (std::is_pointer_v<PushType>) {
            prealloced.resize(sample_size);
        }

        return BenchThreads<T>(
            nthreads,
            niterations,
            [&prealloced](uint32_t thread_id, uint32_t nthreads, T& queue, uint32_t sample_size) -> int {
                (void)prealloced;
                const uint32_t per_thread = sample_size * 2 / nthreads;

                const uint32_t start = thread_id / 2 * per_thread;
                const uint32_t end = start + per_thread;
                Generator<uint32_t> g(start, 1, end);

                const uint32_t tid = thread_id % 2;
                if (0 == tid) {
                    while (!g.empty()) {
                        if constexpr (std::is_pointer_v<PushType>) {
                            queue.push(&(prealloced[g.get()]));
                        }
                        else {
                            queue.push(g.get());
                        }
                    }
                }
                else {
                    PushType value;
                    for (uint32_t i = 0; i < per_thread; ++i) {
                        while (!queue.pop(value)) {
                            std::this_thread::yield();
                        }
                    }
                }

                return 0;
            },
            sample_size);
    }

    //////////////////////////////////////////////////////////////////
    void ReportSingle(Duration lftime, Duration std_time) {
        std::cout << std::fixed << std::setprecision(2) << std::setw(6);
        const auto width = std::setw(9);

        const double lf_time_μs = (double)lftime.Microseconds();
        const double std_time_μs = (double)std_time.Microseconds();
        const double std_diff = ((std_time_μs / lf_time_μs) - 1) * 100;

        std::cout << "MF time:         " << width << lftime.StrMilli() << std::endl;
        std::cout << "std::queue time: " << width << std_time.StrMilli() << width
                  << " rel imp: " << (std_diff > 0 ? '+' : ' ') << std::setprecision(2) << std_diff << "%"
                  << std::endl;
    }

    //--------------------------------------------------------------//
    void Report(std::pair<Duration, Duration> lfstat, std::pair<Duration, Duration> std_stat) {
        std::cout << std::fixed << std::setprecision(2) << std::setw(6);
        const auto width = std::setw(15);

        const Duration& lftime = lfstat.first;
        const Duration& lfvar = lfstat.second;
        const Duration& std_time = std_stat.first;
        const Duration& std_var = std_stat.second;

        const double lf_time_μs = (double)lfstat.first.Microseconds();
        const double std_time_μs = (double)std_stat.first.Microseconds();
        const double std_diff = ((std_time_μs / lf_time_μs) - 1) * 100;

        std::cout << "MF time:      " << width << lftime.StrMilli() << "   dev: " << width << lfvar.StrMilli()
                  << std::endl;
        std::cout << "Mutex time:   " << width << std_time.StrMilli() << "   dev: " << width << std_var.StrMilli()
                  << width << " rel imp: " << (std_diff > 0 ? '+' : ' ') << std::setprecision(2) << std_diff << "%"
                  << std::endl;
    }

    //////////////////////////////////////////////////////////////////
    class BenchMutexFreeQueue : public ::testing::Test {
    public:
        using value_t = uint32_t;

        BenchMutexFreeQueue() { }

        BenchMutexFreeQueue(const BenchMutexFreeQueue& other) = delete;
        BenchMutexFreeQueue(BenchMutexFreeQueue&& other) noexcept = delete;
        BenchMutexFreeQueue& operator=(const BenchMutexFreeQueue& other) = delete;
        BenchMutexFreeQueue& operator=(BenchMutexFreeQueue&& other) noexcept = delete;

    public:
        static constexpr uint32_t m_sample_size = 1000000;

        static constexpr uint32_t m_nsamples = 5;

        static constexpr std::array<uint32_t, 4> m_nthread_modes = {2, 8, 12, 1024};

        static constexpr uint32_t m_nsections = 1000;
    };

    //--------------------------------------------------------------//
    TEST_F(BenchMutexFreeQueue, single_thread_push) {
        constexpr uint64_t sample_size = 10000000;

        Relax::MutexFreeQueue<value_t> mfqueue;
        Generator<value_t> g(0, 1, sample_size);

        Timestamp start = Timestamp::Now();
        while (!g.empty()) {
            mfqueue.push(g.nextUp());
        }

        Duration lftime = Timestamp::Now() - start;

        std::queue<value_t> std_queue;
        g.restart();

        start = Timestamp::Now();
        while (!g.empty()) {
            std_queue.push(g.nextUp());
        }

        Duration std_time = Timestamp::Now() - start;

        ReportSingle(lftime, std_time);
    }

    //--------------------------------------------------------------//
    TEST_F(BenchMutexFreeQueue, single_thread_push_pop) {
        constexpr uint64_t sample_size = 10000000;

        Relax::MutexFreeQueue<value_t> mfqueue;
        Generator<value_t> g(0, 1, sample_size);

        Timestamp start = Timestamp::Now();
        while (!g.empty()) {
            mfqueue.push(g.nextUp());
        }

        uint32_t ret;
        while (mfqueue.pop(ret))
            ;

        Duration lftime = Timestamp::Now() - start;

        std::queue<value_t> std_queue;
        g.restart();

        start = Timestamp::Now();
        while (!g.empty()) {
            std_queue.push(g.nextUp());
        }

        while (!std_queue.empty()) {
            std_queue.pop();
        }

        Duration std_time = Timestamp::Now() - start;

        ReportSingle(lftime, std_time);
    }

    //--------------------------------------------------------------//
    TEST_F(BenchMutexFreeQueue, seq_push) {
        constexpr uint64_t sample_size = m_sample_size;
        constexpr uint32_t niterations = m_nsamples;

        for (uint32_t nthreads : m_nthread_modes) {
            std::cout << "NThread: " << std::setw(9) << nthreads << std::endl;
            const uint64_t size = sample_size - (sample_size % nthreads);

            auto lfstat = BenchQueueSeqPush<Relax::MutexFreeQueue<value_t>>(size, nthreads, niterations);

            auto std_stat = BenchQueueSeqPush<SillyMutexQueue<value_t>>(size, nthreads, niterations);

            Report(lfstat, std_stat);
        }
    }

    //--------------------------------------------------------------//
    TEST_F(BenchMutexFreeQueue, seq_push_pop) {
        constexpr uint64_t sample_size = m_sample_size;
        constexpr uint32_t niterations = m_nsamples;

        for (uint32_t nthreads : m_nthread_modes) {
            std::cout << "NThread: " << std::setw(9) << nthreads << std::endl;
            const uint64_t size = sample_size - (sample_size % nthreads);

            auto lfstat = BenchQueueSeqPushPop<Relax::MutexFreeQueue<value_t>>(size, nthreads, niterations);

            auto std_stat = BenchQueueSeqPushPop<SillyMutexQueue<value_t>>(size, nthreads, niterations);

            Report(lfstat, std_stat);
        }
    }

    //--------------------------------------------------------------//
    TEST_F(BenchMutexFreeQueue, batched_push_pop) {
        constexpr uint64_t sample_size = m_sample_size;
        constexpr uint32_t niterations = m_nsamples;
        constexpr uint32_t nsections = m_nsections;

        for (uint32_t nthreads : m_nthread_modes) {
            std::cout << "NThread: " << std::setw(9) << nthreads << std::endl;
            const uint64_t size = sample_size - (sample_size % nthreads);

            auto lfstat =
                BenchQueueSectioned<Relax::MutexFreeQueue<value_t>>(size, nthreads, nsections, niterations);

            auto std_stat = BenchQueueSectioned<SillyMutexQueue<value_t>>(size, nthreads, nsections, niterations);

            Report(lfstat, std_stat);
        }
    }

    //--------------------------------------------------------------//
    TEST_F(BenchMutexFreeQueue, parallel_push_pop) {
        constexpr uint64_t sample_size = m_sample_size;
        constexpr uint32_t niterations = m_nsamples;

        for (uint32_t nthreads : m_nthread_modes) {
            std::cout << "NThread: " << std::setw(9) << nthreads << std::endl;
            const uint64_t size = sample_size - (sample_size % nthreads);

            auto lfstat = BenchQueueParPushPop<Relax::MutexFreeQueue<value_t>>(size, nthreads, niterations);

            auto std_stat = BenchQueueParPushPop<SillyMutexQueue<value_t>>(size, nthreads, niterations);

            Report(lfstat, std_stat);
        }
    }

    //////////////////////////////////////////////////////////////////
    class BenchIntrusiveMutexFreeQueue : public ::testing::Test {
    public:
        struct Node {
            Node* m_next = nullptr;
        };

        using value_t = Node;

        BenchIntrusiveMutexFreeQueue() { }

        BenchIntrusiveMutexFreeQueue(const BenchIntrusiveMutexFreeQueue& other) = delete;
        BenchIntrusiveMutexFreeQueue(BenchIntrusiveMutexFreeQueue&& other) noexcept = delete;
        BenchIntrusiveMutexFreeQueue& operator=(const BenchIntrusiveMutexFreeQueue& other) = delete;
        BenchIntrusiveMutexFreeQueue& operator=(BenchIntrusiveMutexFreeQueue&& other) noexcept = delete;

    public:
        static constexpr uint32_t m_sample_size = 1000000;

        static constexpr uint32_t m_nsamples = 5;

        static constexpr std::array<uint32_t, 4> m_nthread_modes = {2, 8, 12, 1024};

        static constexpr uint32_t m_nsections = 1000;
    };

    //--------------------------------------------------------------//
    TEST_F(BenchIntrusiveMutexFreeQueue, seq_push) {
        constexpr uint64_t sample_size = m_sample_size;
        constexpr uint32_t niterations = m_nsamples;

        for (uint32_t nthreads : m_nthread_modes) {
            std::cout << "NThread: " << std::setw(9) << nthreads << std::endl;
            const uint64_t size = sample_size - (sample_size % nthreads);

            auto lfstat =
                BenchQueueSeqPush<Relax::IntrusiveMutexFreeQueue<value_t>, value_t*>(size, nthreads, niterations);

            auto std_stat = BenchQueueSeqPush<SillyMutexQueue<value_t*>>(size, nthreads, niterations);

            Report(lfstat, std_stat);
        }
    }

    //--------------------------------------------------------------//
    TEST_F(BenchIntrusiveMutexFreeQueue, seq_push_pop) {
        constexpr uint64_t sample_size = m_sample_size;
        constexpr uint32_t niterations = m_nsamples;

        for (uint32_t nthreads : m_nthread_modes) {
            std::cout << "NThread: " << std::setw(9) << nthreads << std::endl;
            const uint64_t size = sample_size - (sample_size % nthreads);

            auto lfstat = BenchQueueSeqPushPop<Relax::IntrusiveMutexFreeQueue<value_t>, value_t*>(size,
                                                                                                  nthreads,
                                                                                                  niterations);

            auto std_stat = BenchQueueSeqPushPop<SillyMutexQueue<value_t*>>(size, nthreads, niterations);

            Report(lfstat, std_stat);
        }
    }

    //--------------------------------------------------------------//
    TEST_F(BenchIntrusiveMutexFreeQueue, batched_push_pop) {
        constexpr uint64_t sample_size = m_sample_size;
        constexpr uint32_t niterations = m_nsamples;
        constexpr uint32_t nsections = m_nsections;

        for (uint32_t nthreads : m_nthread_modes) {
            std::cout << "NThread: " << std::setw(9) << nthreads << std::endl;
            const uint64_t size = sample_size - (sample_size % nthreads);

            auto lfstat = BenchQueueSectioned<Relax::IntrusiveMutexFreeQueue<value_t>, value_t*>(size,
                                                                                                 nthreads,
                                                                                                 nsections,
                                                                                                 niterations);

            auto std_stat = BenchQueueSectioned<SillyMutexQueue<value_t*>>(size, nthreads, nsections, niterations);

            Report(lfstat, std_stat);
        }
    }

    //--------------------------------------------------------------//
    TEST_F(BenchIntrusiveMutexFreeQueue, parallel_push_pop) {
        constexpr uint64_t sample_size = m_sample_size;
        constexpr uint32_t niterations = m_nsamples;

        for (uint32_t nthreads : m_nthread_modes) {
            std::cout << "NThread: " << std::setw(9) << nthreads << std::endl;
            const uint64_t size = sample_size - (sample_size % nthreads);

            auto lfstat = BenchQueueParPushPop<Relax::IntrusiveMutexFreeQueue<value_t>, value_t*>(size,
                                                                                                  nthreads,
                                                                                                  niterations);

            auto std_stat = BenchQueueParPushPop<SillyMutexQueue<value_t*>>(size, nthreads, niterations);

            Report(lfstat, std_stat);
        }
    }

    //////////////////////////////////////////////////////////////////

}  // namespace Test
