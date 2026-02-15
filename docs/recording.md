# Counting Hardware Events

This section details how to leverage the *perf-cpp* library to monitor and analyze hardware performance counters directly from your C++ applications. 
The library also supports [multi-threading and multi-CPU counting](recording-parallel.md) and [live access to event counts without stopping the counters](recording-live-events.md).

> [!TIP]
> Our examples include several working code-examples, e.g., **[statistics/single_thread.cpp](../examples/statistics/single_thread.cpp)**.

---
## Table of Contents
- [Setting Up Event Counters](#setting-up-event-counters)
- [Initializing the Hardware Counters *(optional)*](#initializing-the-hardware-counters-optional)
- [Managing Counter Lifecycle](#managing-counter-lifecycle)
- [Retrieving Counter Data](#retrieving-counter-data)
- [Closing the Hardware Counters *(optional)*](#closing-the-hardware-counters-optional)
- [Binding the Event Counter to a Specific CPU Core](#binding-the-event-counter-to-a-specific-cpu-core)
- [Binding the Event Counter to a Specific Process](#binding-the-event-counter-to-a-specific-process)
- [Control Scheduling of Events to Hardware Counters](#control-scheduling-of-events-to-hardware-counters)
- [Adjusting Hardware Settings to the Underlying System](#adjusting-hardware-settings-to-the-underlying-system)
- [Further Configuration Settings](#further-configuration-settings)
- [Example: Analyzing Random Access Patterns](#example-analyzing-random-access-patterns)
- [Troubleshooting Counter Configurations](#troubleshooting-counter-configurations)
---

## Setting Up Event Counters
Define the specific events you wish to record using the `perf::EventCounter` class.
The `perf::EventCounter` instances requires a `perf::CounterDefinition` as a reference, containing all events, their configurations, and names.

```cpp
#include <perfcpp/event_counter.h>
auto event_counter = perf::EventCounter{ };

try {
    event_counter.add({"instructions", "cycles", "branches", "branch-misses", "cache-misses", "cache-references"});
} catch (std::runtime_error& e) {
    std::cerr << e.what() << std::endl;
}
```

> [!IMPORTANT] 
> The `perf::CounterDefinition` instance is used to store event configurations (e.g., names) and passed as a reference.
> Consequently, the instance needs to be alive while using the `EventCounter`.

## Initializing the Hardware Counters *(optional)*
Optionally, preparing the hardware counters ahead of time to exclude configuration time from your measurements, though this is also handled automatically at the start if skipped:

```cpp
try {
    event_counter.open();
} catch (std::runtime_error& e) {
    std::cerr << e.what() << std::endl;
}
```

## Managing Counter Lifecycle
Surround your computational code with `start()` and `stop()` methods to count hardware events:

```cpp
try {
    event_counter.start();
} catch (std::runtime_error& e) {
    std::cerr << e.what() << std::endl;
}

/// ... do some computational work here...

event_counter.stop();
```

## Retrieving Counter Data
Extract and analyze the results from the event counter:

```cpp
/// Retrieve the result.
const auto result = event_counter.result();

/// Query result for specific events.
const auto cycles = result.get("cycles");
std::cout << "Took " << cycles.value() << " cycles" << std::endl;

/// Or, print all counters.
for (const auto [name, value] : result)
{
    std::cout << "Counter " << name << " = " << value << std::endl;
}

//// Or, print the results as table.
std::cout << result.to_string() << std::endl;

/// Or, get as CSV and JSON.
std::cout << result.to_csv(/* delimiter = */'|', /* print header = */ true) << std::endl;
std::cout << result.to_json() << std::endl;
```

## Closing the Hardware Counters *(optional)*
Once you have [initialized](#initializing-the-hardware-counters-optional) the hardware performance counters, you can `start()`, `stop()`, and gather results repeatedly. 
To ultimately release resources such as file descriptors, consider closing the `EventCounter`:

```cpp
event_counter.close();
```

This action is optional and will occur automatically upon object deconstruction if `close()` is not invoked manually.

## Control Scheduling of Events to Hardware Counters
The number of *physical* hardware counters that can count low-level events is limited (around one handful on the most modern CPUs). 
However, many vendors implement *multiplexing*–allowing to schedule multiple events to the same counter.

By default, *perf-cpp* will try to schedule the events to as few physical hardware counters as possible.
However, you can control this scheduling via the `EventCounter::add()` method, providing a schedule hint next to the event name(s), for example:

```cpp
event_counter.add({ "instructions", "cycles",
                    "branches", "dTLB-miss-ratio",
                  }, perf::EventCounter::Schedule::Separate);
```

which will schedule each provided event to a **separate** hardware counter.
*perf-cpp* implements three different scheduling modes:

| Schedule Mode                            | Description                                                                                                                                                                                                        |
|------------------------------------------|--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `perf::EventCounter::Schedule::Separate` | Schedule each event to a separate *physical* hardware counter to avoid multiplexing. If a metric is provided as an event, each counter used to calculate the metric will be placed on a separate hardware counter. |
| `perf::EventCounter::Schedule::Append`   | Schedule each event to any *physical*  hardware counter and make use of multiplexing. This is the **default**.                                                                                                     |
| `perf::EventCounter::Schedule::Group`    | Schedule the list of provided events to the **same** *physical* hardware counter to multiplex the events (this is true for list of events and metrics).                                                            |

`EventCounter::add()` will throw an exception, if the scheduling does not fit (e.g., too many events are requested to group together.)

## Binding the Event Counter to a Specific CPU Core
By default, a `perf::EventCounter` tracks events across all CPU cores on which the associated thread is scheduled, as well as the process that instantiated the counter.
To restrict event counting to a particular CPU core, configure the counter as follows:

```cpp
auto config = perf::Config{};
config.cpu_core(5U); /// Bind to CPU core 5.
```

To revert this and resume counting on all cores the thread executes on:

```cpp
config.cpu_core(perf::CpuCore::Any); /// Count events an all CPU cores the thread is executed on.
```

## Binding the Event Counter to a Specific Process
Similarly, process binding determines which process’s events are monitored. 
By default, `perf::EventCounter` captures only the events triggered by the *calling* process.
You can customize this behavior to:
- Bind to a specific process by PID
- Monitor all processes on the system

> [!NOTE]
> Monitoring other or all processes may require elevated privileges. 
> Refer to the [perf paranoid setting](perf-paranoid.md) for configuration guidance.

The process to monitor can be configured as follows:

```cpp
auto config = perf::Config{};
config.process(perf::Process::Calling); /// Default: Monitor only the calling process.

/// Alternatively:
config.process(perf::Process{1337});    /// Monitor events from process with PID 1337.

/// Alternatively:
config.process(perf::Process::Any);     /// Monitor events from all processes.
```

> [!TIP]
> Certain hardware events (e.g., Intel's off-core events) may require monitoring all processes on a specific CPU core, as the hardware does not attribute these events to individual processes.

## Adjusting Hardware Settings to the Underlying System
Every CPU has a limited number of physical performance counters—special registers that track events. 
Modern processors typically have `4` to `8` counters per core (e.g., see the specs for [Intel Sapphire Rapids](https://github.com/RRZE-HPC/likwid/wiki/SapphireRapids#general-purpose-counters)), and some allow measuring multiple events per counter through time-multiplexing.

*perf-cpp* automatically detects these hardware limits on most systems. 
But if you're working with unusual hardware or embedded systems where auto-detection fails, you can specify the limits manually:

```cpp
auto config = perf::Config{};
config.num_physical_counters(2U);           // This CPU only has 2 hardware counters
config.num_events_per_physical_counter(1U); // Each counter tracks just one event at a time

auto event_counter = perf::EventCounter{ config };
```

## Further Configuration Settings
The `perf::Config` class provides additional settings to control the monitoring scope and behavior:

```cpp
auto config = perf::Config{};
config.include_child_threads(true); /// Also monitor child threads.
config.include_kernel(false);       /// Exclude kernel-activity from monitoring.
config.is_pinned(true);             /// Pin events to the CPU.

auto event_counter = perf::EventCounter{ config };
```

| Setting                       | Default | Description                                                                                                                                                                                                            |
|-------------------------------|---------|------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `include_child_threads(bool)` | `false` | If enabled, child threads spawned by the recording thread will also be monitored.                                                                                                                                      |
| `include_kernel(bool)`        | `true`  | If enabled, events triggered by kernel-activity are included. Disabling this can be useful when only user-space performance matters or when the [perf paranoid setting](perf-paranoid.md) restricts kernel monitoring. |
| `include_user(bool)`          | `true`  | If enabled, events triggered by user-space activity are included.                                                                                                                                                      |
| `include_hypervisor(bool)`    | `true`  | If enabled, events triggered by hypervisor-activity are included.                                                                                                                                                      |
| `include_idle(bool)`          | `true`  | If enabled, events triggered during CPU idle periods are included.                                                                                                                                                     |
| `include_guest(bool)`         | `true`  | If enabled, events triggered by guest (virtual machine) activity are included.                                                                                                                                         |
| `include_host(bool)`          | `true`  | If enabled, events triggered by host activity are included.                                                                                                                                                            |
| `is_pinned(bool)`             | `false` | If enabled, events are kept on the CPU if possible, preventing them from being multiplexed off.                                                                                                                        |

---

## Example: Analyzing Random Access Patterns
Investigate the high costs associated with unpredictable memory access patterns by measuring their impact on hardware prefetching:

```cpp
#include <random>
#include <iostream>
#include <cstdint>
#include <vector>
#include <algorithm>
#include <perfcpp/event_counter.h>

/// We want access one cache line per iteration.
struct alignas(64U) cache_line { std::int64_t value; };

int main()
{
    /// Initialize performance counters.
    auto event_counter = perf::EventCounter{ };
    try {
        event_counter.add({"instructions", "cycles", "branches", "cache-misses", "cycles-per-instruction"});
    } catch (std::runtime_error& e) {
        std::cerr << e.what() << std::endl;
    }
    
    /// Setup random access benchmark.
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

    /// Start recording.
    try {
        event_counter.start();
    } catch (std::runtime_error& e) {
        std::cerr << e.what() << std::endl;
    }

    /// Process the data and force the value to be not optimized away by the compiler.
    auto value = 0ULL;
    for (const auto index : access_pattern_indices)
    {
        value += cache_lines[index].value;
    }
    asm volatile("" : "+r,m"(value) : : "memory");

    /// Stop recording counters and get the result (normalized to the number of accessed cache lines).
    event_counter.stop();
    const auto result = event_counter.result(cache_lines.size());

    /// Print the performance counters.
    for (const auto [name, value] : result)
    {
        std::cout << value << " " << name << " per cache line" << std::endl;
    }

    return 0;
}
```

The output will be something like that, indicating that we have more than one cache miss per cache line:

    7.1214 instructions per cache line
    57.1871 cycles per cache line
    1.02313 branches per cache line
    1.6294 cache-misses per cache line
    8.03031 cycles-per-instruction per cache line

If you're interested in seeing the outcome with not-shuffled `access_pattern_indices`, thereby establishing a predictable access pattern:

    6.85057 instructions per cache line
    8.94096 cycles per cache line
    0.97978 branches per cache line
    0.00748136 cache-misses per cache line
    1.30514 cycles-per-instruction per cache line

---

## Troubleshooting Counter Configurations
Debugging and configuring hardware counters can sometimes be complex. 
Utilize *perf-cpp*'s debugging features to gain insights into the internal workings of performance counters and troubleshoot any configuration issues:

```cpp
auto config = perf::Config{};
config.is_debug(true);

auto event_counter = perf::EventCounter{ config };
```

The idea is borrowed from *Linux Perf*, which can be asked to print counter configurations as follows:
```bash
perf --debug perf-event-open stat -- sleep 1
```

This command helps visualize configurations for various counters, which is also beneficial for retrieving event codes (for more details, see the [counters documentation](counters.md)).

