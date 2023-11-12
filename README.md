# *perf-cpp*: Performance Counter Wrapper for C++

*perf-cpp* is a lightweight wrapper for accessing CPU *performance counters* straight from C++.
The library defines a set of hard- and software counters and allows you to add your own counters manually or via a list (in CSV format).

Author: Jan Mühlig (`mail@janmuehlig.de`)

## How to use
(See more detailed examples below)
### 1) Define the counters you want to record

    #include <perfcpp/perf.h>
    auto counter_definitions = perf::CounterDefinition{};
    auto perf = perf::Perf{counter_definitions};

    perf.add({"instructions", "cycles", "branches", "branch-misses", "cache-misses", "cache-references"});

### 3) Wrap `start()` and `stop()` around your processing code

    perf.start();

    /// ... do some computational work here...

    perf.stop();

### 4) Access the counter

    /// Calculate the result.
    const auto result = perf.result();

    /// Ask the result for specific counters.
    const auto cycles = result.get("cycles");
    std::cout << "Took " << cycles.value() << " cycles" << std::endl;

    /// Or print all counters.
    for (const auto [name, value] : result)
    {
        std::cout << "Counter " << name << " = " << value << std::endl;
    }

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
      GIT_TAG "main"
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
        perf.add({"instructions", "cycles", "branches", "cache-misses"});
    
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
        const auto result = perf.result();
    
        /// Print the performance counters.
        for (const auto [name, value] : result)
        {
            std::cout << (value / cache_lines.size()) << " " << name << " / cache line"  << std::endl;
        }
    
        /// Calculate cycles per instruction (CPI).
        auto instructions = result.get("instructions");
        auto cycles = result.get("cycles");
        if (instructions.has_value() && cycles.has_value())
        {
            std::cout << (cycles.value() / instructions.value())<< " cycles / instruction"  << std::endl;
        }

        return 0;
    }

The output will be something like that, indicating that we have one cache miss per cache line (as expected):

    6.02599 instructions / cache line
    49.8446 cycles / cache line
    1.00502 branches / cache line
    1.34752 cache-misses / cache line
    8.27161 cycles / instruction

If you are curious how the result would look like if we do **not** shuffle the `access_pattern_indices` and, consequently, create a predictable access pattern:

    6.00469 instructions / cache line
    10.5781 cycles / cache line
    1.00092 branches / cache line
    0.00600791 cache-misses / cache line
    1.76164 cycles / instruction

### Using multiple threads
Performance counters are measured per thread/process. 
If you want to measure multiple threads, you need to `start()` and `stop()` a dedicated `Perf` instance within every thread.
To do so, create a global `Perf` instance and copy it into every thread:

    /// Initialize performance counters.
    auto counter_definitions = perf::CounterDefinition{};
    auto perf = perf::Perf{counter_definitions};
    perf.add({"instructions", "cycles"});

    /// Copy the perf instance for every thread.
    auto thread_local_monitors = std::vector<perf::Perf>{perf, perf};
    auto threads = std::vector<std::thread>{};

    /// Create threads and access the thread-local perfs.
    for (auto i = 0U; i < 2U; ++i)
    {
        threads.emplace_back([i, &thread_local_monitors](){
            /// Start recording counters.
            thread_local_monitors[i].start();

            /// ... do your data processing here ...

            /// Stop recording counters.
            thread_local_monitors[i].stop();
        });
    }

    /// Wait for the threads.
    for (auto& thread : threads)
    {
        thread.join();
    }

Now, you have the performance counters for every thread.
Aggregate them into a single result:

    const auto result = perf::Perf::aggregate(thread_local_monitors);

The `result` can be used like in the first example; in fact, it is the sum of all thread-local performance counters.

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

    counter_definitions.add("llc-load-misses", PERF_TYPE_HW_CACHE, PERF_COUNT_HW_CACHE_LL | (PERF_COUNT_HW_CACHE_OP_READ << 8) | (PERF_COUNT_HW_CACHE_RESULT_MISS << 16));

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

    auto counter_definitions = perf::CounterDefinition{"my_file.csv"};
    counter_definitions.add("llc-load-misses", PERF_TYPE_HW_CACHE, ...);
    auto perf = perf::Perf{counter_definitions};
    perf.add({"instructions", "cycles", "llc-load-misses"});

    ....

    const auto result = perf.result();
    const auto llc_misses = result.get("llc-load-misses");

## Pre-defined counters
The following counters are already defined (see `src/counter_definition.cpp` for details) and available for usage:

    branches
    branch-instructions
    branch-misses
    stalled-cycles-backend
    idle-cycles-backend
    stalled-cycles-frontend
    idle-cycles-frontend

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

## TODOs
* [ ] Add sample-based monitoring
* [ ] Automatically read performance counters provided by the machine