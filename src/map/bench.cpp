#include <unordered_map>

#include "map.h"
#include "test/test.h"
#include "testgen.h"
#include "utils/utils.h"

#define CHECK_UNO 0
#define MAX_KEY 64

namespace Test {
    //////////////////////////////////////////////////////////////////
    template<class K, class V>
    class TMTSTDMap : public std::map<K, V> {
    public:
        typedef V value_type;

        template<typename... Args>
        std::pair<typename std::map<K, V>::iterator, bool> emplace(const K& key, Args&&... args) {
            std::lock_guard<decltype(m_lock)> g(m_lock);
            return std::map<K, V>::emplace(key, std::forward<Args&&>(args)...);
        }

        size_t erase(K key) {
            std::lock_guard<decltype(m_lock)> g(m_lock);
            return std::map<K, V>::erase(key);
        }

    private:
        std::mutex m_lock;
    };

    //////////////////////////////////////////////////////////////////
    template<class K, class V>
    class TMTSTDUnorderedMap : public std::unordered_map<K, V> {
    public:
        typedef V value_type;

        template<typename... Args>
        std::pair<typename std::unordered_map<K, V>::iterator, bool> emplace(const K& key, Args&&... args) {
            std::lock_guard<decltype(m_lock)> g(m_lock);
            return std::unordered_map<K, V>::emplace(key, std::forward<Args&&>(args)...);
        }

        size_t erase(K key) {
            std::lock_guard<decltype(m_lock)> g(m_lock);
            return std::unordered_map<K, V>::erase(key);
        }

    private:
        std::mutex m_lock;
    };

    //////////////////////////////////////////////////////////////////
    template<class T>
    std::pair<Duration, Duration> BenchMapTemplate(const std::vector<TestCommand>& commands,
                                                   std::vector<typename T::mapped_type>& values,
                                                   uint32_t nthreads,
                                                   uint32_t niterations) noexcept {
        return BenchThreads<T>(nthreads,
                               niterations,
                               [&values, &commands](uint32_t thread_id, uint32_t nthreads, T& map) -> int {
                                   const uint32_t cmd_per_thread = commands.size() / nthreads;
                                   const TestCommand* start_cmd = &(commands[thread_id * cmd_per_thread]);

                                   for (uint32_t i = 0; i < cmd_per_thread; ++i) {
                                       const TestCommand& cmd = start_cmd[i];
                                       if (cmd.m_is_add)
                                           map.emplace(cmd.m_key, values[cmd.m_key]);
                                       else
                                           map.erase(cmd.m_key);
                                   }

                                   return 0;
                               });
    }

    //////////////////////////////////////////////////////////////////
    class BenchMap : public ::testing::Test {
    public:
        template<class... Ts>
        using map_t = Relax::Map<Ts...>;
        using key_t = uint32_t;
        using value_t = TestValue*;

    public:
        BenchMap() { }

        BenchMap(const BenchMap& other) = delete;
        BenchMap(BenchMap&& other) noexcept = delete;
        BenchMap& operator=(const BenchMap& other) = delete;
        BenchMap& operator=(BenchMap&& other) noexcept = delete;

        void report(uint64_t sample_size,
                    Duration gen_time,
                    std::pair<Duration, Duration> intrusive_map_stat,
                    std::pair<Duration, Duration> std_map_stat);

        void run(TestGeneratorBucketed generator, uint32_t sample_size, uint32_t nthreads, uint32_t niterations);
    };

    //--------------------------------------------------------------//
    void BenchMap::report(uint64_t sample_size,
                          Duration gen_time,
                          std::pair<Duration, Duration> intrusive_map_stat,
                          std::pair<Duration, Duration> std_map_stat) {
        (void)sample_size;
        std::cout << std::fixed << std::setprecision(2) << std::setw(6);
        const auto width = std::setw(15);

        std::cout << "Gen time:      " << width << static_cast<double>(gen_time.Milliseconds()) << std::endl;

        const double map_time = (double)intrusive_map_stat.first.Microseconds();
        const double origin_time = (double)std_map_stat.first.Microseconds();
        const double origin_diff = ((origin_time / map_time) - 1) * 100;

        std::cout << "IntMap time:   " << width << intrusive_map_stat.first.Str() << "   dev: " << width
                  << intrusive_map_stat.second.Str() << std::endl;
        std::cout << "std::map time: " << width << std_map_stat.first.Str() << "   dev: " << width
                  << std_map_stat.second.Str() << width << " rel imp: " << (origin_diff > 0 ? '+' : ' ')
                  << std::setprecision(2) << origin_diff << "%" << std::endl;

#if CHECK_UNO
        const double unordered_speed = (double)sample_size / unordered_time.Milliseconds();
        const double unordered_diff = ((map_speed / unordered_speed) - 1) * 100;

        std::cout << "std::uno time: " << width << static_cast<double>(unordered_time.Milliseconds())
                  << " rel imp: " << (unordered_diff > 0 ? '+' : ' ') << std::setprecision(2) << unordered_diff
                  << "%" << std::endl;
#endif
    }

