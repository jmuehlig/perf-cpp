# *perf-cpp*: Performance Counter Wrapper for C++

Welcome to *perf-cpp*, a streamlined C++ library designed to make performance analysis more intuitive and focused. 
At its core, *perf-cpp* serves as a wrapper for the [perf_event_open](https://man7.org/linux/man-pages/man2/perf_event_open.2.html) function, enabling engineers to track performance counters at a fine-granular level within specific blocks of code. 
Unlike traditional tools (like *perf*) that provide metrics the entire application, *perf-cpp* offers the precision needed to pinpoint performance issues and optimize code-segments effectively.

Author: Jan Mühlig (`jan.muehlig@tu-dortmund.de`)

----

## Key Features
* **Simplified Performance Measurement**: Directly integrate with CPU performance counters into your C++ application, using a more accessible interface.
* **Fine-granular Metrics**: Focus your performance analysis on specific code segments.
* **Event Sampling**: Leverage sampling to gather performance data periodically, enabling efficient analysis of resource usage (including instruction pointers, data, branches, registers, and more) over time and/or execution.
* **Customizable Counter Management**: Easily extend the built-in counters with those specific to your underlying hardware substrate.

## Documentation
* [Building and including this library](docs/build.md)
* **Recording performance counters**
    * [Overview (single threaded)](docs/recording-performance-counters.md)
    * [Multithreaded](docs/recording-performance-counters-multithreaded.md)
    * [Defining and using metrics](docs/metrics.md)
* [Event sampling](docs/sampling.md)
* [Built-in and hardware-specific performance counters](docs/counters.md)

---

## Getting Started
### Building this library
&rarr; [More details are here](docs/build.md).

```
/// 1) Clone the repository
git clone https://github.com/jmuehlig/perf-cpp.git

/// 2) Switch to the cloned folder
cd perf-cpp

/// 3) Generate the Makefile
cmake .

/// 4) Build the library and examples
make
```

### Quick Examples
Capture performance counters and samples directly within your C++ application, focusing exclusively on the code crucial to your analysis.

&rarr; Further details are available in the [documentation](docs/README.md).

#### Record Counters
The perf::EventCounter class offers an interface to add and manage counters, as well as to start and stop recordings. 
Utilize predefined counters or customize counters specific to your hardware.

&rarr; See the documentation for [single-threaded](docs/recording-performance-counters.md) and [multithreaded](docs/recording-performance-counters-multithreaded.md) recording.

```cpp
#include <perfcpp/event_counter.h>
auto counter_definitions = perf::CounterDefinition{};
auto event_counter = perf::EventCounter{counter_definitions};
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
The `perf::Sampler` class delivers an interface to specify sampling criteria and control the start/stop of recordings. 
You can sample various aspects such as instructions, time, memory addresses, access latency, call chains, branches, and more.

&rarr; See the documentation for [sampling](docs/sampling.md).

```cpp
#include <perfcpp/sampler.h>
auto counter_definitions = perf::CounterDefinition{};

auto sample_config = perf::SampleConfig{};
perf_config.period(1000U);

auto sampler = perf::Sampler{
    counter_definitions,
    "cycles",
    perf::Sampler::Type::Time | perf::Sampler::Type::InstructionPointer 
        | perf::Sampler::Type::CPU
};

sampler.start();
/// your code that will be sampled is here...
sampler.stop();

const auto samples = sampler.result();
for (const auto& sample : samples)
{
    std::cout 
        << "Time = " << sample.time().value() 
        << " | Instruction = 0x" << std::hex << sample.instruction_pointer().value() << std::dec
        << " | CPU = " << sample.cpu().value()
        << std::endl;
}

/// Possible output:
// Time = 365449130714033 | Instruction = 0x5a6e84b2075c | CPU = 8
// Time = 365449130913157 | Instruction = 0x64af7417c75c | CPU = 8
// Time = 365449131112591 | Instruction = 0x5a6e84b2075c | CPU = 8
// Time = 365449131312005 | Instruction = 0x64af7417c75c | CPU = 8
// ...
```

---

## Further Examples
We provide a variety of examples detailed below. 
Build them effortlessly by running `make`. 
All compiled example binaries are located in examples/bin and can be executed directly without additional arguments.

* Example for recording counters on a [single thread: `examples/single_thread.cpp`](examples/single_thread.cpp)
* Example for recording counters on [multiple threads: `examples/multi_thread.cpp`](examples/multi_thread.cpp)
* Example for recording counters on  [multiple threads through inheritance: `examples/inherit_thread.cpp`](examples/inherit_thread.cpp)
* Example for sampling [counter values: `counter_sampling.cpp`](examples/counter_sampling.cpp)
* Example for sampling [instruction pointers: `instruction_pointer_sampling.cpp`](examples/instruction_pointer_sampling.cpp)
* Example for sampling [memory addresses: `address_sampling.cpp`](examples/address_sampling.cpp)
* Example for sampling [branches: `branch_sampling.cpp`](examples/branch_sampling.cpp)
* Example for sampling [register values: `register_sampling.cpp`](examples/register_sampling.cpp)

## System Requirements
* Minimum *Linux Kernel version*: `>= 5.4`
* Recommended *Linux Kernel version*: `>= 5.13` (for older Kernel versions see below)
* Installed `perf` (check if `perf stat ls` provides any output, otherwise follow the instructions)

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
