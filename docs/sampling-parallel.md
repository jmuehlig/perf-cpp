# Event Sampling for multiple threads/CPU cores
Sampling for parallel executed code can be done by sampling individual threads or individual CPU cores.
We will describe both option in detail below.

---
## Table of Contents
- [Sample individual threads](#sample-individual-threads)
    - [1) Creating a sampler for multiple threads](#1-creating-a-sampler-for-multiple-threads)
    - [2) Call `start()` and `stop()` from threads](#2-call-start-and-stop-from-threads)
    - [3) Access the recorded samples](#3-access-the-recorded-samples)
    - [4) Closing the sampler](#4-closing-the-sampler)
- [Sample on specific CPU cores](#sample-on-specific-cpu-cores)
    - [1) Creating a sampler for multiple CPU cores](#1-creating-a-sampler-for-multiple-cpu-cores)
    - [2) Open the sampler *(optional)*](#2-open-the-sampler-optional)
    - [3) Call `start()` and `stop()`](#3-call-start-and-stop)
    - [4) Access the recorded samples](#4-access-the-recorded-samples-1)
    - [5) Closing the sampler](#5-closing-the-sampler)
---

## Sample individual threads
Sampling is always done for the calling thread.
The `perf::MultiThreadSampler` class provides a wrapper that can hold multiple samplers for multiple threads.
Each wrapper has to be started and stopped *locally* on a specific thread.
The results are merged.

&rarr; [See code example `multi_thread_sampling.cpp`](../examples/multi_thread_sampling.cpp)

### 1) Creating a sampler for multiple threads
The `MultiThreadSampler` needs to know how many threads will be sampled (see `count_threads` in the code example below).

```cpp
#include <perfcpp/sampler.h>

/// The perf::CounterDefinition object holds all counter names and must be alive when counters are accessed.
auto counter_definitions = perf::CounterDefinition{};

/// Define when the data is recorded.
/// This can be every Nth event (e.g., every 1000th cycle):
auto sample_config = perf::SampleConfig{};
sample_config.period(1000U);
/// or with a frequency of N (e.g., every ms with a clock-frequency of 2GHz):
sample_config.frequency(2000000U);
/// Note that you can use only frequency XOR period

constexpr auto count_threads = 4U;
auto sampler = perf::MultiThreadSampler{  
    counter_definitions,
    count_threads /// Number of threads
    sample_config
};

/// Define the trigger (cycles in this example) 
sampler.trigger("cycles");

//  and what to record (time and instruction pointer in this example)
sampler.values().time(true).thread_id(true);
```

### 2) Call `start()` and `stop()` from threads
Each thread has to start and stop its sampler.

```cpp
auto threads = std::vector<std::thread>{};
for (auto thread_index = 0U; thread_index < count_threads; ++thread_index) {
    
    /// Create threads.
    threads.emplace_back([thread_index, &sampler /*,  ... more stuff .. */]() {
      /// Start sampling per thread.
        try {
            sampler.start(thread_index);
        } catch (std::runtime_error& e) {
            std::cerr << e.what() << std::endl;
        }

      /// ... do some work that is sampled...

      /// Stop sampling on this thread.
      sampler.stop(thread_index);
    });
}

/// Wait for all threads to finish.
for (auto& thread : threads) {
    thread.join();
}
```

### 3) Access the recorded samples
The output consists of a list of `perf::Sample` instances, where each sample may contain comprehensive data. 
As you have the flexibility to specify which data elements to sample, each piece of data is encapsulated within an `std::optional` to handle its potential absence.
You may want to sort the results (e.g., based on the timestamp) since threads will write samples in parallel.

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
    Time = 173058803763485 | Thread ID = 62803
    Time = 173058804277715 | Thread ID = 62802

### 4) Closing the sampler
Closing the sampler will free and un-map all buffers.
```cpp
sampler.close();
```

---

## Sample on specific CPU cores
If you do not want (or cannot) start sampling on individual threads, you can also choose specific CPU cores, no matter which thread is executed there, by using the `MultiCoreSampler`.
The `MultiCoreSampler` will be started and stopped for all CPU cores at once.
Please keep in mind that this also implies that undesirable events are sampled, such as when other apps run on these cores.

&rarr; [See code example `multi_cpu_sampling.cpp`](../examples/multi_cpu_sampling.cpp)

### 1) Creating a sampler for multiple CPU cores
The `MultiCoreSampler` needs to know which CPU cores will be sampled (see `cpus_to_watch` in the code example below).

```cpp
#include <perfcpp/sampler.h>
/// Create a list of cpus to sample (all available, in this example).
  auto cpus_to_watch = std::vector<std::uint16_t>(std::thread::hardware_concurrency());
  std::iota(cpus_to_watch.begin(), cpus_to_watch.end(), 0U);

/// The perf::CounterDefinition object holds all counter names and must be alive when counters are accessed.
auto counter_definitions = perf::CounterDefinition{};

/// Define when the data is recorded.
/// This can be every Nth event (e.g., every 1000th cycle):
auto sample_config = perf::SampleConfig{};
sample_config.period(1000U);
/// or with a frequency of N (e.g., every ms with a clock-frequency of 2GHz):
sample_config.frequency(2000000U);
/// Note that you can use only frequency XOR period

auto sampler = perf::MultiCoreSampler{
    counter_definitions,
    std::move(cpus_to_watch) /// List of CPUs to sample
    sample_config
};

/// Define the trigger (cycles in this example) 
sampler.trigger("cycles");

/// and what to record (time and instruction pointer in this example).
sampler.values().time(true).cpu_id(true).thread_id(true);
```

### 2) Open the sampler *(optional)*
The sampler will be opened by `sampler.start()`, if it is not already opened.
Opening the sampler means setting up all the counters and buffers, which can take some time.
If you need precise time measurements and want to exclude the counter setup, you can call open individually.

```cpp
try {
    sampler.open();
} catch (std::runtime_error& e) {
    std::cerr << e.what() << std::endl;
}
```

### 3) Call `start()` and `stop()` 
No matter for which threads, the sampler only needs to be started once.

```cpp
auto threads = std::vector<std::thread>{};
for (auto thread_index = 0U; thread_index < count_threads; ++thread_index) {
    
    /// Create threads.
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

### 4) Access the recorded samples
The output consists of a list of `perf::Sample` instances, where each sample may contain comprehensive data.
As you have the flexibility to specify which data elements to sample, each piece of data is encapsulated within an `std::optional` to handle its potential absence.
You may want to sort the results (e.g., based on the timestamp) since threads will write samples in parallel.

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
    Time = 173058799309247 | CPU ID = 6 | Thread ID = 62803
    Time = 173058799826723 | CPU ID = 3 | Thread ID = 62802
    Time = 173058800426323 | CPU ID = 6 | Thread ID = 62803
    Time = 173058800943659 | CPU ID = 3 | Thread ID = 62802
    Time = 173058801403355 | CPU ID = 8 | Thread ID = 62804

### 5) Closing the sampler
Closing the sampler will free and un-map all buffers.
```cpp
sampler.close();
```
