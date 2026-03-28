# Performance Metrics

Metrics combine multiple hardware events into calculated values — knowing you had 1 million cache misses is less useful than knowing those misses represent a 5% miss rate.

> [!TIP]
> See the example: **[metric.cpp](https://github.com/jmuehlig/perf-cpp/tree/dev/examples/statistics/metric.cpp)**.
> For inspiration when creating custom metrics, explore the [Likwid project](https://github.com/RRZE-HPC/likwid/tree/master/groups).

---

## Available Built-in Metrics
*perf-cpp* provides ready-to-use metrics that cover the most common performance analysis needs. 
You don't need any special setup—just use them like regular events by adding their names to your `EventCounter`.

| Metric                   | What It Tells You                                                    | Formula                                                |
|--------------------------|----------------------------------------------------------------------|--------------------------------------------------------|
| `gigahertz`              | CPU frequency during measurement                                     | `cycles / (seconds × 10⁹)`                             |
| `cycles-per-instruction` | How many cycles each instruction takes (lower is better)             | `cycles / instructions`                                |
| `instructions-per-cycle` | How many instructions complete per cycle (higher is better)          | `instructions / cycles`                                |
| `cache-hit-ratio`        | Percentage of cache accesses that found data                         | `cache-references / (cache-references + cache-misses)` |
| `cache-miss-ratio`       | Percentage of cache accesses that missed                             | `cache-misses / (cache-references + cache-misses)`     |
| `dTLB-miss-ratio`        | How often data address translation misses                            | `dTLB-load-misses / dTLB-loads`                        |
| `iTLB-miss-ratio`        | How often instruction address translation misses                     | `iTLB-load-misses / iTLB-loads`                        |
| `L1-data-miss-ratio`     | L1 data cache miss rate                                              | `L1-dcache-load-misses / L1-dcache-loads`              |
| `branch-miss-ratio`      | Branch prediction failure rate                                       | `branch-misses / branches`                             |
| `watts-pkg`              | CPU package power consumption in Watts (requires RAPL)               | `energy-pkg / seconds`                                 |
| `watts-cores`            | CPU core power consumption in Watts (requires RAPL)                  | `energy-cores / seconds`                               |
| `watts-ram`              | RAM power consumption in Watts (requires RAPL)                       | `energy-ram / seconds`                                 |

> [!NOTE]
> The `watts-*` metrics require RAPL (Running Average Power Limit) support, which is available on most modern Intel and AMD processors.
> Available RAPL domains vary by hardware: `energy-pkg` is widely supported, `energy-cores` and `energy-ram` depend on the processor model.
> Reading RAPL counters may require `perf_event_paranoid <= 0` or `CAP_SYS_ADMIN`.

## Working with Metrics
Metrics work exactly like regular events — add them, measure, and retrieve results:

```cpp
#include <perfcpp/event_counter.hpp>

auto event_counter = perf::EventCounter{};

/// Add metrics just like regular events.
event_counter.add("cycles-per-instruction");

/// Measure your code.
event_counter.start();
/// ... your code being measured ...
event_counter.stop();

/// Get the calculated metric value.
const auto result = event_counter.result();
const auto cpi = result.get("cycles-per-instruction");

/// Release resources explicitly, or let the destructor handle it.
event_counter.close();
```

The required hardware events (e.g., `cycles` and `instructions` for CPI) are configured automatically if not already being measured.

## Creating Custom Metrics
Built-in metrics cover common cases, but your hardware likely supports hundreds of specialized counters that can yield deeper insights. 
*perf-cpp* gives you two ways to define custom metrics, each with its own strengths.

Custom metrics are registered via the `perf::CounterDefinition` that needs to be passed to the `EventCounter` (&rarr; [read more about adding custom events and metrics](counters.md#the-counterdefinition-system)).

### Formula-Based Metrics
For straightforward calculations, express your metric as a mathematical formula. 
This approach works well when you need to combine a few events with basic arithmetic:

```cpp
auto counter_definition = perf::CounterDefinition{};

/// Define a metric showing what percentage of stalls come from memory loads
counter_definition.add("stalls-by-mem-loads", 
                       "(CYCLE_ACTIVITY_STALLS_LDM_PENDING / CYCLE_ACTIVITY_STALLS_TOTAL) * 100");

auto event_counter = perf::EventCounter{ counter_definition };
event_counter.add("stalls-by-mem-loads");
```

This example uses Intel SkylakeX events to identify memory bottlenecks—adapted from [Likwid's cycle stalls metrics](https://github.com/RRZE-HPC/likwid/blob/master/groups/skylakeX/CYCLE_STALLS.txt).

#### Supported Operations
Your formulas can use:
- **Basic arithmetic**: `+`, `-`, `*`, `/`
- **Scientific notation**: `1E9`, `2.5e-6`
- **Parentheses** for grouping: `(a + b) / c`

#### Built-in Functions
*perf-cpp* provides helper functions to simplify common patterns:

| Function         | Purpose                                 | Example                                |
|------------------|-----------------------------------------|----------------------------------------|
| `ratio(a, b)`    | Safe division with null handling        | `ratio('branch-misses', 'branches')`   |
| `d_ratio(a, b)`  | Same as `ratio()` (compatibility alias) | `d_ratio('misses', 'attempts')`        |
| `sum(a, b, ...)` | Add multiple values together            | `sum('l1_hits', 'l2_hits', 'l3_hits')` |

You can nest functions for complex calculations:

```cpp
/// Calculate miss ratio across all cache levels
counter_definition.add("total-cache-miss-ratio", 
    "ratio("
    "  sum('mem_load_retired.l1_miss', 'mem_load_retired.l2_miss', 'mem_load_retired.l3_miss'),"
    "  sum('mem_load_retired.l1_hit', 'mem_load_retired.l2_hit', 'mem_load_retired.l3_hit')"
    ")"
);
```

> [!IMPORTANT]
> Event names containing operators (like the hyphen in `L1-dcache-misses`) must be wrapped in single quotes: `'L1-dcache-misses'`. 
> This prevents the parser from interpreting the hyphen as subtraction.

### Class-Based Metrics
When your metric needs complex logic, validation, or stateful computation, implement it as a class. 
This approach gives you full control over the calculation:

```cpp
#include <perfcpp/metric/metric.hpp>

class StallsPerCacheMiss final : public perf::Metric
{
public:
    /// Define the metric's identifier
    [[nodiscard]] std::string name() const override 
    {
        return "stalls-per-cache-miss"; 
    }
    
    /// Declare which events this metric needs
    [[nodiscard]] std::vector<std::string> required_counter_names() const override
    { 
        return {"stalls", "cache-misses"}; 
    }
    
    /// Perform the calculation after measurement completes
    [[nodiscard]] std::optional<double> calculate(const CounterResult& result) const override
    {
        const auto stalls = result.get("stalls");
        const auto cache_misses = result.get("cache-misses");

        /// Handle missing data gracefully
        if (stalls.has_value() && cache_misses.has_value())
        {
            /// Avoid division by zero
            if (cache_misses.value() > 0)
            {
                return stalls.value() / cache_misses.value();
            }
        }

        return std::nullopt;  // Return empty if calculation isn't possible
    }
};
```

Register your custom metric class with the counter definition:

```cpp
auto counter_definition = perf::CounterDefinition{};

/// Register using the metric's built-in name
counter_definition.add(std::make_unique<StallsPerCacheMiss>());

/// Or register with a custom name
counter_definition.add("SPCM", std::make_unique<StallsPerCacheMiss>());

/// Use it like any other metric
auto event_counter = perf::EventCounter{ counter_definition };
event_counter.add("stalls-per-cache-miss");  /// Or "SPCM" if using custom name
```

Use class-based metrics when the calculation requires complex logic, validation, or architecture-specific behavior.