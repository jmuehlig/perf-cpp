# Accessing Event Counts without Stopping the Counter

By default, the values of events can only be accessed after the `perf::EventCounter` has been stopped. 
However, on `x86` hardware, it is possible to read events without halting the counter by using the [rdpmc](https://www.felixcloutier.com/x86/rdpmc) instruction. 
This method provides a quick way to obtain interim results during the counting process.

The `perf::EventCounter` class is designed to support both standard and "live" events, allowing configuration of hardware performance counters to access results either "live" (for interim results) or after stopping.
For the latter, see [the recording basics documentation](recording.md).

&rarr; [See code example](../examples/live_events.cpp)

---
## Table of Contents
- [1) Define the Events to Record](#1-define-the-events-to-record)
- [2) Open the Hardware Performance Counters *(optional)*](#2-open-the-hardware-performance-counters-optional)
- [3) Start the Counter and Read Events without Stopping](#3-start-the-counter-and-read-events-without-stopping)
- [4) Stop the Counter and Access "normal" Results](#4-stop-the-counter-and-access-normal-results)
---

## 1) Define the Events to Record
```cpp
#include <perfcpp/event_counter.h>

/// The perf::CounterDefinition object holds all counter names and must be alive when counters are accessed.
auto counters = perf::CounterDefinition{}; 

auto event_counter = perf::EventCounter{counters};
try {
    /// Add events that can be read live.
    event_counter.add_live({"cache-misses", "cache-references"});
    
    /// Add events that can be read after stopping the counter.
    event_counter.add({"instructions", "cycles", "branches", "branch-misses", "cache-misses", "cache-references"});
} catch (std::runtime_error& e) {
    std::cerr << e.what() << std::endl;
}
```

## 2) Open the Hardware Performance Counters *(optional)*
Opening the `EventCounter` configures all hardware performance counters without starting them. 
This step is optional, as the configuration will also occur when the counter is started, if it has not been previously done.

Opening individually is beneficial when measuring time, as it allows the configuration phase to be excluded from the time measurements.

```cpp
try {
    event_counter.open();
} catch (std::runtime_error& e) {
    std::cerr << e.what() << std::endl;
}
```

## 3) Start the Counter and Read Events without Stopping
Reading live events is optimized for efficiency. 
To this end, we avoid expensive memory allocations, such as those for containers that store results.

```cpp
try {
    event_counter.start();
} catch (std::runtime_error& e) {
    std::cerr << e.what() << std::endl;
}

/// Define a container to hold interim values.
/// To prevent memory allocation during the benchmark, allocate these containers beforehand.
auto start_values = std::vector<double>{/* cache-misses */ .0, /* cache-references */ .0};
auto end_values = std::vector<double>{/* cache-misses */ .0, /* cache-references */ .0};

for (auto i = 0U; i < runs; ++i) {
    /// Read the current values before the run begins.
    event_counter.live_results(start_values);
    
    /// ... execute computational work here...
    
    /// Read the current values after the run concludes.
    event_counter.live_results(end_values);
    
    std::cout << "Live Results: "
        << "cache-misses: " << end_values[0U] - start_values[0U] << ","
        << "cache-references: " << end_values[1U] - start_values[1U] << std::endl;
}
```

## 4) Stop the Counter and Access "normal" Results
"Normal" (non-live) events are accessible after stopping the counter through `event_counter.result()`.
For further information, refer to the [recording basics documentation](recording.md).

```cpp
/// Stop the counter after processing.
event_counter.stop();

/// Calculate the result.
const auto result = event_counter.result();

//// Or print the results as table.
std::cout << result.to_string() << std::endl;
```
