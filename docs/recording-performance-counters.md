# Recording performance counters

Here, we introduce the interface designed to facilitate the recording of performance counters directly from your C++ application. 

&rarr; [See our single-threaded example: `examples/single_thread.cpp`](../examples/single_thread.cpp)

## 1) Define the counters you want to record
```cpp
#include <perfcpp/event_counter.h>
auto counter_definitions = perf::CounterDefinition{};
auto event_counter = perf::EventCounter{counter_definitions};

event_counter.add({"instructions", "cycles", "branches", "branch-misses", "cache-misses", "cache-references"});
```

## 2) Wrap `start()` and `stop()` around your processing code
```cpp
event_counter.start();

/// ... do some computational work here...

event_counter.stop();
```

## 3) Access the counter
```cpp
/// Calculate the result.
const auto result = event_counter.result();

/// Ask the result for specific counters.
const auto cycles = result.get("cycles");
std::cout << "Took " << cycles.value() << " cycles" << std::endl;

/// Or print all counters on your own.
for (const auto [name, value] : result)
{
    std::cout << "Counter " << name << " = " << value << std::endl;
}

/// Or print in CSV and JSON.
std::cout << result.to_csv(/* delimiter = */'|', /* print header = */ true) << std::endl;
std::cout << result.to_json() << std::endl;
```
---
## Example: Accessing memory in a random fashion
Random access patterns invariably incur high costs, as hardware prefetchers struggle to anticipate such patterns. 
Let's delve into precisely how costly this can be.

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
    event_counter.add({"instructions", "cycles", "branches", "cache-misses", "cycles-per-instruction"});
    
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
    if (!event_counter.start())
    {
        std::cerr << "Could not start performance counters." << std::endl;
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

If you're interested in seeing the outcome with unshuffled `access_pattern_indices`, thereby establishing a predictable access pattern:

    6.85057 instructions per cache line
    8.94096 cycles per cache line
    0.97978 branches per cache line
    0.00748136 cache-misses per cache line
    1.30514 cycles-per-instruction per cache line

---

## Debugging Counter Settings
In certain scenarios, configuring counters can be challenging.
To enable insides into counter configurations, perf provides a debug output option:


    perf --debug perf-event-open [mem] record ...


This command helps visualize configurations for various counters, which is also beneficial for retrieving event codes (for more details, see the [counters documentation](counters.md)).

Similarly, *perf-cpp* includes a debug feature for sampled counters.
To examine the configuration settings—particularly useful if encountering errors during `sampler.start();`—enable debugging in your code as follows:

```cpp
auto config = perf::Config{};
config.is_debug(true);
auto event_counter = perf::EventCounter{ counter_definitions, config };
```

When `is_debug` is set to `true`, *perf-cpp* will display the configuration of all counters upon opening the counters.

