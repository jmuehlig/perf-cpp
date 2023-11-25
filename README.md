# *perf-cpp*: Performance Counter Wrapper for C++

*perf-cpp* is a lightweight wrapper for accessing CPU *performance counters* directly from C++.
The library defines a set of hard- and software counters and allows you to add your own counters manually or via a list (in CSV format).

Author: Jan Mühlig (`mail@janmuehlig.de`)

## How to use
See more detailed examples below and in the `examples/` directory.

### 1) Define the counters you want to record
```cpp
#include <perfcpp/perf.h>
auto counter_definitions = perf::CounterDefinition{};
auto perf = perf::Perf{counter_definitions};

perf.add({"instructions", "cycles", "branches", "branch-misses", "cache-misses", "cache-references"});
```

### 3) Wrap `start()` and `stop()` around your processing code
```cpp
perf.start();

/// ... do some computational work here...

perf.stop();
```

### 4) Access the counter
```cpp
/// Calculate the result.
const auto result = perf.result();

/// Ask the result for specific counters.
const auto cycles = result.get("cycles");
std::cout << "Took " << cycles.value() << " cycles" << std::endl;

/// Or print all counters on your own.
for (const auto [name, value] : result)
{
    std::cout << "Counter " << name << " = " << value << std::endl;
}

/// Or print in CSV and JSON
std::cout << result.to_csv(/* delimiter = */'|', /* print header = */ true) << std::endl;
std::cout << result.to_json() << std::endl;
```

## How to include *perf-cpp* in your project?
### Manual
* Download the source code
* call `cmake .` and `make` within the downloaded folder
* Copy the `include/` directory and the static library `libperf-cpp.a` to your project
* Include the `include/` folder and link the library: `-lperf-cpp`

### Using `CMake` and `ExternalProject`
* Add `include(ExternalProject)` to your `CMakeLists.txt`
* Define an external project:
```
ExternalProject_Add(
  perf-cpp-external
  GIT_REPOSITORY "https://github.com/jmuehlig/perf-cpp"
  GIT_TAG "v0.2.1"
  PREFIX "path/to/your/libs/perf-cpp"
  INSTALL_COMMAND cmake -E echo ""
)
```
* Add `path/to/your/libs/perf-cpp/src/perf-cpp-external/include` to your `include_directories()`
* Add `perf-cpp` to your linked libraries

## Examples
### Accessing memory in a random fashion
Random access patterns are always expensive as the hardware prefetcher is not able to predict a pattern. 
But how expensive is that really? Let's find out.
```cpp
#include <random>
#include <iostream>
#include <cstdint>
#include <vector>
#include <algorithm>
#include <perfcpp/perf.h>

/// We want access one cache line per iteration.
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
    auto perf = perf::Perf{counter_definitions};
    perf.add({"instructions", "cycles", "branches", "cache-misses", "cycles-per-instruction"});

    /// Start recording.
    if (!perf.start())
    {
        std::cerr << "Could not start performance counters." << std::endl;
    }

    /// Process the data and force the value to be not optimized away by the compiler.
    auto value = 0U;
    for (const auto index : access_pattern_indices)
    {
        value += cache_lines[index].value;
    }
    asm volatile("" : "+r,m"(value) : : "memory");

    /// Stop recording counters.
    perf.stop();
    const auto result = perf.result(cache_lines.size());

    /// Print the performance counters.
    for (const auto [name, value] : result)
    {
        std::cout << value << " " << name << std::endl;
    }

    return 0;
}
```

The output will be something like that, indicating that we have one cache miss per cache line (as expected):

    6.02599 instructions
    49.8446 cycles
    1.00502 branches
    1.34752 cache-misses
    8.27161 cycles-per-instruction

If you are curious how the result would look like if we do **not** shuffle the `access_pattern_indices` and, consequently, create a predictable access pattern:

    6.00469 instructions 
    10.5781 cycles
    1.00092 branches
    0.00600791 cache-misses
    1.76164 cycles-per-instruction

### Using multiple threads
Performance counters are measured per thread/process.
If you wish to measure multiple threads, you must measure each thread individually.
To do so, use the early created `perf::Perf` instance to build a global `perf::PerfMT` instance:
```cpp
/// Initialize performance counters.
auto counter_definitions = perf::CounterDefinition{};
auto perf = perf::Perf{counter_definitions};
perf.add({"instructions", "cycles"});

/// Create a perf::PerfMT instance that can be used by multiple threads.
const auto count_threads = 8U;
auto perf_multithread = perf::PerfMT{std::move(perf), count_threads};
auto threads = std::vector<std::thread>{};

/// Create threads and access the PerfMT instance to start/stop counters for every thread.
for (auto thread_id = 0U; thread_id < count_threads; ++thread_id)
{
    threads.emplace_back([thread_id, &perf_multithread](){
        /// Start recording counters.
        perf_multithread.start(thread_id);

        /// ... do your data processing here ...

        /// Stop recording counters.
        perf_multithread.stop(thread_id);
    });
}

/// Wait for the threads.
for (auto& thread : threads)
{
    thread.join();
}

/// Access the entire result.
auto result = multithread_perf.result();
for (const auto& [name, value] : result)
{
    std::cout << value  << " " << name  << std::endl;
}

/// Or ask for a specific thread-local result.
auto thread_0_result = multithread_perf.result_of_thread(0U);
for (const auto& [name, value] : thread_0_result)
{
    std::cout << value  << " " << name  << std::endl;
}
```

