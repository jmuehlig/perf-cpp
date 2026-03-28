# perf-cpp: Hardware Performance Monitoring for C++

**perf-cpp** lets you measure hardware performance counters for specific parts of your code — not the entire program.
Place `start()` and `stop()` around exactly the code you care about.

## Count Hardware Events

Count instructions, cycles, cache misses, and more while your code runs:

```cpp
#include <perfcpp/event_counter.hpp>

auto event_counter = perf::EventCounter{};
event_counter.add({"seconds", "instructions", "cycles", "cache-misses"});

event_counter.start();
code_to_profile(); /// <-- your code here
event_counter.stop();

const auto result = event_counter.result();
for (const auto [event_name, value] : result)
{
    std::cout << event_name << ": " << value << std::endl;
}
```

```
seconds:      0.0955897
instructions: 5.92087e+07
cycles:       4.70254e+08
cache-misses: 1.35633e+07
```

&rarr; [Recording event statistics in detail](recording.md)

## Sample Instructions and Memory Accesses

Record snapshots every *N* events — capturing instruction pointers, timestamps, memory addresses, cache levels, and more:

```cpp
#include <perfcpp/sampler.hpp>

auto sampler = perf::Sampler{};
sampler.trigger("cycles", perf::Period{50000U});
sampler.values()
    .timestamp(true)
    .cpu_id(true)
    .logical_instruction_pointer(true);

sampler.start();
code_to_profile(); /// <-- your code here
sampler.stop();

const auto samples = sampler.result();
samples.to_csv("samples.csv");
```

&rarr; [Sampling in detail](sampling.md)

## What else can perf-cpp do?

- **[Multi-thread and multicore event recording](recording-parallel.md)**: Record events across threads and CPU cores
- **[Multi-thread and multicore sampling](sampling-parallel.md)**: Record events across threads and CPU cores
- **[Metrics](metrics.md)**: Combine counters into ratios like cycles-per-instruction or cache miss rates
- **[Live counters](recording-live-events.md)**: Read counter values without stopping — useful for tight loops
- **[Flamegraphs](sampling-symbols-and-flamegraphs.md)**: Resolve instruction pointers to symbols and generate flamegraphs
- **[CSV export](analyzing-samples-with-csv.md)**: Export samples for analysis with Python, R, or spreadsheets
- **[Memory access analysis](analyzing-memory-access-patterns.md)**: Map sampled memory addresses to your data structures

## Building

```bash
git clone https://github.com/jmuehlig/perf-cpp.git
cd perf-cpp
cmake . -B build
cmake --build build
```

&rarr; [Build options and CMake integration](build.md)
