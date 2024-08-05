#pragma once

#include <future>
#include <list>

#include "utils/utils.h"

namespace Test {
    //////////////////////////////////////////////////////////////////
    template<class T>
    class Generator {
    public:
        typedef T value_type;

        Generator(T start = 0, T step = 0, T limit = 0)
          : m_start(start)
          , m_step(step)
          , m_limit(limit)
          , m_current(start) { }

        Generator& restart() {
            m_current = m_start;
            return *this;
        }

        Generator& reverse() {
            m_current = m_limit - ((m_limit - m_start) % m_step);
            return *this;
        }

        inline T nextUp() {
            T v = m_current;
            m_current += m_step;
            return v;
        }

        inline T nextDown() {
            T v = m_current;
            m_current -= m_step;
            return v;
        }

        inline T get() { return nextUp(); }

        inline bool empty() { return m_current >= m_limit; }

    private:
        const T m_start;
        const T m_step;
        const T m_limit;
        T m_current;
    };

    //////////////////////////////////////////////////////////////////
    /// Do bind all args in callable
    // TODO: template<class T, typename Callable>
    template<class T>
    class GeneratorPreallocedValues {
    public:
        typedef T* value_type;

        GeneratorPreallocedValues(size_t alloc_size = 0)
          : m_alloc_size(alloc_size)
          , m_values(alloc_size)
          , m_next(0) {
            // m_values.reserve(alloc_size);
            // for (size_t i = 0 ; i < alloc_size; ++i) {
            //    m_values.emplace_back(new T());
            // }
        }

        inline GeneratorPreallocedValues<T>& restart() { m_next = 0; }

        inline T* get() {
            assert(m_alloc_size > m_next);
            return &(m_values[m_next++]);
        }

        inline bool empty() { return m_alloc_size == m_next; }

    private:
        const size_t m_alloc_size;

        std::vector<T> m_values;

        size_t m_next;
    };

    //////////////////////////////////////////////////////////////////
    template<typename Callable, typename... Args>
    std::pair<std::vector<std::invoke_result_t<Callable, uint32_t, uint32_t, Args&&...>>, Duration>
    RunThreads(uint32_t nthreads, Callable&& f, Args&&... args) {
        std::list<std::future<std::invoke_result_t<Callable, uint32_t, uint32_t, Args&&...>>> threads;
        Timestamp start = Timestamp::Now();
        for (uint32_t i = 0; i < nthreads; ++i) {
            threads.emplace_back(std::async(std::forward<Callable>(f), i, nthreads, std::forward<Args>(args)...));
        }

        std::vector<std::invoke_result_t<Callable, uint32_t, uint32_t, Args&&...>> res;

        for (uint32_t i = 0; i < nthreads; ++i) {
            res.emplace_back(std::move(threads.front().get()));
            threads.pop_front();
        }

        return {res, Timestamp::Now() - start};
    }

    //////////////////////////////////////////////////////////////////
    inline Duration Deviation(const std::vector<Duration>& samples) {
        uint64_t a = 0;
        uint64_t b = 0;
        for (const auto& sample : samples) {
            a += (sample.Microseconds() * sample.Microseconds());
            b += sample.Microseconds();
        }
        a *= samples.size();
        b *= b;

        double var_raw = ((double)(a - b) / samples.size()) / (samples.size() - 1);

        return Duration(std::sqrt(var_raw));
    }

    //////////////////////////////////////////////////////////////////
    template<class Collection, typename Callable, typename... Args>
    std::pair<Duration, Duration> BenchThreads(uint32_t nthreads,
                                               uint32_t niterations,
                                               Callable&& f,
                                               Args&&... args) {
        std::vector<Duration> samples;
        for (uint32_t iter = 0; iter < niterations; ++iter) {
            Collection collection;
            samples.emplace_back(
                RunThreads(nthreads, std::forward<Callable>(f), std::ref(collection), std::forward<Args>(args)...)
                    .second);
        }

        uint64_t e = 0;
        for (const auto& sample : samples) {
            e += sample.Microseconds();
        }
        e /= samples.size();

        Duration dev;
        if (1 < niterations) {
            dev = Deviation(samples);
        }

        return {Duration(e), dev};
    }

}  // namespace Test