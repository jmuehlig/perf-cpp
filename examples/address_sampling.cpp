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
    perf_config.precise_ip(1U); /// See https://man7.org/linux/man-pages/man2/perf_event_open.2.html ; may depend on hardware.
    auto sampler = perf::Sampler{counter_definitions, "cycles", perf::Sampler::Type::LogicalMemAddress, 10000U, perf_config};

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

    /// Print the performance counters.
    sampler.for_each_sample([](auto* event_addr) {

        /// See PERF_RECORD_SAMPLE from https://man7.org/linux/man-pages/man2/perf_event_open.2.html
        struct Event
        {
            std::uint64_t addr;
        };

        auto *event = reinterpret_cast<Event*>(event_addr);
        if (event->addr > 0U)
        {
            std::cout << event->addr <<  std::endl;
        }
    });

    sampler.close();

    return 0;
}