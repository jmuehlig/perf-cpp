# Metrics
Performance metrics are critical for evaluating the efficiency of computer hardware using specific, user-defined calculations based on hardware events. 
One key metric frequently used is the "Cycles per Instruction" (CPI). 
This metric helps to measure how many CPU cycles are consumed for executiong an instruction, providing insight into the system's efficiency—the fewer the cycles needed per instruction, the more efficient the system.

**Note**: Metrics are not applicable for [sampling](sampling.md) and [live events](recording-live-events.md).

---
## Table of Contents
- [Built-in Metrics](#built-in-metrics)
- [Utilizing Metrics](#utilizing-metrics)
- [Defining Metrics](#creating-custom-metrics)
---

## Built-in Metrics
*perf-cpp* comes pre-equipped with several built-in metrics which can be used analogously to events. 
To employ these metrics, include their names in the `perf::EventCounter` instance as shown in the [Utilizing Metrics](#utilizing-metrics) section:

* `cycles-per-instruction`: Represents the number of cycles required per instruction.
* `cache-hit-ratio`: Indicates the ratio of cache hits to total cache accesses.
* `dTLB-miss-ratio` The ratio of data TLB misses to data TLB accesses.
* `iTLB-miss-ratio` The ratio of instruction TLB misses to instruction TLB accesses.
* `L1-data-miss-ratio`: Reflects the ratio of L1 data cache misses to L1 data cache accesses.

## Utilizing Metrics
Metrics function similarly to hardware events in the  `perf::EventCounter`:
```cpp
#include <perfcpp/event_counter.h>
auto counter_definitions = perf::CounterDefinition{};
auto event_counter = perf::EventCounter{counter_definitions};

event_counter.add("cycles-per-instruction");
```
When metrics are used, *perf-cpp* internally counts the required hardware events (like cycles and instructions for CPI) and displays only the specified metrics and events.

## Creating Custom Metrics
You can create custom metrics based on the hardware counters available on your specific hardware. 
To define these metrics, use the `perf::Metric interface` and adapt it to your hardware characteristics:

```cpp
#include <perfcpp/metric.h>
class StallsPerCacheMiss final : public perf::Metric
{
public:
    /// Provides a name used to access the metric value.
    [[nodiscard]] std::string name() const override 
    {
        return "stalls-per-cache-miss"; 
    }
    
    /// Identifies the necessary hardware events for this metric.
    [[nodiscard]] std::vector<std::string> required_counter_names() const 
    { 
        return {"stalls", "cache-misses"}; 
    }
    
    /// Calculates the metric using the recorded hardware event data.
    /// Calculation happens after stopping the EventCounter.
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
````

### Use defined Metrics
After implementing custom metrics, incorporate them into the `perf::CounterDefinition` to utilize them effectively:

```cpp
auto counter_definitions = perf::CounterDefinition{};
counter_definitions.add(std::make_unique<StallsPerCacheMiss>());
```

You can also rename the metrics as needed:

```cpp
counter_definitions.add("SPM", std::make_unique<StallsPerCacheMiss>());
```

Finally, add the custom metrics to your `perf::EventCounter`:

```cpp
event_counter.add("stalls-per-cache-miss");

/// Or, if you renamed it:
event_counter.add("SPCM");
```