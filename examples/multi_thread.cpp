#include <perfcpp/perf.h>
#include <random>
#include <iostream>
#include <cstdint>
#include <vector>
#include <algorithm>
#include <thread>

struct alignas(64U) cache_line { std::int64_t value; };

int main()
{
    /// Create data to process.
    auto cache_lines = std::vector<cache_line>{};
    cache_lines.resize((1024U * 1024U * 256U) / sizeof(cache_line));
    for (auto i = 0U; i < cache_lines.size(); ++i)
    {
        cache_lines[i].value = i;
    }

    /// Create an access pattern (otherwise the hardware prefetcher will take action).
    auto indices = std::vector<std::uint64_t>{};
    indices.resize(cache_lines.size());
    std::iota(indices.begin(), indices.end(), 0U);
    std::shuffle(indices.begin(), indices.end(), std::mt19937 {std::random_device{}()});

    auto value = 0U;

    /// Initialize performance counters.
    auto counter_definitions = perf::CounterDefinition{};

    auto perf = perf::Perf{counter_definitions};
    perf.add({"instructions", "cycles", "branches", "branch-misses", "cache-misses", "cache-references", "cycles-per-instruction"});

    /// One perf instance for every thread.
    const auto count_threads = 2U;
    auto multithread_perf = perf::PerfMT{std::move(perf), count_threads};
    auto threads = std::vector<std::thread>{};

    for (auto i = 0U; i < count_threads; ++i)
    {
        threads.emplace_back([i, &multithread_perf, &cache_lines, &indices, &value](){
            auto local_value = 0U;

            /// Start recording counters.
            multithread_perf.start(i);

            /// Process the data.
            for (const auto index : indices)
            {
                local_value += cache_lines[index].value;
            }

            /// Stop recording counters.
            multithread_perf.stop(i);

            value += local_value;
        });
    }

    for (auto& thread : threads)
    {
        thread.join();
    }

    /// Print the performance counters.
    auto result = multithread_perf.result(cache_lines.size() * count_threads);
    for (const auto& [name, value] : result)
    {
        std::cout << value  << " " << name  << std::endl;
    }

    return 0;
}