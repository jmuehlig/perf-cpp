#include <perfcpp/event_counter.h>
#include <random>
#include <iostream>
#include <cstdint>
#include <vector>
#include <algorithm>
#include <thread>

struct alignas(64U) cache_line { std::int64_t value; };

int main()
{
    /// Create data to process: Allocate enough cache lines for 256 MB.
    auto cache_lines = std::vector<cache_line>{};
    cache_lines.resize((1024U * 1024U * 256U) / sizeof(cache_line));
    for (auto i = 0U; i < cache_lines.size(); ++i)
    {
        cache_lines[i].value = i;
    }

    /// Create a random access pattern (otherwise the hardware prefetcher will take action).
    auto access_pattern_indices = std::vector<std::uint64_t>{};
    access_pattern_indices.resize(cache_lines.size());
    std::iota(access_pattern_indices.begin(), access_pattern_indices.end(), 0U);
    std::shuffle(access_pattern_indices.begin(), access_pattern_indices.end(), std::mt19937 {std::random_device{}()});

    /// Initialize performance counters.
    auto counter_definitions = perf::CounterDefinition{};

    auto perf = perf::EventCounter{counter_definitions};
    if (!perf.add({"instructions", "cycles", "branches", "cache-misses", "dTLB-miss-ratio", "L1-data-miss-ratio", "cycles-per-instruction"}))
    {
        std::cerr << "Could not add performance counters." << std::endl;
    }

    /// Start recording.
    if (!perf.start())
    {
        std::cerr << "Could not start performance counters." << std::endl;
    }

    /// Process the data and force the value to be not optimized away by the compiler.
    auto value = 0U;
    for (const auto index : access_pattern_indices)
    {
        value += cache_lines[index].value;
    }
    asm volatile("" : "+r,m"(value) : : "memory");

    /// Stop recording counters.
    perf.stop();
    const auto result = perf.result(cache_lines.size());

    /// Print the performance counters.
    for (const auto& [name, value] : result)
    {
        std::cout << value  << " " << name  << std::endl;
    }

    return 0;
}