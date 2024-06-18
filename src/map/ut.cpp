#include <gtest/gtest.h>
#include <utils/utils.h>

#include <algorithm>
#include <fstream>
#include <list>
#include <map>
#include <thread>

#include "map.h"
#include "test/test.h"
#include "testgen.h"

namespace Test {
    struct TestValue;
}

#define CHECK_ALWAYS 0

namespace Test {

    //////////////////////////////////////////////////////////////////
    class TestMap : public ::testing::Test {
    public:
        template<class... Ts>
        using map_t = Relax::Map<Ts...>;
        using key_t = uint32_t;
        using value_t = TestValue*;

    public:
        TestMap() { }

        TestMap(const TestMap& other) = delete;
        TestMap(TestMap&& other) noexcept = delete;
        TestMap& operator=(const TestMap& other) = delete;
        TestMap& operator=(TestMap&& other) noexcept = delete;

    public:
        static bool execSample(map_t<key_t, value_t>& intr_map,
                               std::map<key_t, value_t>& std,
                               const std::vector<TestCommand>& sample,
                               std::vector<value_t>& values,
                               std::vector<std::pair<bool, bool>>& returns);

        void run(TestGeneratorBucketed generator, uint32_t sample_size, uint32_t niterations, uint32_t ntreads);

        void run_custom(const std::vector<TestCommand>& sample);

    private:
        static bool check(std::map<key_t, value_t>& origin,
                          map_t<key_t, value_t>& tested,
                          std::vector<std::pair<bool, bool>>* vreturns,
                          uint32_t vreturns_size);

        static void dump(uint32_t id, const std::vector<TestCommand>* vsamples, uint32_t vsamples_size);
    };

    //--------------------------------------------------------------//
    bool TestMap::execSample(map_t<key_t, value_t>& intr_map,
                             std::map<key_t, value_t>& std,
                             const std::vector<TestCommand>& sample,
                             std::vector<value_t>& values,
                             std::vector<std::pair<bool, bool>>& returns) {
        const size_t size = sample.size();

        for (uint32_t i = 0; i < size; ++i) {
            const TestCommand& cmd = sample[i];
            returns[i].first = cmd.Exec(std, values[cmd.m_key]);
            returns[i].second = cmd.Exec(intr_map, values[cmd.m_key]);

#if CHECK_ALWAYS
            if (!check(standard, tested, &returns, 1)) {
                dump(0, &sample, 1);
                exit(666);
                return false;
            }
#endif
        }

        if (!check(std, intr_map, &returns, 1)) {
            dump(0, &sample, 1);
            exit(666);
            return false;
        }

        return true;
    }

    //--------------------------------------------------------------//
    void TestMap::run(TestGeneratorBucketed generator,
                      uint32_t sample_size,
                      uint32_t niterations,
                      uint32_t nthreads) {
        auto results =
            RunThreads(nthreads, [generator, niterations, sample_size](uint32_t id, uint32_t nthreads) -> bool {
                (void)id;
                (void)nthreads;
                bool result = true;

                std::vector<value_t> values = GenValues<std::remove_pointer_t<value_t>>(sample_size);
                std::vector<TestCommand> sample(sample_size, {0, 0});
                std::vector<std::pair<bool, bool>> returns(sample_size, {0, 0});

                map_t<key_t, value_t> tested;
                std::map<key_t, value_t> standard;

                for (uint32_t iteration = 0; iteration < niterations; ++iteration) {
                    generator(sample, sample_size, 64, 1);

                    result &= execSample(tested, standard, sample, values, returns);

                    tested.clear();
                    standard.clear();
                }

                KillValues(values);

                return result;
            });

        for (const auto& r : results.first) {
            ASSERT_TRUE(r);
        }
    }

    //--------------------------------------------------------------//
    void TestMap::run_custom(const std::vector<TestCommand>& sample) {
        const size_t size = sample.size();
        std::vector<value_t> values = GenValues<std::remove_pointer_t<value_t>>(64);
        std::vector<std::pair<bool, bool>> returns(size, {0, 0});

        map_t<key_t, value_t> tested;
        std::map<key_t, value_t> standard;

        execSample(tested, standard, sample, values, returns);

        tested.clear();
        standard.clear();

        KillValues(values);
    }

    //--------------------------------------------------------------//
    bool TestMap::check(std::map<key_t, value_t>& origin,
                        map_t<key_t, value_t>& tested,
                        std::vector<std::pair<bool, bool>>* vreturns,
                        uint32_t vreturns_size) {
        for (uint32_t v = 0; v < vreturns_size; ++v) {
            const std::vector<std::pair<bool, bool>>& returns = vreturns[v];
            const size_t size = returns.size();
            for (size_t i = 0; i < size; ++i) {
                if (returns[i].first != returns[i].second) {
                    return false;
                }
            }
        }

        const size_t size = origin.size();
        if (origin.size() != tested.size()) {
            return false;
        }

        std::vector<std::pair<key_t, value_t>> origin_v(origin.begin(), origin.end());
        std::vector<std::pair<key_t, value_t>> tested_v(tested.begin(), tested.end());
        if (origin_v.size() != tested_v.size()) {
            return false;
        }
        assert(origin_v.size() == tested_v.size());

        std::sort(origin_v.begin(), origin_v.end());
        // tested_v should be already sorted
        // std::sort(tested_v.begin(), tested_v.end());

        bool flag = true;

        for (uint32_t i = 0; i < size; ++i) {
            flag &= (origin_v[i] == tested_v[i]);
        }

        return flag && tested.checkRB();
    }

