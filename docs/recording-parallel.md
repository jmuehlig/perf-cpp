# Recording Hardware Events in Parallel

Performance counters can be recorded per thread, per CPU core, or per process:

1. **[Per-thread counters](#per-thread-counters)**: Each thread gets its own counter, results are combined afterward.
2. **[Inherited counters](#inherited-counters)**: A single counter automatically covers all child threads.
3. **[Per-CPU-core counters](#per-cpu-core-counters)**: Monitor specific CPU cores regardless of which process runs on them.
4. **[Per-process counters](#per-process-counters)**: Monitor specific processes by PID.

> [!TIP]
> See the examples: **[multi_thread.cpp](https://github.com/jmuehlig/perf-cpp/tree/dev/examples/statistics/multi_thread.cpp)**, **[inherit_thread.cpp](https://github.com/jmuehlig/perf-cpp/tree/dev/examples/statistics/inherit_thread.cpp)**, **[multi_cpu.cpp](https://github.com/jmuehlig/perf-cpp/tree/dev/examples/statistics/multi_cpu.cpp)**, **[multi_process.cpp](https://github.com/jmuehlig/perf-cpp/tree/dev/examples/statistics/multi_process.cpp)**.

---

## Per-Thread Counters

`perf::MultiThreadEventCounter` creates one counter per thread and combines the results:

```cpp
#include <perfcpp/event_counter.hpp>

const auto count_threads = 4U;
auto multi_event_counter = perf::MultiThreadEventCounter{count_threads};
multi_event_counter.add({"instructions", "cycles", "cache-misses"});

/// Start/stop per thread.
auto threads = std::vector<std::thread>{};
for (auto thread_id = 0U; thread_id < count_threads; ++thread_id) {
    threads.emplace_back([thread_id, &multi_event_counter]() {
        multi_event_counter.start(thread_id);
        /// ... computation here ...
        multi_event_counter.stop(thread_id);
    });
}

for (auto& thread : threads) {
    thread.join();
}

/// Combined results across all threads.
const auto result = multi_event_counter.result();
for (const auto [name, value] : result)
{
    std::cout << name << " = " << value << std::endl;
}

/// Result for a specific thread.
const auto thread_result = multi_event_counter.result_of_thread(0U);

/// Release resources explicitly, or let the destructor handle it.
multi_event_counter.close();
```

---

## Inherited Counters

A single `perf::EventCounter` with `include_child_threads(true)` automatically monitors all threads spawned after `start()`:

```cpp
#include <perfcpp/event_counter.hpp>

auto config = perf::Config{};
config.include_child_threads(true);

auto event_counter = perf::EventCounter{ config };
event_counter.add({"instructions", "cycles", "cache-misses"});

/// Start before spawning threads.
event_counter.start();

auto threads = std::vector<std::thread>{};
for (auto i = 0U; i < count_threads; ++i) {
    threads.emplace_back([]() {
        /// ... computation here ...
    });
}

for (auto& thread : threads) {
    thread.join();
}

event_counter.stop();

/// Aggregated result across all threads.
const auto result = event_counter.result();
for (const auto [name, value] : result)
{
    std::cout << name << " = " << value << std::endl;
}

/// Release resources explicitly, or let the destructor handle it.
event_counter.close();
```

---

## Per-CPU-Core Counters

`perf::MultiCoreEventCounter` records events on specified CPU cores, capturing activity from all processes running there.

> [!NOTE]
> This requires `perf_event_paranoid < 1`. See the [perf paranoid setting](perf-paranoid.md).

> [!TIP]
> Use `perf::HardwareInfo::all_cpu_cores()` to monitor all available CPU cores without specifying them manually.

```cpp
#include <perfcpp/hardware_info.hpp>
#include <perfcpp/event_counter.hpp>

/// Use all_cpu_cores() to target every core on the system.
const auto cpu_core_ids = perf::HardwareInfo::all_cpu_cores(); // or e.g. {0U, 4U, 8U, 12U}
auto multi_cpu_counter = perf::MultiCoreEventCounter{ cpu_core_ids };
multi_cpu_counter.add({"instructions", "cycles", "cache-misses"});

multi_cpu_counter.start();
/// ... computation runs on the monitored cores ...
multi_cpu_counter.stop();

/// Combined results across all monitored cores.
const auto result = multi_cpu_counter.result();
for (const auto [name, value] : result)
{
    std::cout << name << " = " << value << std::endl;
}

/// Result for a specific core (returns std::optional).
const auto core_result = multi_cpu_counter.result_of_core(12U);

/// Release resources explicitly, or let the destructor handle it.
multi_cpu_counter.close();
```

---

## Per-Process Counters

`perf::MultiProcessEventCounter` records events for specific processes by PID, combining the results:

> [!NOTE]
> Monitoring other processes may require elevated privileges. See the [perf paranoid setting](perf-paranoid.md).

```cpp
#include <perfcpp/event_counter.hpp>

const auto process_ids = std::vector<pid_t>{1234, 5678};
auto multi_process_counter = perf::MultiProcessEventCounter{ process_ids };
multi_process_counter.add({"instructions", "cycles", "cache-misses"});

multi_process_counter.start();
/// ... processes are running ...
multi_process_counter.stop();

/// Combined results across all monitored processes.
const auto result = multi_process_counter.result();
for (const auto [name, value] : result)
{
    std::cout << name << " = " << value << std::endl;
}

/// Result for a specific process (returns std::optional).
const auto process_result = multi_process_counter.result_of_process(1234);

/// Release resources explicitly, or let the destructor handle it.
multi_process_counter.close();
```
