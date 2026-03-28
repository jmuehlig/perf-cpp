# Recording Hardware Events

Record hardware performance counters for specific code regions using `perf::EventCounter`.

> [!NOTE]
> `EventCounter` monitors a single thread. For multi-threaded or multi-core recording, use `MultiThreadEventCounter`, `MultiCoreEventCounter`, or `MultiProcessEventCounter` — see [parallel recording](recording-parallel.md).

> [!TIP]
> See **[single_thread.cpp](https://github.com/jmuehlig/perf-cpp/tree/dev/examples/statistics/single_thread.cpp)** for a full working example.

---

## Basic Lifecycle

Set up an event counter, wrap your code with `start()` / `stop()`, and retrieve the results:

```cpp
#include <perfcpp/event_counter.hpp>

/// Create the counter and add events.
auto event_counter = perf::EventCounter{};
event_counter.add({"instructions", "cycles", "branches", "cache-misses"});

/// Optionally, open counters ahead of time to exclude setup from measurement.
event_counter.open();

/// Measure.
event_counter.start();
/// ... your code here ...
event_counter.stop();

/// Retrieve results.
const auto result = event_counter.result();
```

After `stop()`, you can call `start()` / `stop()` again without re-adding events.

```cpp
/// Release resources explicitly, or let the destructor handle it.
event_counter.close();
```

### Accessing Results

```cpp
/// Query a specific event.
const auto cycles = result.get("cycles");
std::cout << "Took " << cycles.value() << " cycles" << std::endl;

/// Iterate over all results.
for (const auto [name, value] : result)
{
    std::cout << name << " = " << value << std::endl;
}

/// Print as formatted table.
std::cout << result.to_string() << std::endl;

/// Export as CSV or JSON — to string or to file.
std::cout << result.to_csv() << std::endl;
std::cout << result.to_json() << std::endl;
result.to_csv("results.csv");
result.to_json("results.json");
```

## Scheduling Events to Hardware Counters

Physical hardware counters are limited (typically 4–8 per core).
When you request more events than counters, the kernel **multiplexes** — time-sharing counters and scaling results.

By default, *perf-cpp* packs events into as few counters as possible.
You can control this via a scheduling hint in `add()`:

```cpp
event_counter.add({"instructions", "cycles", "branches"},
                  perf::EventCounter::Schedule::Separate);
```

| Schedule Mode | Description |
|---|---|
| `Schedule::Append` | Pack into any counter, using multiplexing. **Default.** |
| `Schedule::Separate` | One event per physical counter — avoids multiplexing. |
| `Schedule::Group` | Force all listed events onto the **same** counter (multiplexed together). |

`add()` throws if the requested scheduling doesn't fit (e.g., too many events to group).

## Binding to a CPU Core or Process

By default, events are counted across all cores the thread runs on, for the calling process only.

```cpp
auto config = perf::Config{};

/// Count only on CPU core 5.
config.cpu_core(5U);
config.cpu_core(perf::CpuCore::Any); /// revert to all cores

/// Monitor a specific process or all processes.
config.process(perf::Process{1337});
config.process(perf::Process::Any);

auto event_counter = perf::EventCounter{ config };
```

> [!NOTE]
> Monitoring other or all processes may require elevated privileges.
> See the [perf paranoid setting](perf-paranoid.md).

> [!TIP]
> Some hardware events (e.g., Intel off-core events) require monitoring all processes on a specific CPU core, as the hardware does not attribute these events to individual processes.

## Detection of Physical Hardware Counters

*perf-cpp* automatically detects the number of physical counters and multiplexing capabilities on most systems.

> [!IMPORTANT]
> If the NMI watchdog is enabled (`cat /proc/sys/kernel/nmi_watchdog` returns `1`), it permanently consumes one hardware counter.
> *perf-cpp* detects this and adjusts automatically.
> To reclaim the counter, disable the watchdog via `echo 0 > /proc/sys/kernel/nmi_watchdog` (requires root).

For unusual hardware where auto-detection fails, specify limits manually:

```cpp
auto config = perf::Config{};
config.num_physical_counters(2U);
config.num_events_per_physical_counter(1U);
auto event_counter = perf::EventCounter{ config };
```

## Further Configuration

| Setting | Default | Description |
|---|---|---|
| `include_child_threads(bool)` | `false` | Also monitor child threads spawned by the recording thread. |
| `include_kernel(bool)` | `true` | Include events from kernel activity. Disable when only user-space matters or [perf paranoid](perf-paranoid.md) restricts access. |
| `include_user(bool)` | `true` | Include events from user-space activity. |
| `include_hypervisor(bool)` | `true` | Include events from hypervisor activity. |
| `include_idle(bool)` | `true` | Include events during CPU idle periods. |
| `include_guest(bool)` | `true` | Include events from guest (VM) activity. |
| `include_host(bool)` | `true` | Include events from host activity. |
| `pinned(bool)` | `false` | Pin events to the CPU, preventing them from being multiplexed off. |

---

## Troubleshooting

Enable debug output to inspect the counter configuration passed to the kernel:

```cpp
auto config = perf::Config{};
config.debug(true);
auto event_counter = perf::EventCounter{ config };
```

This is equivalent to `perf --debug perf-event-open stat -- sleep 1`, which prints the `perf_event_open` arguments for each counter.
Useful for retrieving event codes or diagnosing why a counter fails to open.

---

## Example: Random vs. Sequential Access

This example measures how unpredictable memory access patterns defeat the hardware prefetcher:

```cpp
#include <random>
#include <iostream>
#include <cstdint>
#include <vector>
#include <algorithm>
#include <perfcpp/event_counter.hpp>

/// One cache line per element.
struct alignas(64U) cache_line { std::int64_t value; };

int main()
{
    auto event_counter = perf::EventCounter{};
    event_counter.add({"instructions", "cycles", "cache-misses", "cycles-per-instruction"});

    /// 256 MB of cache lines.
    auto cache_lines = std::vector<cache_line>{};
    cache_lines.resize((1024U * 1024U * 256U) / sizeof(cache_line));
    for (auto i = 0U; i < cache_lines.size(); ++i)
    {
        cache_lines[i].value = i;
    }

    /// Shuffle indices for random access.
    auto indices = std::vector<std::uint64_t>(cache_lines.size());
    std::iota(indices.begin(), indices.end(), 0U);
    std::shuffle(indices.begin(), indices.end(), std::mt19937{std::random_device{}()});

    /// Measure random access.
    event_counter.start();
    auto value = 0ULL;
    for (const auto index : indices)
    {
        value += cache_lines[index].value;
    }
    asm volatile("" : "+r,m"(value) : : "memory");
    event_counter.stop();

    /// Print per-cache-line results.
    const auto result = event_counter.result(cache_lines.size());
    for (const auto [name, val] : result)
    {
        std::cout << val << " " << name << " per cache line" << std::endl;
    }
    
    event_counter.close();
}
```

Random access output — more than one cache miss per line:
```
7.12 instructions per cache line
57.19 cycles per cache line
1.63 cache-misses per cache line
8.03 cycles-per-instruction per cache line
```

Sequential access (without shuffling) — the prefetcher eliminates nearly all misses:
```
6.85 instructions per cache line
8.94 cycles per cache line
0.007 cache-misses per cache line
1.31 cycles-per-instruction per cache line
```
