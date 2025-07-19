# Metrics
Performance metrics provide essential insights into hardware efficiency by combining multiple hardware events into meaningful calculations. 
A commonly used metric is "Cycles per Instruction" (CPI), which measures how many CPU cycles are required to execute an instruction. 
This metric reveals system efficiency–fewer cycles per instruction indicates better performance.


> [!TIP]
> Our examples include a working code example: **[statistics/metric.cpp](../examples/statistics/metric.cpp)**.
> 
> When [defining custom metrics](#creating-custom-metrics), consider reviewing the comprehensive metric definitions in the [Likwid project](https://github.com/RRZE-HPC/likwid/tree/master/groups).

---
## Table of Contents
- [Built-in Metrics](#built-in-metrics)
- [Using Metrics](#using-metrics)
- [Defining Metrics](#creating-custom-metrics)
---

## Built-in Metrics
*perf-cpp* includes several pre-defined metrics that you can use just like hardware events. 
Simply include their names in your `perf::EventCounter` by treating them as standard events (e.g., `event_counter.add("gigahertz");`):

| Metric name              | Description                                                          |
|--------------------------|----------------------------------------------------------------------|
| `gigahertz`              | Processor frequency during the measurement (`cycles/seconds*1e+09`). |
| `cycles-per-instruction` | Number of cycles required per instruction.                           |
| `instructions-per-cycle` | Number of instructions executed per cycle.                           |
| `cache-hit-ratio`        | Ratio of cache hits to total cache accesses.                         |
| `cache-miss-ratio`       | Ratio of cache misses to total cache accesses.                       |
| `dTLB-miss-ratio`        | Ratio of data TLB misses to data TLB accesses.                       |
| `iTLB-miss-ratio`        | Ratio of instruction TLB misses to instruction TLB accesses.         |
| `L1-data-miss-ratio`     | Ratio of L1 data cache misses to L1 data cache accesses.             |
| `branch-miss-ratio`      | Ratio of branch mispredictions to total executed branches.           |

## Using Metrics
Metrics work exactly like hardware events within the `perf::EventCounter`:

```cpp
#include <perfcpp/event_counter.h>

const auto counter_definition = perf::CounterDefinition{};
auto event_counter = perf::EventCounter{ counter_definition };

/// Add the metric like a "normal" hardware event.
event_counter.add("cycles-per-instruction");

/// Record events and metrics.
event_counter.start();
/// ....
event_counter.stop();
const auto result = event_counter.result();

/// Access the metric like events.
const auto cycles_per_instruction = result.get("cycles-per-instruction");
```

When you use metrics, *perf-cpp* automatically counts the necessary hardware events (such as *cycles* and *instructions* for the *cycles-per-instruction* metric) and presents only the requested metrics and events in the results.

## Creating Custom Metrics
Custom metrics allow you to leverage the specific performance counters available on your hardware platform.

*perf-cpp* offers two approaches for defining custom metrics: *formula-based* definitions using text expressions, or implementing custom classes that inherit from the `perf::Metric` interface.

### Using Formulas
The simplest approach is to define metrics using mathematical expressions that combine hardware events and timing data:

```cpp
auto counter_definition = perf::CounterDefinition{};
counter_definition.add("stalls-by-mem-loads", 
                        "(CYCLE_ACTIVITY_STALLS_LDM_PENDING / CYCLE_ACTIVITY_STALLS_TOTAL) * 100");

auto event_counter = perf::EventCounter{ counter_definition };
```

This example uses Intel SkylakeX architecture events and is adapted from [Likwid](https://github.com/RRZE-HPC/likwid/blob/master/groups/skylakeX/CYCLE_STALLS.txt).

#### Operators
Formulas support the following **mathematical operators**: `+`, `-`, `*`, and `/`.
You can also use **scientific notation** (e.g., `1E5`, `1e-5`) for constants.

#### Functions
Formulas provide built-in functions for common calculations:

| Function                       | Description                                                                                                                                            |
|--------------------------------|--------------------------------------------------------------------------------------------------------------------------------------------------------|
| `ratio(a,b)` or `d_ratio(a,b)` | Calculates the ratio between to operands, e.g., `ratio('branch-misses', 'branches')` calculates the *branch-miss ratio*                                |
| `sum(a,b,...)`                 | Adds together two or more operands, e.g., `sum('mem_load_retired.l1_hit', 'mem_load_retired.l2_hit', 'mem_load_retired.l3_hit')` totals all cache hits |

Functions can be combined within metric expressions:

```cpp
counter_definition.add("cache-miss-ratio", 
                        "ratio( sum('mem_load_retired.l1_miss', 'mem_load_retired.l2_miss', 'mem_load_retired.l3_miss'), sum('mem_load_retired.l1_hit', 'mem_load_retired.l2_hit', 'mem_load_retired.l3_hit') )");
```

> [!NOTE]
> Event names containing **mathematical operators** (such as the `-` in `L1D-misses`) must be **enclosed in single quotes**, e.g., `'L1D-misses'`.

### Implementing Metrics using the Interface
For more complex calculations, you can create custom metric classes by implementing the `perf::Metric` interface:

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

After implementing your custom metric, register it with the `perf::CounterDefinition`:

```cpp
auto counter_definition = perf::CounterDefinition{};
counter_definition.add(std::make_unique<StallsPerCacheMiss>());

auto event_counter = perf::EventCounter{ counter_definition };
event_counter.add("stalls-per-cache-miss");
```

You can also rename the metrics as needed:

```cpp
/// Add the metric using a custom name:
counter_definition.add("SPM", std::make_unique<StallsPerCacheMiss>());

/// Use the custom name:
event_counter.add("SPCM");
```
