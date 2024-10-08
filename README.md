# *perf-cpp*: Performance Counter Wrapper for C++

*perf-cpp* is a streamlined C++ library designed to make performance analysis more intuitive and focused. 
The key features are:

* **Simplified Performance Measurement**: Directly interact with hardware performance counters from your C++ application within specific code segments (&rarr;[read the documentation](docs/recording.md)).
* **Event Sampling**: Leverage sampling to gather performance data periodically, enabling efficient analysis of resource usage (including instruction pointers, data, branches, registers, and more) over time and/or execution (&rarr;[read the documentation](docs/sampling.md)).
* **Customizable Counter Management**: Easily extend the built-in counters with those specific to your underlying hardware substrate (&rarr;[read the documentation](docs/counters.md)).

Author: Jan Mühlig (`jan.muehlig@tu-dortmund.de`)

----

## Getting Started

### Quick Examples
Capture performance counters and samples directly within your C++ application, focusing exclusively on the code crucial to your analysis.

&rarr; Further details are available in the [documentation](docs/README.md).

#### Record Counters
The `perf::EventCounter` class offers an interface to record hardware performance counter statistics over a specific code segment – comparable to the `perf stat` command.
You can add and manage counters, as well as to start and stop recordings.

&rarr; See the documentation for [recording basics](docs/recording.md) and [multithreaded](docs/recording-parallel.md) recording.

```cpp
#include <perfcpp/event_counter.h>
auto counter_definitions = perf::CounterDefinition{};
auto event_counter = perf::EventCounter{ counter_definitions };

/// Add performance counters.
event_counter.add({"instructions", "cycles", "cache-misses"});

event_counter.start();
/// your code that will be measured is here...
event_counter.stop();

const auto result = event_counter.result();
for (const auto [name, value] : result)
{
    std::cout << name << ": " << value << std::endl;
}

/// Possible output:
// instructions: 5.97298e+07
// cycles: 5.02462e+08
// cache-misses: 1.36517e+07
```

#### Sampling
The `perf::Sampler` class provides an interface to specify sampling criteria and control the start/stop of recordings – comparable to `perf [mem|c2c] record` and `perf report`; but with control of the recorded code segments.
You can sample various aspects such as instructions, time, memory addresses, access latency, call chains, branches, and more.

&rarr; See the documentation for [sampling basics](docs/sampling.md) and [multithreaded sampling](docs/sampling-parallel.md).

```cpp
#include <perfcpp/sampler.h>
auto counter_definitions = perf::CounterDefinition{};
auto sampler = perf::Sampler{ counter_definitions };

/// Add trigger: an overflow of the 'cycles' counter will lead to writing a sample.
sampler.trigger("cycles");

/// Add what to record to samples: Timestamp, CPU ID, and instruction pointer.
sampler.values()
    .time(true)
    .cpu_id(true)
    .instruction_pointer(true);

/// Start sampling.
sampler.start();
/// your code that will be sampled is here...
sampler.stop();

/// Print the samples with the fields we specified.
const auto samples = sampler.result();
for (const auto& sample_record : samples)
{
    const auto time = sample_record.time().value();
    const auto cpu_id = sample_record.cpu_id().value();
    const auto instruction_pointer = sample_record.instruction_pointer().value();
    
    std::cout 
        << "Time = " << time << " | CPU = " << cpu_id
        << " | Instruction Pointer = 0x" << std::hex << instruction_pointer << std::dec
        << std::endl;
}

/// Close sampler to free buffer and close counter.
sampler.close();

/// Possible output:
// Time = 365449130714033 | CPU = 8 | Instruction Pointer = 0x5a6e84b2075c
// Time = 365449130913157 | CPU = 8 | Instruction Pointer = 0x64af7417c75c
// Time = 365449131112591 | CPU = 8 | Instruction Pointer = 0x5a6e84b2075c
// Time = 365449131312005 | CPU = 8 | Instruction Pointer = 0x64af7417c75c 
// ...
```

### Building this library
&rarr; [More details are here](docs/build.md).

```
/// 1) Clone the repository
git clone https://github.com/jmuehlig/perf-cpp.git

/// 2) Switch to the cloned folder
cd perf-cpp

/// 3) Generate the Makefile
cmake . -B build -DBUILD_EXAMPLES=1

/// 4) Build the library (only) into the build/ folder
cmake --build build

/// 5) Build the examples (optional)
/// Examples will be compiled to build/examples/bin/
cmake --build build --target examples
```

---

