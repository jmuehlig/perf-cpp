# *perf-cpp*: Access Hardware Performance Counters from C++ Applications

*perf-cpp* is a streamlined C++ library that leverages the *perf subsystem* on Linux to access hardware performance counters directly from the application. 
The key features are:

* **Simplified Performance Measurement**: Directly interact with hardware performance counters from your C++ application within specific code segments (&rarr;[documentation](docs/recording.md)).
* **Event Sampling**: Leverage sampling to gather performance data periodically, enabling efficient analysis of resource usage (e.g., instruction pointers, data addresses, access latency, branches, registers, and more) over time and/or execution (&rarr;[documentation](docs/sampling.md)).
* **Customizable Events**: Easily extend the built-in hardware events with those specific to your underlying hardware substrate (&rarr;[documentation](docs/counters.md)).
* We included various [**examples**](examples/) to learn how to utilize the library from your C++ application.

Author: Jan Mühlig (`jan.muehlig@tu-dortmund.de`)

----

## Getting Started
Access hardware performance counters directly within your C++ application, focusing exclusively on the code crucial to your analysis.

&rarr; Further details are available in the [documentation](docs/README.md).

### Count Hardware Events
```cpp
#include <perfcpp/event_counter.h>

/// Create the counter
auto counters = perf::CounterDefinition{};
auto event_counter = perf::EventCounter{ counters };

/// Specify hardware events to record
event_counter.add({"instructions", "cycles", "cache-misses"});

/// Run the workload
event_counter.start();
/// ... workload ...
event_counter.stop();

/// Print the result to the console
const auto result = event_counter.result();
for (const auto [event_name, value] : result)
{
    std::cout << event_name << ": " << value << std::endl;
}
```

The output of the program above could look like this:
```
instructions: 5.97298e+07
cycles: 5.02462e+08
cache-misses: 1.36517e+07
```

The `perf::EventCounter` class offers an interface to count hardware events – comparable to the `perf stat` command, but with control over the monitored code segment.
The interface allows to specify events, start/stop recordings, and access the results.

&rarr; See the documentation for [event-counting basics](docs/recording.md) and for [multithreaded/multicore environments](docs/recording-parallel.md).

### Record Samples
```cpp
#include <perfcpp/sampler.h>

/// Create the sampler
auto counters = perf::CounterDefinition{};
auto sampler = perf::Sampler{ counters };

/// Specify when a sample is recorded: every 4000th cycle
sampler.trigger("cycles", perf::Period{4000U});

/// Specify what metadata is included into a sample: time, CPU ID, instruction
sampler.values()
    .time(true)
    .cpu_id(true)
    .instruction_pointer(true);

/// Run the workload
sampler.start();
/// ... workload ...
sampler.stop();

/// Print the samples to the console
const auto samples = sampler.result();
for (const auto& sample_record : samples)
{
    const auto time = sample_record.time().value();
    const auto cpu_id = sample_record.cpu_id().value();
    const auto instruction = sample_record.instruction_pointer().value();
    
    std::cout 
        << "Time = " << time << " | CPU = " << cpu_id
        << " | Instruction = 0x" << std::hex << instruction << std::dec
        << std::endl;
}
```

The output of the program above could look like this:
```
Time = 365449130714033 | CPU = 8 | Instruction = 0x5a6e84b2075c
Time = 365449130913157 | CPU = 8 | Instruction = 0x64af7417c75c
Time = 365449131112591 | CPU = 8 | Instruction = 0x5a6e84b2075c
Time = 365449131312005 | CPU = 8 | Instruction = 0x64af7417c75c 
```

The `perf::Sampler` class provides an interface to specify sampling criteria and control the start/stop of recordings – comparable to `perf [mem|c2c] record` and `perf report`; but with control of the recorded code segments.
You can sample various aspects such as instructions, time, memory addresses, access latency, call chains, branches, and more.


&rarr; See the documentation for [sampling basics](docs/sampling.md) and [multithreaded sampling](docs/sampling-parallel.md).


## Building
*perf-cpp* can be built by hand or included into CMake projects (&rarr; [more details in the documentation](docs/build.md)).

```
/// 1) Clone the repository
git clone https://github.com/jmuehlig/perf-cpp.git

/// 2) Switch to the cloned folder
cd perf-cpp

/// 3) Switch to the current stable version (optional)
git checkout v0.8.1

/// 4) Generate the Makefile
cmake . -B build -DBUILD_EXAMPLES=1

/// 5) Build the library (only) into the build/ folder
cmake --build build

/// 6) Build the examples (optional)
/// Examples will be compiled to build/examples/bin/
cmake --build build --target examples
```

---

## Documentation
* [Building and Including this Library](docs/build.md)
* **Recording Performance Counters**
  * [Overview and Basics of Recording Performance Counters](docs/recording.md)
  * [Recording Counters in Parallel (multithread / multicore) Settings](docs/recording-parallel.md)
  * [Defining and Using Metrics](docs/metrics.md)
* **Event Sampling**
  * [Overview and Basics of Event Sampling](docs/sampling.md)
  * [Event Sampling in Parallel (multithread / multicore) Settings](docs/sampling-parallel.md)
