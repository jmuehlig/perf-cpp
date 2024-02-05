#include <perfcpp/sampler.h>
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
    auto perf_config = perf::Config{};
    //perf_config.precise_ip(1U); /// See https://man7.org/linux/man-pages/man2/perf_event_open.2.html ; may depend on hardware.
    auto sampler = perf::Sampler{counter_definitions, {"cycles", "L1-dcache-loads", "L1-dcache-load-misses"}, 10000U, perf_config};

    /// Start recording.
    if (!sampler.start())
    {
        std::cerr << "Could not start performance counters: " << sampler.last_error() << std::endl;
        return 1;
    }

    /// Process the data and force the value to be not optimized away by the compiler.
    auto value = 0U;
    for (const auto index : access_pattern_indices)
    {
        value += cache_lines[index].value;
    }
    asm volatile("" : "+r,m"(value) : : "memory");

    /// Stop recording counters.
    sampler.stop();

    struct Event
    {
        struct value
        {
            std::uint64_t value;
            std::uint64_t id;
        };

        std::uint64_t count_members;
        std::array<value, perf::Group::MAX_MEMBERS> values;
    };

    /// Print the performance counters.
    auto last_loads = 0UL;
    auto last_misses = 0UL;
    sampler.for_each_sample([&last_loads, &last_misses](auto* event_addr) {

        /// See PERF_RECORD_SAMPLE from https://man7.org/linux/man-pages/man2/perf_event_open.2.html
        auto *event = reinterpret_cast<Event*>(event_addr);
        std::cout << event->values.at(0).value << " / "  << event->values.at(1).value - last_loads << " / " << event->values.at(2).value - last_misses << std::endl;
        last_loads = event->values.at(1).value;
        last_misses = event->values.at(2).value;
    });

    sampler.close();

    return 0;
}