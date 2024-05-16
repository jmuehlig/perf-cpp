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
    cache_lines.resize((1024U * 1024U * 1024U) / sizeof(cache_line));
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
    counter_definitions.add("retired_ops", perf::CounterConfig{11, (1ULL<<19)});
    auto perf_config = perf::SampleConfig{};
    perf_config.precise_ip(0U); /// See https://man7.org/linux/man-pages/man2/perf_event_open.2.html ; may depend on hardware.
    perf_config.period(1000U);

    auto sampler = perf::Sampler{
        counter_definitions,
        "retired_ops",
        perf::Sampler::Type::Timestamp | perf::Sampler::Type::CPU | perf::Sampler::Type::DataSource,
        perf_config};

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
    auto first_time = 0UL;

    for (auto& sample : sampler.result())
    {
        if (first_time == 0UL && sample.timestamp().has_value())
        {
            first_time = sample.timestamp().value();
        }

        auto data_source = "unknown";
        auto mem_level = "unknown";
        if (sample.data_src().has_value())
        {
            if (sample.data_src()->is_load())
            {
                data_source = "load";
            }
            else if (sample.data_src()->is_store())
            {
                data_source = "store";
            }
            else if (sample.data_src()->is_exec())
            {
                data_source = "exec";
            }
            else if (sample.data_src()->is_prefetch())
            {
                data_source = "pref";
            }

            if (sample.data_src()->is_mem_l1())
            {
                mem_level = "L1";
            }
            else if (sample.data_src()->is_mem_l2())
            {
                mem_level = "L2";
            }
            else if (sample.data_src()->is_mem_l3())
            {
                mem_level = "L3";
            }
            else if (sample.data_src()->is_mem_lfb())
            {
                mem_level = "LFB";
            }
            else if (sample.data_src()->is_mem_local_ram())
            {
                mem_level = "RAM";
            }
        }

        if (sample.mode() == perf::Sample::Mode::User)
        {
            std::cout << "[User]   CPU: " << sample.cpu_id().value_or(0U) << " / " << "Time: " << sample.timestamp().value_or(0U) - first_time << " / "  << data_source << " / " << mem_level << std::endl;
        }
        else if (sample.mode() == perf::Sample::Mode::Kernel)
        {
            std::cout << "[Kernel]   CPU: " << sample.cpu_id().value_or(0U) << " / " << "Time: " << sample.timestamp().value_or(0U) - first_time << " / "  << data_source << " / " << mem_level << std::endl;
        }
    }

    sampler.close();

    return 0;
}