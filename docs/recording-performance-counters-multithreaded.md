# Recording performance counters for multithreaded applications

Performance counters can be recorded for each thread.
To monitor multiple threads, you have two options:
* Record counters individually for each thread and combine the results afterward (&rarr; [See our multithreaded example: `examples/multi_thread.cpp`](../examples/multi_thread.cpp)).
* Initiate measurements that record counters for all child threads simultaneously (&rarr; [See our multithreaded inheritance example: `examples/inherit_thread.cpp`](../examples/inherit_thread.cpp)).

## First Option: Record counters individually for each thread
The `perf::EventCounterMT` class allows you to copy the measurement on every thread and combines the results.

### 1) Define the counters you want to record
```cpp
#include <perfcpp/event_counter.h>
auto counter_definitions = perf::CounterDefinition{};
auto event_counter = perf::EventCounter{counter_definitions};

event_counter.add({"instructions", "cycles", "branches", "branch-misses", "cache-misses", "cache-references"});
```

### 2) Create N thread-local counters
```cpp
auto multithread_event_counter = perf::EventCounterMT{std::move(event_counter), count_threads};
```

### 3) Wrap `start()` and `stop()` around your thread-local processing code
```cpp
auto threads = std::vector<std::thread>{};
for (auto thread_index = 0U; thread_index < count_threads; ++thread_index) {
    threads.emplace_back([thread_index, &multithread_event_counter]() {
        
        multithread_event_counter.start(thread_index);

        /// ... do some computational work here...

        multithread_event_counter.stop(thread_index);
    });
}
```

### 4) Wait for the threads to finish
```cpp
for (auto &thread: threads) {
    thread.join();
}
```

### 5) Access the combined counters
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

## Second Option: Record counters for all child threads simultaneously
The `perf::Config` class allows you to inherit the measurement to all child threads.

### 1) Define inheritance and the counters you want to record
```cpp
#include <perfcpp/event_counter.h>
auto counter_definitions = perf::CounterDefinition{};

auto config = perf::Config{};
config.include_child_threads(true);

auto event_counter = perf::EventCounter{counter_definitions, config};

event_counter.add({"instructions", "cycles", "branches", "branch-misses", "cache-misses", "cache-references"});
```

### 2) Wrap `start()` and `stop()` around thread-spawning
```cpp
auto threads = std::vector<std::thread>{};

event_counter.start();

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

### 3) Access the counter
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