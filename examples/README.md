# *perf-cpp* Examples

We included various examples to teach you how to use *perf-cpp* and leverage hardware performance counter results directly from your application.

## How to Build the Examples

```
# Clone the repository
git clone https://github.com/jmuehlig/perf-cpp.git

# Switch to the cloned folder
cd perf-cpp

# Generate the Makefile
cmake . -B build -DBUILD_EXAMPLES=1

# Build the examples
cmake --build build --target examples
```

The examples will be built to `build/examples/bin/`.

## List of Examples
### Counting Hardware Events
* [statistics/single_thread.cpp](statistics/single_thread.cpp) provides an example to record and read performance counters for a specific code segment on a **single** thread.
* [statistics/inherit_thread.cpp](statistics/inherit_thread.cpp) advances the example to record counter statistics not only from one but also for its **child-threads**.
* [statistics/multi_thread.cpp](statistics/multi_thread.cpp) shows how to record performance counter statistics on **multiple** threads.
* [statistics/multi_cpu.cpp](statistics/multi_cpu.cpp) shows how to pin performance counters to **specific CPU cores** instead of focussing on threads and processes.
* [statistics/live_events.cpp](statistics/live_events.cpp) shows how to access hardware counters with **low latency**.

### Sampling
* [sampling/instruction_pointer.cpp](sampling/instruction_pointer.cpp) provides an example to sample instruction pointers on a single thread.
* [sampling/flame_graph.cpp](sampling/flame_graph.cpp) provides an example to generate a format that can be used by flamegraph generators.
* [sampling/memory_address.cpp](sampling/memory_address.cpp) provides an example to sample virtual memory addresses, their latency, and their origin.
* [sampling/counter.cpp](sampling/counter.cpp) shows how to include values of further hardware performance counters into samples.
* [sampling/branch.cpp](sampling/branch.cpp) exemplifies sampling for last branch records and their prediction success.
* [sampling/register.cpp](sampling/register.cpp) provides an example on how to include values of specific registers into samples.
* [sampling/context_switch.cpp](sampling/context_switch.cpp) provides an example that samples context switches on a single thread.
* [sampling/multi_event.cpp](sampling/multi_event.cpp) exemplifies how to use multiple events as a trigger using Intel counters as an example.
* [sampling/multi_thread.cpp)](sampling/multi_thread.cpp) explains how to sample data on multiple threads at the same time.
* [sampling/multi_cpu.cpp](sampling/multi_cpu.cpp) provides an example that monitors multiple CPU cores and records samples.