* [Built-in and Hardware-specific Performance Counters](docs/counters.md)
* [Changelog](CHANGELOG.md)

---

## More Code Examples
We provide a variety of [examples](examples/) detailed below. 

Build them effortlessly by running 
```
cmake . -B build -DBUILD_EXAMPLES=1
cmake --build build --target examples
```

All compiled example binaries are located in `build/examples/bin` and can be executed directly without additional arguments.

### Counting Hardware Events
* Code example for counting hardware events on a [single thread: `examples/single_thread.cpp`](examples/single_thread.cpp)
* Code example for counting hardware events on  [multiple threads through inheritance: `examples/inherit_thread.cpp`](examples/inherit_thread.cpp)
* Code example for counting hardware events on [multiple threads: `examples/multi_thread.cpp`](examples/multi_thread.cpp)
* Code example for counting hardware events on  [specific CPU cores: `examples/multi_cpu.cpp`](examples/inherit_thread.cpp)

### Recording Samples
* Code example for sampling [instruction pointers: `examples/instruction_pointer_sampling.cpp`](examples/instruction_pointer_sampling.cpp)
* Code example for sampling [memory addresses: `examples/address_sampling.cpp`](examples/address_sampling.cpp)
* Code example for sampling [counter values: `examples/counter_sampling.cpp`](examples/counter_sampling.cpp)
* Code example for sampling [branches: `examples/branch_sampling.cpp`](examples/branch_sampling.cpp)
* Code example for sampling [register values: `examples/register_sampling.cpp`](examples/register_sampling.cpp)
* Code example for sampling [raw values using AMD IBS: `examples/amd_ibs_raw_sampling.cpp`](examples/amd_ibs_raw_sampling.cpp)
* Code example for sampling [context switches: `examples/context_switch_sampling.cpp`](examples/context_switch_sampling.cpp)
* Code example for sampling [with multiple triggers: `examples/multi_event_sampling.cpp`](examples/multi_event_sampling.cpp)
* Code example for [multithreaded sampling: `examples/multi_thread_sampling.cpp`](examples/multi_thread_sampling.cpp)
* Code example for [multicore sampling: `examples/multi_cpu_sampling.cpp`](examples/multi_cpu_sampling.cpp)

## System Requirements
* Minimum *Linux Kernel version*: `>= 4.0`
* Recommended *Linux Kernel version*: `>= 5.13` (older Kernels might not implement all features like sampling for latency).
* Installed `perf` (check if `perf stat -- ls` provides any output, otherwise follow the instructions)

---

## Other Noteworthy Profiling Projects
While *perf-cpp* is dedicated to providing developers with clear insights into application performance, it is part of a broader ecosystem of tools that facilitate performance analysis. 
Below is a non-exhaustive list of some other valuable profiling projects:

* [PAPI](https://github.com/icl-utk-edu/papi) offers access not only to CPU performance counters but also to a variety of other hardware components including GPUs, I/O systems, and more.
* Intel's [Instrumentation and Tracing Technology](https://github.com/intel/ittapi) allows applications to manage the collection of trace data effectively when used in conjunction with [Intel VTune Profiler](https://www.intel.com/content/www/us/en/developer/tools/oneapi/vtune-profiler.html).
* [PerfEvent](https://github.com/viktorleis/perfevent) provides lightweight access to performance counters, facilitating streamlined performance monitoring.
* For those who prefer a more hands-on approach, the [perf_event_open](https://man7.org/linux/man-pages/man2/perf_event_open.2.html) system call can be utilized directly without any wrappers.


## Resources about (Perf-) Profiling
This is a non-exhaustive list of academic research papers and blog articles (feel free to add to it, e.g., via pull request – also your own work).

### Academical Papers
* [Quantitative Evaluation of Intel PEBS Overhead for Online System-Noise Analysis](https://soramichi.jp/pdf/ROSS2017.pdf) (2017)
* [Analyzing memory accesses with modern processors](https://dl.acm.org/doi/abs/10.1145/3399666.3399896) (2020)
* [Precise Event Sampling on AMD Versus Intel: Quantitative and Qualitative Comparison](https://ieeexplore.ieee.org/stamp/stamp.jsp?arnumber=10068807&tag=1) (2023)
* [Multi-level Memory-Centric Profiling on ARM Processors with ARM SPE](https://arxiv.org/html/2410.01514v1) (2024)

### Blog Posts
* [C2C - False Sharing Detection in Linux Perf](https://joemario.github.io/blog/2016/09/01/c2c-blog/) (2016)
* [PMU counters and profiling basics.](https://easyperf.net/blog/2018/06/01/PMU-counters-and-profiling-basics) (2018)
* [Detect false sharing with Data Address Profiling.](https://easyperf.net/blog/2019/12/17/Detecting-false-sharing-using-perf) (2019)
* [Advanced profiling topics. PEBS and LBR.](https://easyperf.net/blog/2018/06/08/Advanced-profiling-topics-PEBS-and-LBR) (2018)

---

## Feedback
Feedback, feature requests, and contributions are always appreciated. 
If you have any feedback, please feel free to email me at `jan.muehlig@tu-dortmund.de`, or you can directly submit a pull request.