    //--------------------------------------------------------------//
    void TestMap::dump(uint32_t id, const std::vector<TestCommand>* vsamples, uint32_t vsamples_size) {
        std::setfill('0');
        std::setw(3);
        for (uint32_t i = 0; i < vsamples_size; ++i) {
            std::string filename = "dump" + std::to_string(id) + "_" + std::to_string(i) + ".dump";
            std::ofstream file;
            file.open(filename);

            for (auto cmd : vsamples[i]) {
                const char* const op = (cmd.m_is_add) ? "ADD" : "RMV";
                file << std::string(op) << " " << id << " " << i << "/n";
            }
            file.close();
        }
    }

    //////////////////////////////////////////////////////////////////
    TEST_F(TestMap, brut_add_64) {
        constexpr uint32_t sample_size = 64;
        constexpr uint32_t niterations = 10000;
        constexpr uint32_t nthreads = 12;

        run(AddTestGeneratorBucketed, sample_size, niterations, nthreads);
    }

    //--------------------------------------------------------------//
    TEST_F(TestMap, brut_add_big_sample) {
        constexpr uint32_t sample_size = 10000;
        constexpr uint32_t niterations = 50;
        constexpr uint32_t nthreads = 12;

        run(AddTestGeneratorBucketed, sample_size, niterations, nthreads);
    }

    //////////////////////////////////////////////////////////////////
    TEST_F(TestMap, brut_add_remove_small_sample) {
        constexpr uint32_t sample_size = 256;
        constexpr uint32_t niterations = 5000;
        constexpr uint32_t nthreads = 12;

        run(AddRemoveTestGeneratorBucketed, sample_size, niterations, nthreads);
    }

    //--------------------------------------------------------------//
    TEST_F(TestMap, brut_add_remove_big_sample) {
        constexpr uint32_t sample_size = 100000;
        constexpr uint32_t niterations = 10;
        constexpr uint32_t nthreads = 12;

        run(AddRemoveTestGeneratorBucketed, sample_size, niterations, nthreads);
    }

    //////////////////////////////////////////////////////////////////
    //                           custom tests                       //
    //////////////////////////////////////////////////////////////////
    TEST_F(TestMap, add1) {
        std::vector<TestCommand> sample = {
            {37, true},
            {21, true},
            {20, true},
            {38, true},
            {14, true},
            {45, true},
            {18, true},
            { 9, true},
            {57, true},
            { 6, true},
        };

        run_custom(sample);
    }

    //--------------------------------------------------------------//
    TEST_F(TestMap, add_rem1) {
        std::vector<TestCommand> sample = {
            {36,  true},
            {44,  true},
            {17,  true},
            {31,  true},
            {40,  true},
            {58,  true},
            {42,  true},
            {40, false},
            {18,  true},
            {14,  true},
        };

        run_custom(sample);
    }

    //--------------------------------------------------------------//
    TEST_F(TestMap, add_rem2) {
        std::vector<TestCommand> sample = {
            {32,  true},
            {29,  true},
            { 2,  true},
            {61,  true},
            {62,  true},
            {57,  true},
            {62, false},
            {10,  true},
            { 5,  true},
            { 4,  true},
        };

        run_custom(sample);
    }

    //--------------------------------------------------------------//
    TEST_F(TestMap, add_rem3) {
        std::vector<TestCommand> sample = {
            { 9,  true},
            {60,  true},
            {18,  true},
            {32,  true},
            { 9, false},
            { 7,  true},
            {41,  true},
            {36,  true},
            { 0,  true},
            {43,  true},
        };

        run_custom(sample);
    }

    //--------------------------------------------------------------//
    TEST_F(TestMap, add_rem4) {
        std::vector<TestCommand> sample = {
            {61,  true},
            {48,  true},
            {31,  true},
            {15,  true},
            {25,  true},
            {48, false},
            {41,  true},
            { 5,  true},
            {25, false},
            { 7,  true},
        };

        run_custom(sample);
    }

    //--------------------------------------------------------------//
    TEST_F(TestMap, add_rem5) {
        std::vector<TestCommand> sample = {
            {29,  true},
            {51,  true},
            {40,  true},
            {45,  true},
            { 2,  true},
            { 0,  true},
            {13,  true},
            {51, false},
            {30,  true},
            {45, false},
        };

        run_custom(sample);
    }

    //--------------------------------------------------------------//
    TEST_F(TestMap, add_rem6) {
        std::vector<TestCommand> sample = {
            {35,  true},
            { 8,  true},
            {49,  true},
            {19,  true},
            {17,  true},
            { 1,  true},
            {12,  true},
            {45,  true},
            {25,  true},
            {47,  true},
            { 0,  true},
            {20,  true},
            {30,  true},
            {57,  true},
            {31,  true},
            { 1, false},
            {61,  true},
            {51,  true},
            { 8, false},
            {44,  true},
        };

        run_custom(sample);
    }

    //--------------------------------------------------------------//

}  // namespace Test
