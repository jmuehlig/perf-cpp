# Accessing Live Event Counts

Read hardware counter values without stopping the counters, using the [rdpmc](https://www.felixcloutier.com/x86/rdpmc) instruction on `x86` systems.
This is useful for measuring individual iterations or phases within a running computation.

For standard (non-live) recording, see [recording basics](recording.md).

> [!TIP]
> See **[live_events.cpp](https://github.com/jmuehlig/perf-cpp/tree/dev/examples/statistics/live_events.cpp)** for a full working example.

---

## Basic Lifecycle

Add events with `add_live()` instead of `add()`, then use `start()` / `stop()` as usual:

```cpp
#include <perfcpp/event_counter.hpp>

auto event_counter = perf::EventCounter{};
event_counter.add_live({"cache-misses", "cache-references", "branches"});

event_counter.start();

/// ... read live values during computation (see below) ...

event_counter.stop();

/// Release resources explicitly, or let the destructor handle it.
event_counter.close();
```

> [!IMPORTANT]
> Avoid mixing live events with regular events — using only live events leads to more consistent results.

> [!NOTE]
> Live events can only capture hardware events, not metrics.

## Reading Live Values

There are two ways to read counter values during computation.

### Using `LiveEventCounter` (recommended)

The `LiveEventCounter` wrapper handles memory management and difference calculation internally:

```cpp
auto live_event_counter = perf::LiveEventCounter{ event_counter };

event_counter.start();

for (auto i = 0U; i < runs; ++i) {
    live_event_counter.start();
    /// ... computation here ...
    live_event_counter.stop();

    std::cout
        << "cache-misses: " << live_event_counter.get("cache-misses")
        << ", cache-references: " << live_event_counter.get("cache-references")
        << std::endl;
}

event_counter.stop();
```

### Direct access via `EventCounter`

For maximum performance, pre-allocate result vectors (one entry per event) and compute differences yourself:

```cpp
/// One entry per event: cache-misses, cache-references, branches.
auto start_values = std::vector<double>{.0, .0, .0};
auto end_values = std::vector<double>{.0, .0, .0};

event_counter.start();

for (auto i = 0U; i < runs; ++i) {
    event_counter.live_result(start_values);
    /// ... computation here ...
    event_counter.live_result(end_values);

    std::cout
        << "cache-misses: " << end_values[0] - start_values[0]
        << ", cache-references: " << end_values[1] - start_values[1]
        << std::endl;
}

event_counter.stop();
```

Values are returned in the order events were added. This avoids per-read allocations but requires you to track the index-to-event mapping.
