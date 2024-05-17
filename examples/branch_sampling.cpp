#include <perfcpp/sampler.h>
#include <random>
#include <iostream>
#include <cstdint>
#include <vector>
#include <algorithm>
#include <array>
#include <thread>

struct alignas(64U) cache_line {
    std::array<std::int16_t, 32> _values;
};

int main()
{
    /// Create data to process: Allocate enough cache lines for 256 MB.
    auto cache_lines = std::vector<cache_line>{};
    cache_lines.resize((1024U * 1024U * 256U) / sizeof(cache_line));
    for (auto i = 0U; i < cache_lines.size(); ++i)
    {
        for (auto& value : cache_lines[i]._values)
        {
            value = std::int16_t((std::uintptr_t(&cache_lines) >> 16U) + i);
        }
    }

    /// Create a random access pattern (otherwise the hardware prefetcher will take action).
    auto access_pattern_indices = std::vector<std::uint64_t>{};
    access_pattern_indices.resize(cache_lines.size());
    std::iota(access_pattern_indices.begin(), access_pattern_indices.end(), 0U);
    std::shuffle(access_pattern_indices.begin(), access_pattern_indices.end(), std::mt19937 {std::random_device{}()});

    /// Initialize performance counters.
    auto counter_definitions = perf::CounterDefinition{};
    counter_definitions.add("retired_ops", perf::CounterConfig{11, (1ULL<<19)});
    auto perf_config = perf::SampleConfig{};
    perf_config.precise_ip(0U); /// See https://man7.org/linux/man-pages/man2/perf_event_open.2.html ; may depend on hardware.
    perf_config.period(10000U);

    auto sampler = perf::Sampler{
        counter_definitions,
        "cycles",
        perf::Sampler::Type::Timestamp | perf::Sampler::Type::CPU | perf::Sampler::Type::BranchStack,
        perf_config};

    /// Start recording.
    if (!sampler.start())
    {
        std::cerr << "Could not start performance counters: " << sampler.last_error() << std::endl;
        return 1;
    }

    /// Process the data and force the value to be not optimized away by the compiler.
    auto values = 0U;
    for (const auto index : access_pattern_indices)
    {
        for (const auto value : cache_lines[index]._values)
        {
            if (value == 1337) {
                values += 12U * value;
            }
            else if (value == 5654) {
                values += value * value;
            }
            else if (value % 2 == 0) {
                values += value + 10;
            }
            else if (value % 3 == 0) {
                values += value / 3;
            }
            else if (value % 4 == 0) {
                values += value * 4;
            }
            else if (value % 5 == 0) {
                values += value - 5;
            }
            else if (value % 6 == 0) {
                values += value * 6;
            }
            else if (value % 7 == 0) {
                values += value / 7;
            }
            else if (value % 8 == 0) {
                values += value + 8;
            }
            else if (value % 9 == 0) {
                values += value * 9;
            }
            else if (value % 10 == 0) {
                values += value - 10;
            }
            else if (value % 11 == 0) {
                values += value / 11;
            }
            else if (value % 12 == 0) {
                values += value * 12;
            }
            else if (value % 13 == 0) {
                values += value - 13;
            }
            else if (value % 14 == 0) {
                values += value * 14;
            }
            else if (value % 15 == 0) {
                values += value / 15;
            }
            else if (value % 16 == 0) {
                values += value + 16;
            }
            else if (value % 17 == 0) {
                values += value * 17;
            }
            else if (value % 18 == 0) {
                values += value - 18;
            }
            else if (value % 19 == 0) {
                values += value / 19;
            }
            else if (value % 20 == 0) {
                values += value * 20;
            }
            else {
                values += value;  // Default case
            }
        }
    }
    asm volatile("" : "+r,m"(values) : : "memory");

    /// Stop recording counters.
    sampler.stop();

    /// Print the performance counters.
    auto first_time = 0UL;

    for (auto& sample : sampler.result())
    {
        if (first_time == 0UL && sample.timestamp().has_value())
        {
            first_time = sample.timestamp().value();
        }

        if (sample.mode() == perf::Sample::Mode::User)
        {
            std::cout << "[User]   CPU: " << sample.cpu_id().value_or(0U) << " / " << "Time: " << sample.timestamp().value_or(0U) - first_time << std::endl;
        }
        else if (sample.mode() == perf::Sample::Mode::Kernel)
        {
            std::cout << "[Kernel]   CPU: " << sample.cpu_id().value_or(0U) << " / " << "Time: " << sample.timestamp().value_or(0U) - first_time << std::endl;
        }

        if (sample.branches().has_value())
        {
            for (const auto& branch : sample.branches().value())
            {
                std::cout <<  "  (" << branch.is_mispredicted() << "," << branch.is_predicted() << ") " << branch.instruction_pointer_from() << " -> " << branch.instruction_pointer_to() <<  "(" << branch.cycles() << " cyc)\n";
            }
        }
    }

    std::cout << std::endl;

    sampler.close();

    return 0;
}