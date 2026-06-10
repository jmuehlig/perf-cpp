# Performance Metrics

Metrics combine multiple hardware events into a single derived value. Knowing that your code produced 1 million cache misses is less useful than knowing they amount to a 5% miss rate.

> [!TIP]
> See the examples: **[metric.cpp](https://github.com/jmuehlig/perf-cpp/tree/dev/examples/statistics/metric.cpp)** (custom metrics) and **[rapl.cpp](https://github.com/jmuehlig/perf-cpp/tree/dev/examples/statistics/rapl.cpp)** (RAPL power counters).
> For inspiration when creating custom metrics, explore the [Likwid project](https://github.com/RRZE-HPC/likwid/tree/master/groups).

---

## Available Built-in Metrics

*perf-cpp* ships with metrics for the most common analysis questions.
They require no setup; add them to your `EventCounter` by name.

| Metric                   | What It Tells You                                                    | Formula                                                |
|--------------------------|----------------------------------------------------------------------|--------------------------------------------------------|
| `gigahertz`              | CPU frequency during measurement                                     | `cycles / (seconds × 10⁹)`                             |
| `cycles-per-instruction` | How many cycles each instruction takes (lower is better)             | `cycles / instructions`                                |
| `instructions-per-cycle` | How many instructions complete per cycle (higher is better)          | `instructions / cycles`                                |
| `cache-hit-ratio`        | Fraction of cache accesses served from cache                         | `(cache-references − cache-misses) / cache-references` |
| `cache-miss-ratio`       | Fraction of cache accesses that missed                               | `cache-misses / cache-references`                      |
| `dTLB-miss-ratio`        | How often data address translation misses                            | `dTLB-load-misses / dTLB-loads`                        |
| `iTLB-miss-ratio`        | How often instruction address translation misses                     | `iTLB-load-misses / iTLB-loads`                        |
| `L1-data-miss-ratio`     | L1 data cache miss rate                                              | `L1-dcache-load-misses / L1-dcache-loads`              |
| `branch-miss-ratio`      | Branch prediction failure rate                                       | `branch-misses / branches`                             |
| `watts-pkg`              | CPU package power consumption in Watts (requires RAPL)               | `energy-pkg / seconds`                                 |
| `watts-cores`            | CPU core power consumption in Watts (requires RAPL)                  | `energy-cores / seconds`                               |
| `watts-ram`              | RAM power consumption in Watts (requires RAPL)                       | `energy-ram / seconds`                                 |

All `*-ratio` metrics return values between `0` and `1`; a 5% miss rate is reported as `0.05`.

> [!NOTE]
> The `watts-*` metrics require RAPL (Running Average Power Limit) support, which is available on most modern Intel and AMD processors.
> Available RAPL domains vary by hardware: `energy-pkg` is widely supported, `energy-cores` and `energy-ram` depend on the processor model.
> Reading RAPL counters may require `perf_event_paranoid <= 0` or `CAP_SYS_ADMIN`.

## Working with Metrics

Metrics behave like regular events: add them by name, measure, and read the result.

```cpp
#include <perfcpp/event_counter.hpp>

auto event_counter = perf::EventCounter{};

/// Add metrics just like regular events.
event_counter.add("cycles-per-instruction");

/// Measure your code.
event_counter.start();
/// ... your code being measured ...
event_counter.stop();

/// Get the calculated metric value; std::nullopt if the required events were not measured.
const auto result = event_counter.result();
const auto cpi = result.get("cycles-per-instruction");

/// Release resources explicitly, or let the destructor handle it.
event_counter.close();
```

*perf-cpp* configures the required hardware events automatically (e.g., `cycles` and `instructions` for CPI) if not already being measured.

## Creating Custom Metrics

*perf-cpp* supports two approaches for defining custom metrics: formula-based and class-based.

Custom metrics are registered via the `perf::CounterDefinition` passed to the `EventCounter` (→ [read more about adding custom events and metrics](counters.md#the-counterdefinition-system)).

### Formula-Based Metrics

For straightforward calculations, express your metric as a mathematical formula:

```cpp
auto counter_definition = perf::CounterDefinition{};

/// Define a metric showing what percentage of stalls come from memory loads
counter_definition.add("stalls-by-mem-loads",
                       "(CYCLE_ACTIVITY_STALLS_LDM_PENDING / CYCLE_ACTIVITY_STALLS_TOTAL) * 100");

auto event_counter = perf::EventCounter{ counter_definition };
event_counter.add("stalls-by-mem-loads");
```

This example uses Intel SkylakeX events to identify memory bottlenecks, adapted from [Likwid's cycle stalls metrics](https://github.com/RRZE-HPC/likwid/blob/master/groups/skylakeX/CYCLE_STALLS.txt).

Every event referenced in a formula must be known to the `CounterDefinition`, either as a built-in event or added beforehand (→ [adding custom events](counters.md#the-counterdefinition-system)).

#### Supported Operations
Formulas can use:
- Basic arithmetic: `+`, `-`, `*`, `/`
- Constants, including scientific notation: `1E9`, `2.5e-6`
- Parentheses for grouping: `(a + b) / c`

#### Built-in Functions
In addition, the following functions are available:

| Function         | Purpose                                               | Example                                |
|------------------|-------------------------------------------------------|----------------------------------------|
| `ratio(a, b)`    | Division that returns `0` if the denominator is `0`   | `ratio('branch-misses', 'branches')`   |
| `d_ratio(a, b)`  | Alias for `ratio()`, named after Linux perf's `d_ratio` | `d_ratio('misses', 'attempts')`      |
| `sum(a, b, ...)` | Add two or more values                                | `sum('l1_hits', 'l2_hits', 'l3_hits')` |

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
> Event names containing operators (like the hyphen in `L1-dcache-misses`) must be wrapped in single quotes or backticks: `'L1-dcache-misses'` or `` `L1-dcache-misses` ``.
> This prevents the parser from interpreting the hyphen as subtraction.
> Underscores and dots are regular identifier characters, so names like `mem_load_retired.l1_miss` work without quotes.

### Class-Based Metrics

Formulas cannot express branching or architecture-specific logic. For such cases, implement the metric as a class derived from `perf::Metric`:

```cpp
#include <perfcpp/metric/metric.hpp>

class StallsPerCacheMiss final : public perf::Metric
{
public:
    /// Define the metric's identifier.
    [[nodiscard]] std::string name() const override
    {
        return "stalls-per-cache-miss";
    }

    /// Declare which events this metric needs.
    [[nodiscard]] std::vector<std::string> required_counter_names() const override
    {
        return {"stalls", "cache-misses"};
    }

    /// Perform the calculation after measurement completes.
    [[nodiscard]] std::optional<double> calculate(const perf::CounterResult& result) const override
    {
        const auto stalls = result.get("stalls");
        const auto cache_misses = result.get("cache-misses");

        /// Both events must have been measured.
        if (stalls.has_value() && cache_misses.has_value())
        {
            /// Avoid division by zero.
            if (cache_misses.value() > 0)
            {
                return stalls.value() / cache_misses.value();
            }
        }

        /// Return empty if the calculation is not possible.
        return std::nullopt;
    }
};
```

Register your custom metric class with the counter definition:

```cpp
auto counter_definition = perf::CounterDefinition{};

/// Register using the name reported by the metric itself.
counter_definition.add(std::make_unique<StallsPerCacheMiss>());

/// Or register under a custom name.
counter_definition.add("SPCM", std::make_unique<StallsPerCacheMiss>());

/// Use it like any other metric.
auto event_counter = perf::EventCounter{ counter_definition };
event_counter.add("stalls-per-cache-miss");  /// Or "SPCM" if registered under a custom name.
```