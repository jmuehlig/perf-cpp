# Event Sampling for multiple threads/CPU cores
Sampling for parallel executed code can be done by sampling individual threads or individual CPU cores.
We will describe both option in detail below.

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
constexpr auto count_threads = 4U;

/// The perf::CounterDefinition object holds all counter names and must be alive when counters are accessed.
auto counter_definitions = perf::CounterDefinition{};

auto event_counter = perf::EventCounter{counter_definitions};

/// Define when the data is recorded.
/// This can be every Nth event (e.g., every 1000th cycle):
auto sample_config = perf::SampleConfig{};
sample_config.period(1000U);
/// or with a frequency of N (e.g., every ms with a clock-frequency of 2GHz):
sample_config.frequency(2000000U);
/// Note that you can use only frequency XOR period

/// Define the trigger (cycles in this example) 
//      and what to record (time and instruction pointer in this example)
auto multi_thread_sampler = perf::MultiThreadSampler{
    counter_definitions,
    "cycles",
    perf::Sampler::Type::Time | perf::Sampler::Type::ThreadId,
    count_threads /// Number of threads
    sample_config
};
```

### 2) Call `start()` and `stop()` from threads
Each thread has to start and stop its sampler.

```cpp
auto threads = std::vector<std::thread>{};
for (auto thread_index = 0U; thread_index < count_threads; ++thread_index) {
    
    /// Create threads.
    threads.emplace_back([thread_index, &multi_thread_sampler /*,  ... more stuff .. */]() {
      /// Start sampling per thread.
      multi_thread_sampler.start(thread_index);

      /// ... do some work that is sampled...

      /// Stop sampling on this thread.
      multi_thread_sampler.stop(thread_index);
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
auto result = multi_thread_sampler.result();

/// Sort samples by time since they may be mixed from different threads
std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) {
    if (a.time().has_value() && b.time().has_value()) {
      return a.time().value() < b.time().value();
    }
    return false;
});

/// Print the samples
for (const auto& sample : result)
{
    if (sample.time().has_value() && sample.thread_id().has_value())
    {
        std::cout << "Time = " << sample.time().value() 
            << " | Thread ID = " << sample.thread_id().value() << std::endl;
    }
}

/// Close the sampler
multi_thread_sampler.close();
```

The output may be something like this:

    Time = 173058802647651 | Thread ID = 62803 
    Time = 173058803163735 | Thread ID = 62802 
    Time = 173058803625986 | Thread ID = 62804
    Time = 173058803763485 | Thread ID = 62803
    Time = 173058804277715 | Thread ID = 62802

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

auto event_counter = perf::EventCounter{counter_definitions};

/// Define when the data is recorded.
/// This can be every Nth event (e.g., every 1000th cycle):
auto sample_config = perf::SampleConfig{};
sample_config.period(1000U);
/// or with a frequency of N (e.g., every ms with a clock-frequency of 2GHz):
sample_config.frequency(2000000U);
/// Note that you can use only frequency XOR period

/// Define the trigger (cycles in this example) 
//      and what to record (time and instruction pointer in this example)
auto multi_cpu_sampler = perf::MultiCoreSampler{
    counter_definitions,
    "cycles",
    perf::Sampler::Type::Time | perf::Sampler::Type::CPU | perf::Sampler::Type::ThreadId,
    std::move(cpus_to_watch) /// List of CPUs to sample
    sample_config
};
```

### 2) Call `start()` and `stop()` 
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
multi_cpu_sampler.start();

/// Wait for all threads to finish.
for (auto& thread : threads) {
    thread.join();
}

/// Stop sampling after all threads have finished.
multi_cpu_sampler.stop();
```

### 3) Access the recorded samples
The output consists of a list of `perf::Sample` instances, where each sample may contain comprehensive data.
As you have the flexibility to specify which data elements to sample, each piece of data is encapsulated within an `std::optional` to handle its potential absence.
You may want to sort the results (e.g., based on the timestamp) since threads will write samples in parallel.

```cpp
auto result = multi_core_sampler.result();

/// Sort samples by time since they may be mixed from different threads
std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) {
    if (a.time().has_value() && b.time().has_value()) {
      return a.time().value() < b.time().value();
    }
    return false;
});

/// Print the samples
for (const auto& sample : result)
{
    if (sample.time().has_value() && sample.cpu_id().has_value() && sample.thread_id().has_value())
    {
        std::cout << "Time = " << sample.time().value() 
            << " | CPU ID = " << sample.cpu_id().value()
            << " | Thread ID = " << sample.thread_id().value() << std::endl;
    }
}

/// Close the sampler
multi_cpu_sampler.close();
```

The output may be something like this:

    Time = 173058798201719 | CPU ID = 6 | Thread ID = 62803
    Time = 173058798713083 | CPU ID = 3 | Thread ID = 62802
    Time = 173058799309247 | CPU ID = 6 | Thread ID = 62803
    Time = 173058799826723 | CPU ID = 3 | Thread ID = 62802
    Time = 173058800426323 | CPU ID = 6 | Thread ID = 62803
    Time = 173058800943659 | CPU ID = 3 | Thread ID = 62802
    Time = 173058801403355 | CPU ID = 8 | Thread ID = 62804