    //--------------------------------------------------------------//
    void BenchMap::run(TestGeneratorBucketed generator,
                       uint32_t sample_size,
                       uint32_t nthreads,
                       uint32_t niterations) {
        std::vector<value_t> values = GenValues<std::remove_pointer<value_t>::type>(sample_size);
        std::vector<TestCommand> sample(sample_size, {0, false});

        Duration gen_time;
        {
            Timestamp start = Timestamp::Now();
            generator(sample, sample_size, MAX_KEY, 1);
            gen_time += (Timestamp::Now() - start);
        }

        auto intrusive_map_stat =
            (1 == nthreads)
                ? BenchMapTemplate<map_t<key_t, value_t>>(sample, values, nthreads, niterations)
                : BenchMapTemplate<map_t<key_t, value_t, std::mutex>>(sample, values, nthreads, niterations);

        auto std_map_stat =
            (1 == nthreads) ? BenchMapTemplate<std::map<key_t, value_t>>(sample, values, nthreads, niterations)
                            : BenchMapTemplate<TMTSTDMap<key_t, value_t>>(sample, values, nthreads, niterations);

#if CHECK_UNO
        unordered_time +=
            (1 == nthreads)
                ? BenchMapTemplate<std::unordered_map<key_t, value_t>>(sample, values, nthreads, niterations)
                : BenchMapTemplate<TMTSTDUnorderedMap<key_t, value_t>>(sample, values, nthreads, niterations);
#endif

        KillValues(values);

        report(sample_size, gen_time, intrusive_map_stat, std_map_stat);
    }

    //--------------------------------------------------------------//

    //////////////////////////////////////////////////////////////////
    TEST_F(BenchMap, bench_add_small) {
        constexpr uint32_t sample_size = 64;
        constexpr uint32_t nthreads = 1;
        constexpr uint32_t niterations = 25000;

        run(AddTestGeneratorBucketed, sample_size, nthreads, niterations);
    }

    TEST_F(BenchMap, bench_add_medium) {
        constexpr uint32_t sample_size = 1024;
        constexpr uint32_t nthreads = 1;
        constexpr uint32_t niterations = 5000;

        run(AddTestGeneratorBucketed, sample_size, nthreads, niterations);
    }

    TEST_F(BenchMap, bench_add_big) {
        constexpr uint32_t sample_size = 100000;
        constexpr uint32_t nthreads = 1;
        constexpr uint32_t niterations = 60;

        run(AddTestGeneratorBucketed, sample_size, nthreads, niterations);
    }

    //--------------------------------------------------------------//
    TEST_F(BenchMap, bench_add_rem_small) {
        constexpr uint32_t sample_size = 64;
        constexpr uint32_t nthreads = 1;
        constexpr uint32_t niterations = 25000;

        run(AddRemoveTestGeneratorBucketed, sample_size, nthreads, niterations);
    }

    TEST_F(BenchMap, bench_add_rem_medium) {
        constexpr uint32_t sample_size = 1024;
        constexpr uint32_t nthreads = 1;
        constexpr uint32_t niterations = 5000;

        run(AddRemoveTestGeneratorBucketed, sample_size, nthreads, niterations);
    }

    TEST_F(BenchMap, bench_add_rem_big) {
        constexpr uint32_t sample_size = 100000;
        constexpr uint32_t nthreads = 1;
        constexpr uint32_t niterations = 250;

        run(AddRemoveTestGeneratorBucketed, sample_size, nthreads, niterations);
    }

    //--------------------------------------------------------------//
    TEST_F(BenchMap, bench_mt_add_small) {
        constexpr uint32_t sample_size = 64;
        constexpr uint32_t nthreads = 8;
        constexpr uint32_t niterations = 5000;

        run(AddTestGeneratorBucketed, sample_size, nthreads, niterations);
    }

    TEST_F(BenchMap, bench_mt_add_medium) {
        constexpr uint32_t sample_size = 1024;
        constexpr uint32_t nthreads = 8;
        constexpr uint32_t niterations = 2000;

        run(AddTestGeneratorBucketed, sample_size, nthreads, niterations);
    }

    TEST_F(BenchMap, bench_mt_add_big) {
        constexpr uint32_t sample_size = 100000;
        constexpr uint32_t nthreads = 8;
        constexpr uint32_t niterations = 16;

        run(AddTestGeneratorBucketed, sample_size, nthreads, niterations);
    }

    //////////////////////////////////////////////////////////////////

}  // namespace Test