## Documentation
* [Building and including this library](docs/build.md)
* **Recording performance counters**
  * [Overview and basics of Recording performance counters (single threaded)](docs/recording.md)
  * [Recording counters in parallel (multithread / multicore) settings](docs/recording-parallel.md)
  * [Defining and using metrics](docs/metrics.md)
* **Event Sampling**
  * [Overview and basics of event sampling](docs/sampling.md)
  * [Event sampling in parallel (multithread / multicore) settings](docs/sampling-parallel.md)
* [Built-in and hardware-specific performance counters](docs/counters.md)

---

## Sophisticated Code Examples
We provide a variety of examples detailed below. 
Build them effortlessly by running `make examples`. 
All compiled example binaries are located in `examples/bin` and can be executed directly without additional arguments.

* Code example for recording counters on a [single thread: `examples/single_thread.cpp`](examples/single_thread.cpp)
* Code example for recording counters on [multiple threads: `examples/multi_thread.cpp`](examples/multi_thread.cpp)
* Code example for recording counters on  [multiple threads through inheritance: `examples/inherit_thread.cpp`](examples/inherit_thread.cpp)
* Code example for sampling [counter values: `counter_sampling.cpp`](examples/counter_sampling.cpp)
* Code example for sampling [instruction pointers: `instruction_pointer_sampling.cpp`](examples/instruction_pointer_sampling.cpp)
* Code example for sampling [memory addresses: `address_sampling.cpp`](examples/address_sampling.cpp)
* Code example for sampling [branches: `branch_sampling.cpp`](examples/branch_sampling.cpp)
* Code example for sampling [register values: `register_sampling.cpp`](examples/register_sampling.cpp)
* Code example for [multithreaded sampling: `multi_thread_sampling.cpp`](examples/multi_thread_sampling.cpp)
* Code example for [multicore sampling: `multi_cpu_sampling.cpp`](examples/multi_cpu_sampling.cpp)

## System Requirements
* Minimum *Linux Kernel version*: `>= 5.4`
* Recommended *Linux Kernel version*: `>= 5.13` (for older Kernel versions see below)
* Installed `perf` (check if `perf stat -- ls` provides any output, otherwise follow the instructions)

### Notes for Linux Kernel < 5.13
#### Linux Kernel < 5.13
The counter `cgroup-switches` is only provided since Kernel `5.13`.
If you have an older Kernel, the counter cannot be used and will be deactivated.

#### Linux Kernel < 5.12
Sampling *weight as struct* (`Type::WeightStruct`, see [sampling documentation](docs/sampling.md)) is only provided since Kernel `5.12`.
However, you can sample for weight using `Type::Weight`. To avoid compilation errors, you have to define


    -DNO_PERF_SAMPLE_WEIGHT_STRUCT


when compiling the binary that links `perf-cpp`. This is not true for the examples (will be done automatically).

#### Linux Kernel < 5.11
Sampling *data page size* and *code page size*  (see [sampling documentation](docs/sampling.md)) is only provided since Kernel `5.11`.
If you have an older Kernel **and** you want to link the library, you need to define


    -DNO_PERF_SAMPLE_DATA_PAGE_SIZE -DNO_PERF_SAMPLE_CODE_PAGE_SIZE


when compiling the binary that links `perf-cpp`. This is not true for the examples (will be done automatically).

---

## Other Noteworthy Profiling Projects
While *perf-cpp* is dedicated to providing developers with clear insights into application performance, it is part of a broader ecosystem of tools that facilitate performance analysis. 
Below is a non-exhaustive list of some other valuable profiling projects:

* [PAPI](https://github.com/icl-utk-edu/papi) offers access not only to CPU performance counters but also to a variety of other hardware components including GPUs, I/O systems, and more.
* Intel's [Instrumentation and Tracing Technology](https://github.com/intel/ittapi) allows applications to manage the collection of trace data effectively when used in conjunction with [Intel VTune Profiler](https://www.intel.com/content/www/us/en/developer/tools/oneapi/vtune-profiler.html).
* [PerfEvent](https://github.com/viktorleis/perfevent) provides lightweight access to performance counters, facilitating streamlined performance monitoring.
* For those who prefer a more hands-on approach, the [perf_event_open](https://man7.org/linux/man-pages/man2/perf_event_open.2.html) system call can be utilized directly without any wrappers.

---

## Feedback
Feedback, feature requests, and contributions are always appreciated. 
If you have any feedback, please feel free to email me at `jan.muehlig@tu-dortmund.de`, or you can directly submit a pull request.
