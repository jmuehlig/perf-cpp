# Advanced Event Sampling for Multithreaded and Multi-core Environments
In parallel computing environments, understanding the performance of code executed across multiple threads or CPU cores is crucial.
*perf-cpp* facilitates this by providing tools to sample performance metrics either by individual threads or specific CPU cores. 
This guide will cover how to set up and utilize these sampling capabilities effectively.

---
## Table of Contents
- [Sample Separate Threads](#sample-individual-threads)
  - [Setting Up Multi-threaded Sampler](#setting-up-multi-threaded-sampler)
  - [Starting and Stopping Sampler in Threads](#starting-and-stopping-sampler-in-threads)
  - [Retrieving and Analyzing Samples](#retrieving-and-analyzing-samples)
  - [Releasing Sampler Resources *(optional)*](#releasing-sampler-resources-optional)
- [Sample on Specific CPU Cores](#sample-on-specific-cpu-cores)
  - [Setting Up Multi-Core Sampler](#setting-up-multi-core-sampler)
  - [Sampler Initialization *(optional)*](#sampler-initialization-optional)
  - [Starting and Stopping Samplers](#starting-and-stopping-samplers)
  - [Retrieving and Analyzing Samples](#retrieving-and-analyzing-samples-1)
  - [Releasing Sampler Resources *(optional)*](#releasing-sampler-resources-optional-1)
---

## Sample Separate Threads
*perf-cpp* provides the `MultiThreadSampler` class to manage samplers for different threads, enabling precise performance measurements across thread-specific tasks.

&rarr; [See code example `multi_thread_sampling.cpp`](../examples/multi_thread_sampling.cpp)

### Setting Up Multi-threaded Sampler
Initialize a sampler for each thread to monitor specific events:

```cpp
#include <perfcpp/sampler.h>

auto counter_definitions = perf::CounterDefinition{};
auto sample_config = perf::SampleConfig{};
sample_config.period(4000U);

auto sampler = perf::MultiThreadSampler{  
    counter_definitions,
    /* number of threads */ 4U
    sample_config
};

sampler.trigger("cycles");
sampler.values().time(true).thread_id(true);
```

### Starting and Stopping Sampler in Threads
Each thread should manage its sampler instance:

```cpp
auto threads = std::vector<std::thread>{};
for (auto thread_index = 0U; thread_index < 4U; ++thread_index) {
    threads.emplace_back([thread_index, &sampler /*,  ... more stuff .. */]() {
        try {
            sampler.start(thread_index);
        } catch (std::runtime_error& e) {
            std::cerr << e.what() << std::endl;
        }

      /// ... do some work that is sampled...

      sampler.stop(thread_index);
    });
}

for (auto& thread : threads) {
    thread.join();
}
```

### Retrieving and Analyzing Samples
After the threads complete execution, collate and analyze the data:

```cpp
auto result = sampler.result(/* sort samples by time*/ true);

/// Print the samples
for (const auto& sample_record : result)
{
    if (sample_record.time().has_value() && sample_record.thread_id().has_value())
    {
        std::cout 
            << "Time = " << sample_record.time().value() 
            << " | Thread ID = " << sample_record.thread_id().value() << std::endl;
    }
}
```

The output may be something like this:

    Time = 173058802647651 | Thread ID = 62803 
    Time = 173058803163735 | Thread ID = 62802 
    Time = 173058803625986 | Thread ID = 62804
    Time = 173058804277715 | Thread ID = 62802

### Releasing Sampler Resources *(optional)*
Closing the sampler will free and un-map all resources like buffers and hardware counters.

```cpp
sampler.close();
```

---

## Sample on Specific CPU Cores
For applications sensitive to the specific cores they run on, *perf-cpp* offers `MultiCoreSampler`.

&rarr; [See code example `multi_cpu_sampling.cpp`](../examples/multi_cpu_sampling.cpp)

**Note**: This records data of all processes running on the specified cores and needs specific permissions (i.e., a value of less than `1` in `/proc/sys/kernel/perf_event_paranoid`).

### Setting Up Multi-Core Sampler
The `MultiCoreSampler` needs to know which CPU cores will be sampled (see `cpus_to_watch` in the code example below).

```cpp
#include <perfcpp/sampler.h>
/// Create a list of CPUS to monitor.
auto cpus_to_watch = std::vector<std::uint16_t>{0U, 1U, 2U, 3U};

auto counter_definitions = perf::CounterDefinition{};
auto sample_config = perf::SampleConfig{};
sample_config.period(4000U);

auto sampler = perf::MultiCoreSampler{
    counter_definitions,
    std::move(cpus_to_watch) /// List of CPUs to sample
    sample_config
};

sampler.trigger("cycles");
sampler.values().time(true).cpu_id(true).thread_id(true);
```

### Sampler Initialization *(optional)*
Optionally, open the sampler before starting to separate configuration overhead from measurement:

```cpp
try {
    sampler.open();
} catch (std::runtime_error& e) {
    std::cerr << e.what() << std::endl;
}
```

### Starting and Stopping Samplers
No matter for which threads, the sampler only needs to be started once.

```cpp
auto threads = std::vector<std::thread>{};
for (auto thread_index = 0U; thread_index < count_threads; ++thread_index) {
    threads.emplace_back([thread_index, /*,  ... more stuff .. */]() {
      /// ... do some work that is sampled...
    });
}

/// Start sampling.
try {
    sampler.start();
} catch (std::runtime_error& e) {
    std::cerr << e.what() << std::endl;
}

/// Wait for all threads to finish.
for (auto& thread : threads) {
    thread.join();
}

/// Stop sampling after all threads have finished.
sampler.stop();
```

### Retrieving and Analyzing Samples
Access and print the collected data:

```cpp
auto result = sampler.result(/* sort samples by time*/ true);

/// Print the samples
for (const auto& sample_record : result)
{
    if (sample_record.time().has_value() && sample_record.cpu_id().has_value() && sample_record.thread_id().has_value())
    {
        std::cout 
            << "Time = " << sample_record.time().value() 
            << " | CPU ID = " << sample_record.cpu_id().value()
            << " | Thread ID = " << sample_record.thread_id().value() << std::endl;
    }
}
```

The output may be something like this:

    Time = 173058798201719 | CPU ID = 6 | Thread ID = 62803
    Time = 173058798713083 | CPU ID = 3 | Thread ID = 62802
    Time = 173058799826723 | CPU ID = 3 | Thread ID = 62802
    Time = 173058800426323 | CPU ID = 6 | Thread ID = 62803
    Time = 173058801403355 | CPU ID = 8 | Thread ID = 62804

### Releasing Sampler Resources *(optional)*
Closing the sampler will free and un-map all resources like buffers and hardware counters.

```cpp
sampler.close();
```
