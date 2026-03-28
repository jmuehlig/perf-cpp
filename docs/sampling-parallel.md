# Sampling in Parallel

Sampling can target specific threads or CPU cores:

1. **[Per-thread sampling](#per-thread-sampling)**: Each thread gets its own sampler, results are combined afterward.
2. **[Per-CPU-core sampling](#per-cpu-core-sampling)**: Monitor specific CPU cores regardless of which process runs on them.

> [!TIP]
> See the examples: **[multi_thread.cpp](https://github.com/jmuehlig/perf-cpp/tree/dev/examples/sampling/multi_thread.cpp)**, **[multi_cpu.cpp](https://github.com/jmuehlig/perf-cpp/tree/dev/examples/sampling/multi_cpu.cpp)**.

---

## Per-Thread Sampling

`perf::MultiThreadSampler` creates one sampler per thread and combines the results:

```cpp
#include <perfcpp/sampler.hpp>

auto sample_config = perf::SampleConfig{};
sample_config.period(50000U);

const auto count_threads = 4U;
auto sampler = perf::MultiThreadSampler{ count_threads, sample_config };
sampler.trigger("cycles");
sampler.values().timestamp(true).thread_id(true);

/// Optionally open before start() to exclude setup time from measurements.
sampler.open();

/// Start/stop per thread.
auto threads = std::vector<std::thread>{};
for (auto thread_id = 0U; thread_id < count_threads; ++thread_id) {
    threads.emplace_back([thread_id, &sampler]() {
        sampler.start(thread_id);
        /// ... computation here ...
        sampler.stop(thread_id);
    });
}

for (auto& thread : threads) {
    thread.join();
}

/// Combined results across all threads.
for (const auto& record : sampler.result(/* sort by time */ true))
{
    const auto timestamp = record.metadata().timestamp();
    const auto thread_id = record.metadata().thread_id();
    if (timestamp.has_value() && thread_id.has_value())
    {
        std::cout
            << "Time = " << timestamp.value()
            << " | Thread ID = " << thread_id.value() << std::endl;
    }
}

/// Release resources explicitly, or let the destructor handle it.
sampler.close();
```

The output may be something like this:

    Time = 173058802647651 | Thread ID = 62803
    Time = 173058803163735 | Thread ID = 62802
    Time = 173058803625986 | Thread ID = 62804
    Time = 173058804277715 | Thread ID = 62802

---

## Per-CPU-Core Sampling

`perf::MultiCoreSampler` records samples on specified CPU cores, capturing activity from all processes running there.

> [!NOTE]
> This requires `perf_event_paranoid < 1`. See the [perf paranoid setting](perf-paranoid.md).

```cpp
#include <perfcpp/sampler.hpp>

auto sample_config = perf::SampleConfig{};
sample_config.period(50000U);

const auto cpu_core_ids = std::vector<std::uint16_t>{0U, 1U, 2U, 3U};
auto sampler = perf::MultiCoreSampler{ cpu_core_ids, sample_config };
sampler.trigger("cycles");
sampler.values().timestamp(true).cpu_id(true).thread_id(true);

/// Optionally open before start() to exclude setup time from measurements.
sampler.open();

sampler.start();
/// ... computation runs on the monitored cores ...
sampler.stop();

/// Combined results across all monitored cores.
for (const auto& record : sampler.result(/* sort by time */ true))
{
    const auto timestamp = record.metadata().timestamp();
    const auto cpu_id = record.metadata().cpu_id();
    const auto thread_id = record.metadata().thread_id();
    if (timestamp.has_value() && cpu_id.has_value() && thread_id.has_value())
    {
        std::cout
            << "Time = " << timestamp.value()
            << " | CPU ID = " << cpu_id.value()
            << " | Thread ID = " << thread_id.value() << std::endl;
    }
}

/// Release resources explicitly, or let the destructor handle it.
sampler.close();
```

The output may be something like this:

    Time = 173058798201719 | CPU ID = 6 | Thread ID = 62803
    Time = 173058798713083 | CPU ID = 3 | Thread ID = 62802
    Time = 173058799826723 | CPU ID = 3 | Thread ID = 62802
    Time = 173058800426323 | CPU ID = 6 | Thread ID = 62803
    Time = 173058801403355 | CPU ID = 8 | Thread ID = 62804
