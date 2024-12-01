# Counting Hardware Events

This section details how to leverage the *perf-cpp* library to monitor and analyze hardware performance counters directly from your C++ applications. 
The library also supports [multi-threading and multi-CPU counting](recording-parallel.md) and [live access to event counts without stopping the counters](recording-live-events.md).

---
## Table of Contents
- [Setting Up Event Counters](#setting-up-event-counters)
- [Initializing the Hardware Counters *(optional)*](#initializing-the-hardware-counters-optional)
- [Managing Counter Lifecycle](#managing-counter-lifecycle)
- [Retrieving Counter Data](#retrieving-counter-data)
- [Closing the Hardware Counters *(optional)*](#closing-the-hardware-counters-optional)
- [Control Scheduling of Events to Hardware Counters](#control-scheduling-of-events-to-hardware-counters)
- [Example: Analyzing Random Access Patterns](#example-analyzing-random-access-patterns)
- [Troubleshooting Counter Configurations](#troubleshooting-counter-configurations)
---

## Setting Up Event Counters
Define the specific events you wish to record using the `perf::EventCounter` class:

```cpp
#include <perfcpp/event_counter.h>

auto counters = perf::CounterDefinition{}; 
auto event_counter = perf::EventCounter{counters};

try {
    event_counter.add({"instructions", "cycles", "branches", "branch-misses", "cache-misses", "cache-references"});
} catch (std::runtime_error& e) {
    std::cerr << e.what() << std::endl;
}
```

**Note**: The `perf::CounterDefinition` instance is used to store event configurations (e.g., names) and passed as a reference.
Consequently, the instance needs to be alive while using the `EventCounter` ([as described here](counters.md)).

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

| Schedule Mode                            | Description                                                                                                                                                                                   |
|------------------------------------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `perf::EventCounter::Schedule::Separate` | Schedule each event to a separate *physical*  hardware counter. If a metric is provided as an event, each counter used to calculate the metric will be placed on a separate hardware counter. |
| `perf::EventCounter::Schedule::Append`   | Schedule each event to any *physical*  hardware counter and make use of multiplexing. This is the **default**.                                                                                |
| `perf::EventCounter::Schedule::Group`    | Schedule the list of provided events to the **same** *physical*  hardware counter (this is true for list of events and metrics).                                                              |

`EventCounter::add()` will throw an exception, if the scheduling does not fit (e.g., too many events are requested to group together.)

### Adjusting hardware settings to the underlying system
*perf-cpp* cannot identify the underlying hardware settings and assumes **four** groups (i.e., *physical* hardware counters) and **five** events per group.
However, some CPUs (e.g., ARM Cortex-A72) do not implement multiplexing at all.

You can specify the settings using the `perf::Config` configuration as follows:

```cpp
auto config = perf::Config{};
config.max_groups(2U);             /// Only two hardware counters
config.max_counters_per_group(1U); /// Only one event per counter.

auto event_counter = perf::EventCounter{ counter_definitions, config };
```

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
    auto counter_definitions = perf::CounterDefinition{};
    auto event_counter = perf::EventCounter{counter_definitions};
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
        event_counter.start()
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

auto event_counter = perf::EventCounter{ counter_definitions, config };
```

The idea is borrowed from *Linux Perf*, which can be asked to print counter configurations as follows:
```bash
perf --debug perf-event-open stat -- sleep 1
```

This command helps visualize configurations for various counters, which is also beneficial for retrieving event codes (for more details, see the [counters documentation](counters.md)).

