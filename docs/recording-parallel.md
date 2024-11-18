# Counting Hardware Events in Parallel Applications

Performance counters can be counted for each thread or CPU core.
To monitor multiple threads or CPU cores, you have various options:
* Record counters individually for each thread and combine the results afterward (&rarr; [See our multithreaded code example: `examples/multi_thread.cpp`](../examples/multi_thread.cpp)).
* Initiate measurements that record counters for all child threads simultaneously (&rarr; [See our multithreaded inheritance code example: `examples/inherit_thread.cpp`](../examples/inherit_thread.cpp)).
* Monitor specific CPU cores and record counters of all processes executed there (&rarr; [See our multi cpu code example: `examples/multi_cpu.cpp`](../examples/multi_cpu.cpp)).

----
## Table of Contents
- [1st Option: Count Events Individually for each Thread](#1st-option-count-events-individually-for-each-thread)
- [2nd Option: Count Events for all Child Threads Simultaneously](#2nd-option-count-events-for-all-child-threads-simultaneously)
- [3rd Option: Count Events for entire CPU Cores](#3rd-option-count-events-on-specific-cpu-cores)
---

## 1st Option: Count Events Individually for each Thread
The `perf::MultiThreadEventCounter` class allows you to copy the measurement on every thread and combines the results.

### Define the events to record
```cpp
#include <perfcpp/event_counter.h>
/// The perf::CounterDefinition object holds all counter names and must be alive when counters are accessed.
auto counter_definitions = perf::CounterDefinition{};

auto multithread_event_counter = perf::MultiThreadEventCounter{counter_definitions};
try {
    multithread_event_counter.add({"instructions", "cycles", "branches", "branch-misses", "cache-misses", "cache-references"});
} catch (std::runtime_error& e) {
    std::cerr << e.what() << std::endl;
}
```

### Wrap `start()` and `stop()` around your thread-local processing code
```cpp
auto threads = std::vector<std::thread>{};
for (auto thread_index = 0U; thread_index < count_threads; ++thread_index) {
    threads.emplace_back([thread_index, &multithread_event_counter]() {
        
        try {
            multithread_event_counter.start(thread_index);
        } catch (std::runtime_error& e) {
            std::cerr << e.what() << std::endl;
        }

        /// ... do some computational work here...

        multithread_event_counter.stop(thread_index);
    });
}
```

### Wait for the threads to finish
```cpp
for (auto &thread: threads) {
    thread.join();
}
```

### Access the combined results
```cpp
/// Calculate the result.
const auto result = multithread_event_counter.result();

/// Ask the result for specific counters.
const auto cycles = result.get("cycles");
std::cout << "Took " << cycles.value() << " cycles" << std::endl;

/// Or print all counters on your own.
for (const auto [name, value] : result)
{
    std::cout << "Counter " << name << " = " << value << std::endl;
}

/// Or print in CSV and JSON.
std::cout << result.to_csv(/* delimiter = */'|', /* print header = */ true) << std::endl;
std::cout << result.to_json() << std::endl;
```

### Closing the Hardware Counters *(optional)*
Once you have initialized the hardware performance counters, you can `start()`, `stop()`, and gather results repeatedly.
To ultimately release resources such as file descriptors, consider closing the `MultiThreadEventCounter`:

```cpp
multithread_event_counter.close();
```

This action is optional and will occur automatically upon object deconstruction if `close()` is not invoked manually.

---

## 2nd Option: Count Events for all Child Threads Simultaneously
The `perf::Config` class allows you to inherit the measurement to all child threads.

### Define inheritance and the counters to record
```cpp
#include <perfcpp/event_counter.h>
auto counter_definitions = perf::CounterDefinition{};

auto config = perf::Config{};
config.include_child_threads(true);

auto event_counter = perf::EventCounter{counter_definitions, config};

try {
    event_counter.add({"instructions", "cycles", "branches", "branch-misses", "cache-misses", "cache-references"});
} catch (std::runtime_error& e) {
    std::cerr << e.what() << std::endl;
}
```

### Wrap `start()` and `stop()` around thread-spawning
```cpp
auto threads = std::vector<std::thread>{};

try {
    event_counter.start()
} catch (std::runtime_error& e) {
    std::cerr << e.what() << std::endl;
}

for (auto thread_index = 0U; thread_index < count_threads; ++thread_index) {
    threads.emplace_back([]() {
        /// ... do some computational work here...
    });
}

/// Wait for all threads to finish.
for (auto &thread: threads) {
    thread.join();
}

event_counter.stop();
```

### Access the results
```cpp
/// Calculate the result.
const auto result = event_counter.result();

/// Ask the result for specific counters.
const auto cycles = result.get("cycles");
std::cout << "Took " << cycles.value() << " cycles" << std::endl;

/// Or print all counters on your own.
for (const auto [name, value] : result)
{
    std::cout << "Counter " << name << " = " << value << std::endl;
}

/// Or print in CSV and JSON.
std::cout << result.to_csv(/* delimiter = */'|', /* print header = */ true) << std::endl;
std::cout << result.to_json() << std::endl;
```

### Closing the Hardware Counters *(optional)*
Once you have initialized the hardware performance counters, you can `start()`, `stop()`, and gather results repeatedly.
To ultimately release resources such as file descriptors, consider closing the `EventCounter`:

```cpp
event_counter.close();
```

This action is optional and will occur automatically upon object deconstruction if `close()` is not invoked manually.

---

## 3rd Option: Count Events on specific CPU Cores
The `perf::MultiCoreEventCounter` class allows you record performance counters on specified CPU cores.
Please note that you may record events of other applications running on that CPU cores.

According to the [`perf_event_open` documentation](https://man7.org/linux/man-pages/man2/perf_event_open.2.html), this option needs a `/proc/sys/kernel/perf_event_paranoid` value of `< 1`.

### Define CPU cores to watch
```cpp
/// Create a list of (logical) cpu ids to record performance counters on.
auto cpus_to_watch = std::vector<std::uint16_t>{};
cpus_to_watch.add(0U);
cpus_to_watch.add(1U);
/// ... add more.
```

### Define the counters you want to record
```cpp
#include <perfcpp/event_counter.h>
/// The perf::CounterDefinition object holds all counter names and must be alive when counters are accessed.
auto counter_definitions = perf::CounterDefinition{};

auto multi_cpu_event_counter = perf::MultiCoreEventCounter{counter_definitions};
try {
    multi_cpu_event_counter.add({"instructions", "cycles", "branches", "branch-misses", "cache-misses", "cache-references"});
} catch (std::runtime_error& e) {
    std::cerr << e.what() << std::endl;
}
```

### Start and stop the counters whenever you want
```cpp
/// You can start threads here.
try {
    multi_cpu_event_counter.start();
} catch (std::runtime_error& e) {
    std::cerr << e.what() << std::endl;
}

/// ... wait until some work is done on the CPUs.
/// For example, join threads here.

multi_cpu_event_counter.stop();
```

### Access the combined results
```cpp
/// Calculate the result.
const auto result = multi_cpu_event_counter.result();

/// Ask the result for specific counters.
const auto cycles = result.get("cycles");
std::cout << "Took " << cycles.value() << " cycles" << std::endl;

/// Or print all counters on your own.
for (const auto [name, value] : result)
{
    std::cout << "Counter " << name << " = " << value << std::endl;
}

/// Or print in CSV and JSON.
std::cout << result.to_csv(/* delimiter = */'|', /* print header = */ true) << std::endl;
std::cout << result.to_json() << std::endl;
```

### Closing the Hardware Counters *(optional)*
Once you have initialized the hardware performance counters, you can `start()`, `stop()`, and gather results repeatedly.
To ultimately release resources such as file descriptors, consider closing the `MultiCoreEventCounter`:

```cpp
multi_cpu_event_counter.close();
```

This action is optional and will occur automatically upon object deconstruction if `close()` is not invoked manually.
