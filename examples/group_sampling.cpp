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

    struct Event
    {
        struct value
        {
            std::uint64_t value;
            std::uint64_t id;
            std::uint64_t lost;
        };

        std::uint64_t timestamp;

        std::uint32_t cpu_id;
        std::uint32_t res;
/*
        std::uint64_t count_members;
        value l1_dcache_loads;
        value cycles;
        value l1_dcache_load_misses;
*/
        std::uint64_t data_source;
    };

    /// Print the performance counters.
    auto last_loads = 0UL;
    auto last_misses = 0UL;
    auto first_time = 0UL;
    sampler.for_each_sample([&last_loads, &last_misses, &first_time](auto* event_addr, perf::Sampler::SampleMode type) {
        /// See PERF_RECORD_SAMPLE from https://man7.org/linux/man-pages/man2/perf_event_open.2.html
        auto *event = reinterpret_cast<Event*>(event_addr);

        if (first_time == 0UL)
        {
            first_time = event->timestamp;
        }

        auto data_source = "unknown";
        if (event->data_source & PERF_MEM_OP_LOAD)
        {
            data_source = "load";
        }
        else if (event->data_source & PERF_MEM_OP_STORE)
        {
            data_source = "store";
        }
        else if (event->data_source & PERF_MEM_OP_EXEC)
        {
            data_source = "exec";
        }
        else if (event->data_source & PERF_MEM_OP_PFETCH)
        {
            data_source = "pref";
        }

        auto mem_level = "unknown";
        if (event->data_source & (PERF_MEM_LVL_L1 << PERF_MEM_LVL_SHIFT))
        {
            mem_level = "L1";
        }
        else if (event->data_source & (PERF_MEM_LVL_L2 << PERF_MEM_LVL_SHIFT))
        {
            mem_level = "L2";
        }
        else if (event->data_source & (PERF_MEM_LVL_L3 << PERF_MEM_LVL_SHIFT))
        {
            mem_level = "L3";
        }
        else if (event->data_source & (PERF_MEM_LVL_LFB << PERF_MEM_LVL_SHIFT))
        {
            mem_level = "LFB";
        }
        else if (event->data_source & (PERF_MEM_LVL_LOC_RAM << PERF_MEM_LVL_SHIFT))
        {
            mem_level = "RAM";
        }

        /*
        if (type == perf::Sampler::SampleMode::User)
        {
            std::cout << "[User]   CPU: " << event->cpu_id << " / " << "Time: " << event->timestamp - first_time << " / " << event->cycles.value << " / "  << event->l1_dcache_loads.value - last_loads << " / " << event->l1_dcache_load_misses.value - last_misses << " / " << data_source << std::endl;
        }
        else if (type == perf::Sampler::SampleMode::Kernel)
        {
            std::cout << "[Kernel] CPU: " << event->cpu_id << " / " << "Time: " << event->timestamp - first_time << " / " << event->cycles.value << " / "  << event->l1_dcache_loads.value - last_loads << " / " << event->l1_dcache_load_misses.value - last_misses  << " / " << data_source << std::endl;
        }
         */

        if (type == perf::Sampler::SampleMode::User)
        {
            std::cout << "[User]   CPU: " << event->cpu_id << " / " << "Time: " << event->timestamp - first_time << " / " << event->data_source << " / "  << data_source << " / " << mem_level << std::endl;
        }
        else if (type == perf::Sampler::SampleMode::Kernel)
        {
            std::cout << "[Kernel] CPU: " << event->cpu_id << " / " << "Time: " << event->timestamp - first_time << " / " << event->data_source << " / "  << data_source << " / " << mem_level << std::endl;
        }


        //last_loads = event->l1_dcache_loads.value;
        //last_misses = event->l1_dcache_load_misses.value;
    });

    sampler.close();

    return 0;
}