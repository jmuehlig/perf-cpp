# *perf-cpp* Examples

We included various examples to teach you how to use *perf-cpp* and leverage hardware performance counter results directly from your application.

## How to Build the Examples

```
/// 1) Clone the repository
git clone https://github.com/jmuehlig/perf-cpp.git

/// 2) Switch to the cloned folder
cd perf-cpp

/// 3) Generate the Makefile
cmake . -B build -DBUILD_EXAMPLES=1

/// 4) Build the examples
cmake --build build --target examples
```

The examples will be built to `build/examples/bin/`.

## Recording Counter Statistics
* [single_thread.cpp](single_thread.cpp) provides an example to record and read performance counters for a specific code segment on a **single** thread.
* [inherit_thread.cpp](inherit_thread.cpp) advances the example to record counter statistics not only from one but also for its **child-threads**.
* [multi_thread.cpp](multi_thread.cpp) shows how to record performance counter statistics on **multiple** threads.
* [multi_cpu.cpp](multi_cpu.cpp) shows how to pin performance counters to **specific CPU cores** instead of focussing on threads and processes.

## Sampling Data
* [instruction_pointer_sampling.cpp](instruction_pointer_sampling.cpp) provides and example to sample instruction pointers on a single thread.
* [address_sampling.cpp](address_sampling.cpp) provides and example to sample virtual memory addresses, their latency, and their origin.
* [counter_sampling.cpp](counter_sampling.cpp) shows how to include values of further hardware performance counters into samples.
* [branch_sampling.cpp](branch_sampling.cpp) exemplifies sampling for last branch records and their prediction success.
* [register_sampling.cpp](register_sampling.cpp) provides an example on how to include values of specific registers into samples.
* [amd_ibs_raw_sampling.cpp](amd_ibs_raw_sampling.cpp) shows how to include raw data, using AMD IBS as an example, and how to interpret that data.
* [context_switch_sampling.cpp](context_switch_sampling.cpp) provides an example that samples context switches on a single thread.
* [multi_event_sampling.cpp](multi_event_sampling.cpp) exemplifies how to use multiple events as a trigger using Intel counters as an example.
* [multi_thread_sampling.cpp)](multi_thread_sampling.cpp) explains how to sample data on multiple threads at the same time.
* [multi_cpu_sampling.cpp](multi_cpu_sampling.cpp) provides an example that monitors multiple CPU cores and records samples.
