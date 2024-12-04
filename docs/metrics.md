# Metrics
Performance metrics are critical for evaluating the efficiency of computer hardware using specific, user-defined calculations based on hardware events. 
One key metric frequently used is the "Cycles per Instruction" (CPI). 
This metric helps to measure how many CPU cycles are consumed for executiong an instruction, providing insight into the system's efficiency—the fewer the cycles needed per instruction, the more efficient the system.

**Hint**: When [defining custom metrics](#creating-custom-metrics), you should take a look at the list of metrics in the [Likwid project](https://github.com/RRZE-HPC/likwid/tree/master/groups).

**Note**: Metrics are not applicable for [live events](recording-live-events.md).

---
## Table of Contents
- [Built-in Metrics](#built-in-metrics)
- [Utilizing Metrics](#utilizing-metrics)
- [Defining Metrics](#creating-custom-metrics)
---

## Built-in Metrics
*perf-cpp* comes pre-equipped with several built-in metrics which can be used analogously to events. 
To employ these metrics, include their names in the `perf::EventCounter` instance as shown in the [Utilizing Metrics](#utilizing-metrics) section:

| Metric name              | Description                                                           |
|--------------------------|-----------------------------------------------------------------------|
| `gigahertz`              | Processor speed during the measurement (`cycles/seconds*1e+09`).      |
| `cycles-per-instruction` | Represents the number of cycles required per instruction.             |
| `instructions-per-cycle` | Represents the number of instructions executed per cycle.             |
| `cache-hit-ratio`        | Indicates the ratio of cache hits to total cache accesses.            |
| `cache-miss-ratio`       | Indicates the ratio of cache misses to total cache accesses.          |
| `dTLB-miss-ratio`        | The ratio of data TLB misses to data TLB accesses.                    |
| `iTLB-miss-ratio`        | The ratio of instruction TLB misses to instruction TLB accesses.      |
| `L1-data-miss-ratio`     | Reflects the ratio of L1 data cache misses to L1 data cache accesses. |
| `branch-miss-ratio`      | Reflects the ratio of branch misses to executed branches.             |

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
Metrics are often based on the performance counters supported by the underlying hardware.
You can create custom metrics to tailor them to your specific hardware. 

**Hint**: The [Likwid project](https://github.com/RRZE-HPC/likwid/tree/master) gives an excellent and extensive list of available metrics for various CPUs. 
Take a look at their [groups/ directory](https://github.com/RRZE-HPC/likwid/tree/master/groups).

There are two ways to define custom metrics.

### Using Formulas
The first option is to express a metric as a calculation of several hardware and time events, for example:

```cpp
auto counter_definitions = perf::CounterDefinition{};
counter_definitions.add("stalls-by-mem-loads", "(CYCLE_ACTIVITY_STALLS_LDM_PENDING/CYCLE_ACTIVITY_STALLS_TOTAL)*100");
```

The formular can use the following operators: `+`, `-`, `*`, and `/`.

**Note**: In formulas, event names that contain *operators* (like `-` in `L1D-misses`) need to be **escaped** using single quotes, e.g., `'L1D-misses'`.

The example depends on events from the Intel SkylakeX architecture and is taken from [Likwid](https://github.com/RRZE-HPC/likwid/blob/master/groups/skylakeX/CYCLE_STALLS.txt).

### Implementing Metrics using the Interface
The second option is to define metrics by implementing the `perf::Metric` interface, for example:

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

After implementing custom metrics, incorporate them into the `perf::CounterDefinition` to utilize them effectively:

```cpp
auto counter_definitions = perf::CounterDefinition{};
counter_definitions.add(std::make_unique<StallsPerCacheMiss>());
```

You can also rename the metrics as needed:

```cpp
counter_definitions.add("SPM", std::make_unique<StallsPerCacheMiss>());
```

### Record custom Metrics
To record custom defined metrics (via formula  or `perf::Metric` interface), add the custom metrics to the `perf::EventCounter`:

```cpp
event_counter.add("stalls-per-cache-miss");

/// Or, if you renamed it:
event_counter.add("SPCM");
```