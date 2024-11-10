# Accessing Live Event Counts
The *perf-cpp* library supports reading hardware performance counter values without stopping the counters ("live" events), particularly on `x86` systems using the [rdpmc](https://www.felixcloutier.com/x86/rdpmc) instruction. 
This feature allows for interim results during ongoing computations, ideal for real-time monitoring and adjustments.

The `perf::EventCounter` class is designed to support both standard and "live" events, allowing configuration of hardware performance counters to access results either "live" (for interim results) or after stopping.
For the latter, see [the recording basics documentation](recording.md).

&rarr; [View a working code-example here.](../examples/live_events.cpp)

---
## Table of Contents
- [Setting Up Live Events](#setting-up-live-events)
- [Initializing the Hardware Counters *(optional)*](#initializing-the-hardware-counters-optional)
- [Reading Live Events During Computation](#reading-live-events-during-computation)
- [Finalizing and Retrieving Results](#finalizing-and-retrieving-results)
---

## Setting Up Live Events
Define which events to monitor live and which to read post-computation using the `perf::EventCounter`:

```cpp
#include <perfcpp/event_counter.h>

auto counters = perf::CounterDefinition{}; 
auto event_counter = perf::EventCounter{counters};

try {
    /// Events for live monitoring.
    event_counter.add_live({"cache-misses", "cache-references"});
    
    /// Traditional events for post-processing analysis.
    event_counter.add({"instructions", "cycles", "branches", "branch-misses", "cache-misses", "cache-references"});
} catch (std::runtime_error& e) {
    std::cerr << e.what() << std::endl;
}
```

## Initializing the Hardware Counters *(optional)*
Optionally, preparing the hardware counters ahead of time to exclude configuration time from your measurements, though this is also handled automatically at the start if skipped:

```cpp
try {
    event_counter.open();
} catch (std::runtime_error& e) {
    std::cerr << e.what() << std::endl;
}
```

## Reading Live Events During Computation
The library provides two methods for accessing live events during computation: directly via the `EventCounter` and using a simplified `LiveEventCounter` wrapper.

### Option 1: Direct Access via `EventCounter`
Events added as live events (via `add_live()`) can be directly accessed from the `EventCounter` without stopping.
To be efficient, read live event counts by pre-allocating memory for the results to avoid allocation overheads during critical measurement phases:

```cpp
try {
    event_counter.start();
} catch (std::runtime_error& e) {
    std::cerr << e.what() << std::endl;
}

/// Pre-allocated containers for live results.
auto start_values = std::vector<double>{/* cache-misses */ .0, /* cache-references */ .0};
auto end_values = std::vector<double>{/* cache-misses */ .0, /* cache-references */ .0};

for (auto i = 0U; i < runs; ++i) {
    /// Capture start values.
    event_counter.live_results(start_values); 
    
    /// Computation here...
    
    /// Capture end values after computation.
    event_counter.live_results(end_values);  
    
    std::cout << "Live Results: "
        << "cache-misses: " << end_values[0U] - start_values[0U] << ","
        << "cache-references: " << end_values[1U] - start_values[1U] << std::endl;
}
```

### Option 2: Simplified Access via `LiveEventCounter` Wrapper
The `LiveEventCounter` provides a streamlined method to manage live event monitoring by handling memory management and calculation of differences internally.

```cpp
/// Initiate the LiveEventCounter wrapper before starting.
auto live_event_counter = perf::LiveEventCounter{ event_counter };

try {
    event_counter.start();
} catch (std::runtime_error& e) {
    std::cerr << e.what() << std::endl;
}

for (auto i = 0U; i < runs; ++i) {
    /// Capture start values.
    live_event_counter.start();
    
    /// Computation here...
    
    /// Capture end values after computation.
    live_event_counter.stop();
    
    std::cout << "Live Results: "
        << "cache-misses: " << live_event_counter.get("cache-misses") << ","
        << "cache-references: " << live_event_counter.get("cache-references") << std::endl;
}
```

## Finalizing and Retrieving Results
Upon completion, stop the counters and fetch final results for non-live events:

```cpp
/// Stop the counter after processing.
event_counter.stop();

/// Calculate the result.
const auto result = event_counter.result();

//// Or print the results as table.
std::cout << result.to_string() << std::endl;
```

For further information, refer to the [recording basics documentation](recording.md).
