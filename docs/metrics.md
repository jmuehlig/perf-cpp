# Metrics
Metrics are defined by calculations based on performance counters. 
A prevalent metric is "Cycles per Instruction" (CPI), which gauges efficiency by calculating how many CPU cycles are needed per instruction—the lower the number, the better. 
To compute this metric, the "instructions" and "cycles" counters are essential.

The library comes pre-equipped with several metrics that can be utilized in the same way as counters. 
Simply add the metric names to the `perf::EventCounter` instance to use them:
```
cycles-per-instruction
cache-hit-ratio
dTLB-miss-ratio
iTLB-miss-ratio
L1-data-miss-ratio
```

**Attention**: Metrics will not work with sampling.

## Recording metrics
You can add metrics like counters.
```cpp
#include <perfcpp/event_counter.h>
auto counter_definitions = perf::CounterDefinition{};
auto event_counter = perf::EventCounter{counter_definitions};

event_counter.add({"cycles-per-instruction"});
```

## Defining specific metrics
However, the most intriguing metrics depend on the counters that are available on specific hardware. 
You can use the `perf::Metric` interface to develop your own metrics, tailored to the unique performance counters of your system:

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

### Measure the new metric
Upon implementation, the metric must be added to the counter definition instance:
```cpp
auto counter_definitions = perf::CounterDefinition{};
counter_definitions.add(std::make_unique<StallsPerCacheMiss>());

/// You can also override the name of the metric
counter_definitions.add("SPCM", std::make_unique<StallsPerCacheMiss>());
```

Then, it can be added to the `perf::EventCounter`-instance:
```cpp
event_counter.add("stalls-per-cache-miss");

/// Or, in case you have overriden the name:
event_counter.add("SPCM");
```