### Adding more performance counters
Almost every CPU generation has its own performance counters.
If you want to measure your code on different machines, you may want to add different counters.
The `perf::CounterDefinition` class enables the add/access of different counters.
Basically, there are two options to add more counters:

#### 1) In-code
The `perf::CounterDefinition` interface offers an `add()` function that takes 
* the name of the counter,
* the type of that counter (e.g., `PERF_TYPE_RAW` or `PERF_TYPE_HW_CACHE`),
* and the config id

Example:
```cpp
counter_definitions.add("cycle_activity.stalls_l3_miss", PERF_TYPE_RAW, 0x65306a3);
```

or

```cpp
counter_definitions.add("llc-load-misses", PERF_TYPE_HW_CACHE, PERF_COUNT_HW_CACHE_LL | (PERF_COUNT_HW_CACHE_OP_READ << 8) | (PERF_COUNT_HW_CACHE_RESULT_MISS << 16));
```

#### 2) Using a file
Alternatively, you can use a file in csv-like format with `name,id` in every line. Example:

    cycle_activity.stalls_l1d_miss,0xc530ca3
    cycle_activity.stalls_l2_miss,0x55305a3
    cycle_activity.stalls_l3_miss,0x65306a3

These counters will be added as `PERF_TYPE_RAW` with the given `id`.
The file needs to be provided to `perf::CounterDefinition` when creating it:

    auto counter_definition = perf::CounterDefinition{"my_file.csv"};

#### Recording the new counters
In both scenarios, the counters can be added to the `Perf` instance:
```cpp
auto counter_definitions = perf::CounterDefinition{"my_file.csv"};
counter_definitions.add("llc-load-misses", PERF_TYPE_HW_CACHE, ...);

/// Note, that the (now defined) counter "llc-load-misses" must be also added to the perf instance.
auto perf = perf::Perf{counter_definitions};
perf.add({"instructions", "cycles", "llc-load-misses"});

/// start, compute, and stop...

const auto result = perf.result();
const auto llc_misses = result.get("llc-load-misses");
```

#### How to get _raw_ counter codes?
The easiest way is to use the [libpfm4 (see on GitHub)](https://github.com/wcohen/libpfm4):
* Download library 
* Build executables in `examples/` folder
* Choose a specific counter (from `perf list` on your machine) and get the code using the compiled `check_events` executable:
```
./check_events cycle_activity.stalls_l3_miss
```

The output will guide you the id that can be used as a _raw_  value for the counter.

## Pre-defined counters
The following counters are already defined (see `src/counter_definition.cpp` for details) and available for usage:

    branches OR branch-instructions
    branch-misses
    cache-misses
    cache-references
    cycles OR cpu-cycles
    instructions
    stalled-cycles-backend OR idle-cycles-backend
    stalled-cycles-frontend OR idle-cycles-frontend

    L1-dcache-loads
    L1-dcache-load-misses
    L1-icache-loads
    L1-icache-load-misses
    dTLB-loads
    dTLB-load-misses
    iTLB-loads
    iTLB-load-misses

    cpu-clock
    task-clock
    page-faults
    faults
    major-faults
    minor-faults
    alignment-faults
    emulation-faults
    context-switches
    bpf-output
    cgroup-switches
    cpu-migrations
    migrations

## Metrics
Metrics are described as performance counter calculations.
The most common example is the "Cycles per Instruction" (or CPI) metric: the fewer CPU cycles your code requires per instruction, the better. 
Obviously, the "instructions" and "cycles" counters are required to calculate this measure.

The library pre-defines some metrics that can be used just as counters by adding the names to the `perf::Perf` instance:
```
cycles-per-instruction
cache-hit-ratio
dTLB-miss-ratio
iTLB-miss-ratio
L1-data-miss-ratio
```

When added to the `perf::Perf` instance, each metric will add all required counters to calculate (although the counters will not appear in the result unless you explicitly add them).

### Defining specific Metrics
However, the fascinating metrics will depend on the counters available on certain hardware.
You may use the `perf::Metric` interface to implement your own metrics, based on specific performance counters:
```cpp
#include <perfcpp/metric.h>
class StallsPerCacheMiss final : public perf::Metric
{
public:
    /// Set up a name, can be overriden by the counter definition when adding.
    [[nodiscard]] std::string name() const override 
    {
        return "stalls-per-cache-miss"; 
    }
    
    /// List of counters that need to be measured to calculate this metric.
    [[nodiscard]] std::vector<std::string> required_counter_names() const 
    { 
        return {"stalls", "cache-misses"}; 
    }
    
    /// Calculate the metric.
    [[nodiscard]] std::optional<double> calculate(const CounterResult& result) const
    {
        const auto stalls = result.get("stalls");
        const auto cache_misses = result.get("cache-misses");

        if (stalls.has_value() && cache_misses.has_value())
        {
            return stalls.value() / cache_misses.value();
        }

        return std::nullopt;
    }
};
```

### Measure the specific Metric
Upon implementation, the metric must be added to the counter definition instance:
```cpp
auto counter_definitions = perf::CounterDefinition{};
counter_definitions.add(std::make_unique<StallsPerCacheMiss>());

/// You can also override the name of the metric
counter_definitions.add("SPCM", std::make_unique<StallsPerCacheMiss>());
```

Then, it can be added to the `perf::Perf`-instance:
```cpp
perf.add("stalls-per-cache-miss");

/// Or, in case you have overriden the name:
perf.add("SPCM");
```

## TODOs
* [ ] Add sample-based monitoring
* [ ] Automatically read performance counters provided by the machine