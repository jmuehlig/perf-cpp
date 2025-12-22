# perf-cpp: Effortless Hardware Performance Monitoring for C++ Applications
![LGPL-3.0](https://img.shields.io/github/license/jmuehlig/perf-cpp?) ![LinuxKernel->=4.0](https://img.shields.io/badge/Linux_Kernel-%3E%3D4.0-yellow)
![C++17](https://img.shields.io/badge/C++-17-00599C?logo=cplusplus) [![Ask DeepWiki](https://deepwiki.com/badge.svg)](https://deepwiki.com/jmuehlig/perf-cpp)

[Quick Start](#quick-start) | [How to Build](#building) | [Documentation](#full-documentation) | [System Requirements](#system-requirements) 

**perf-cpp** embeds Linux's hardware performance monitoring directly into your code, letting you profile exactly what matters and process the results in your application.
Tools like [Linux Perf](https://perfwiki.github.io/main/), [Intel® VTune™](https://www.intel.com/content/www/us/en/developer/tools/oneapi/vtune-profiler.html), and [AMD uProf](https://www.amd.com/en/developer/uprof.html) are powerful but monitor entire programs – and high-performance applications need surgical precision.

## What can perf-cpp do?
Built around Linux's powerful [*perf subsystem*](https://man7.org/linux/man-pages/man2/perf_event_open.2.html), **perf-cpp** provides a clean interface for *counting* and *sampling* hardware events – without the complexity of low-level APIs.

- **Measure exactly what you want** – utilize *performance counters* to count hardware events, similar to `perf stat`, but around specific code paths, not an entire binary ([documentation](docs/recording.md)). 
- **Calculate metrics** such as *cycles per instruction* and *cache miss to access ratio* based on hardware events and timing ([documentation](docs/metrics.md)). 
- **Low-latency performance counters access** without starting/stopping the counters, for micro-benchmarks or adaptive tuning ([documentation](docs/recording-live-events.md)).
- **Record instruction and memory samples**, just like `perf [mem] record` – but from inside your application ([documentation](docs/recording-live-events.md)).
- **Correlate samples with data structures and symbols** to generate [per-class access statistics](docs/analyzing-memory-access-patterns.md) and [flame graphs](docs/sampling-symbols-and-flamegraphs.md).
- Mix built-in events (e.g., *cycles*, *instructions*, *cache misses*, ...) with processor-specific counters ([documentation](docs/counters.md)).

See various **[practical examples](examples/README.md)** and the **[documentation](#full-documentation)** for more details.

## Quick Start
### Record Hardware Event Statistics
Recording hardware event statistics operates much like `perf stat`: it quantifies critical events–such as executed *instructions*, CPU *cycles*, and *cache misses*–throughout a code segment's execution.

```cpp
#include <perfcpp/event_counter.h>

/// Initialize the counter
auto event_counter = perf::EventCounter{};

/// Specify hardware events to count
event_counter.add({"seconds", "instructions", "cycles", "cache-misses"});

/// Run the workload
event_counter.start();
code_to_profile(); /// <-- Statistics recorded while execution
event_counter.stop();

/// Print the result to the console
const auto result = event_counter.result();
for (const auto [event_name, value] : result)
{
    std::cout << event_name << ": " << value << std::endl;
}
```

Possible output:
```
seconds:      0.0955897 
instructions: 5.92087e+07
cycles:       4.70254e+08
cache-misses: 1.35633e+07
```

> [!NOTE]
> For additional insights please refer to the guides on **[recording event statistics](docs/recording.md)** and **[event statistics on multiple CPUs/threads](docs/recording-parallel.md)**. 
> Also, check out the **[hardware events](docs/counters.md)** documentation for details on both built-in and processor-specific events.

### Record Samples
Recording samples functions much like `perf [mem] record`: it captures execution snapshots, e.g., the *instruction pointer*, executing *CPU*, and *timestamp*, at regular intervals (here every `50,000`th CPU cycle).

```cpp
#include <perfcpp/sampler.h>

/// Create the sampler
auto sampler = perf::Sampler{};

/// Specify when a sample is recorded: every 50,000th cycle
sampler.trigger("cycles", perf::Period{50000U});

/// Specify what data is included into a sample: time, CPU ID, instruction
sampler.values()
    .timestamp(true)
    .cpu_id(true)
    .instruction_pointer(true);

/// Run the workload
sampler.start();
code_to_profile(); /// <-- Samples recorded while execution
sampler.stop();

/// Print the samples to the console
const auto samples = sampler.result();
for (const auto& record : samples)
{
    const auto timestamp = record.metadata().timestamp().value();
    const auto cpu_id = record.metadata().cpu_id().value();
    const auto instruction = record.instruction_execution().logical_instruction_pointer().value();
    
    std::cout 
        << "Time = " << timestamp << " | CPU = " << cpu_id
        << " | Instruction = 0x" << std::hex << instruction << std::dec
        << std::endl;
}
```

Possible output:
```
Time = 365449130714033 | CPU = 8 | Instruction = 0x5a6e84b2075c
Time = 365449130913157 | CPU = 8 | Instruction = 0x64af7417c75c
Time = 365449131112591 | CPU = 8 | Instruction = 0x5a6e84b2075c
Time = 365449131312005 | CPU = 8 | Instruction = 0x64af7417c75c 
```

> [!NOTE]
> For additional details–such as the types of data that can be included in samples–please consult the **[sampling guide](docs/sampling.md)**.
> Additionally, consult the **[sampling on multiple CPUs/threads guide](docs/sampling-parallel.md)** for instructions on parallel sampling. 

### More Examples
We include a collection of [examples](examples/README.md) demonstrating the functionality and interface of *perf-cpp* in the `examples/` directory, including
- examples for counting hardware events (`examples/statistics`)
- and for sampling (`examples/sampling`).

## Building
*perf-cpp* is designed as a library (static or shared) that can be linked to your application.

```bash
# Clone the repository
git clone https://github.com/jmuehlig/perf-cpp.git

# Switch to the repository folder
cd perf-cpp

# Optional: Switch to this development version
git checkout v0.12.5

# Build the library (in build/)
# -DBUILD_EXAMPLES=1        compiles all examples (optional)
# -DBUILD_LIB_SHARED=1      creates the library as a shared one (optional)
# -DGEN_PROCESSOR_EVENTS=1  generates and compiles a .cpp file that adds events specific to the underlying CPU (optional)
cmake . -B build -DBUILD_EXAMPLES=1
cmake --build build

# Optional: Build examples (in build/examples/bin) if -DBUILD_EXAMPLES=1
cmake --build build --target examples
```

> [!NOTE]
> Further information and detailed building instructions (e.g., how to integrate into *CMake* projects) are available in the **[building guide](docs/build.md)**.

## Full Documentation
- [**Building**](docs/build.md): Integrate *perf-cpp* seamlessly into your C++ projects.
- **Counting Performance Events**
    - [**Basics**](docs/recording.md): Master recording hardware event statistics directly within your application.
    - [**Parallel and Multithreaded**](docs/recording-parallel.md): Learn how to monitor events across threads and CPU cores.
    - [**Metrics**](docs/metrics.md):  Learn how to combine hardware events into meaningful metrics for clearer performance insights.
    - [**Live Access**](docs/recording-live-events.md): See how events can be accessed without stopping the recording, ideal for profiling tight loops and small functions.
- **Recording Samples**
    - [**Basics**](docs/sampling.md): Understand sampling mechanisms, which data to record, and how to access the results.
    - [**Parallel and Multithreaded**](docs/sampling-parallel.md): Learn how to record samples in multithreaded workloads.
    - [**Use the Linux Perf Tool to Analyze Recorded Samples**](docs/analyzing-samples-with-perf-report.md): See how samples recorded via *perf-cpp* can be analyzed with `perf [mem] report`.
    - [**Translating Instruction Pointers into Symbols and Samples into flame graphs**](docs/sampling-symbols-and-flamegraphs.md): See how to translate instruction pointers into function names and prepare sampling results to transform them into flame graphs (e.g., using [FlameGraph](https://github.com/brendangregg/FlameGraph)).
    - [**Analyzing Memory Access Patterns**](docs/analyzing-memory-access-patterns.md): See how to link memory sampling data to specific data objects to profile detailed memory access characteristics.
- [**Built-in and Hardware-specific Events**](docs/counters.md): Discover built-in events and learn how to define new ones tailored to your hardware.
- [**Perf Paranoid**](docs/perf-paranoid.md): Learn how to configure perf permissions.

## Further Reading
- **[Examples](examples/README.md)**: Learn how to set up different features from code-examples.
- **[Changelog](CHANGELOG.md)**: Stay updated with the latest changes and improvements.

## System Requirements
- *Clang* / *GCC* with support for **C++17** features.
- *CMake* version **3.10** or higher.
- *Linux Kernel* **4.0** or newer (note that some features need a newer Kernel).
- `perf_event_paranoid` setting: Adjust as needed to allow access to performance counters (see the [Paranoid Value](docs/perf-paranoid.md) documentation).
- *Python3*, if you make use of [processor-specific hardware event generation](docs/build.md#generate-processor-specific-events).

## Contribute and Contact
We welcome contributions and feedback.
For feature requests, feedback, or bug reports, please reach out via our issue tracker or submit a pull request.

Alternatively, you can email me: `jan.muehlig@tu-dortmund.de`.

---

## Further PMU-related Projects
Below is a non-exhaustive list of some other valuable profiling projects:

- [PAPI](https://github.com/icl-utk-edu/papi) offers access not only to CPU performance counters but also to a variety of other hardware components including GPUs, I/O systems, and more.
- [Likwid](https://github.com/RRZE-HPC/likwid) is a collection of several command line tools for benchmarking, including an extensive [wiki](https://github.com/RRZE-HPC/likwid/wiki).
- [PerfEvent](https://github.com/viktorleis/perfevent) provides lightweight access to performance counters, facilitating streamlined performance monitoring.
- Intel's [Instrumentation and Tracing Technology](https://github.com/intel/ittapi) allows applications to manage the collection of trace data effectively when used in conjunction with [Intel VTune Profiler](https://www.intel.com/content/www/us/en/developer/tools/oneapi/vtune-profiler.html).
- For those who prefer a more hands-on approach, the [perf_event_open](https://man7.org/linux/man-pages/man2/perf_event_open.2.html) system call can be utilized directly without any wrappers.

## Resources about (Perf-) Profiling
This is a non-exhaustive list of academic research papers and blog articles (feel free to add to it, e.g., via pull request – also your own work).

### Academical Papers
- [Quantitative Evaluation of Intel PEBS Overhead for Online System-Noise Analysis](https://soramichi.jp/pdf/ROSS2017.pdf) (2017)
- [Analyzing memory accesses with modern processors](https://dl.acm.org/doi/abs/10.1145/3399666.3399896) (2020)
- [Precise Event Sampling on AMD Versus Intel: Quantitative and Qualitative Comparison](https://ieeexplore.ieee.org/stamp/stamp.jsp?arnumber=10068807&tag=1) (2023)
- [Multi-level Memory-Centric Profiling on ARM Processors with ARM SPE](https://arxiv.org/html/2410.01514v1) (2024)
- [Breaking the Cycle - A Short Overview of Memory-Access Sampling Differences on Modern x86 CPUs](https://dl.acm.org/doi/pdf/10.1145/3736227.3736241) (2025)

### Blog Posts
- [C2C - False Sharing Detection in Linux Perf](https://joemario.github.io/blog/2016/09/01/c2c-blog/) (2016)
- [PMU counters and profiling basics.](https://easyperf.net/blog/2018/06/01/PMU-counters-and-profiling-basics) (2018)
- [Detect false sharing with Data Address Profiling.](https://easyperf.net/blog/2019/12/17/Detecting-false-sharing-using-perf) (2019)
- [Advanced profiling topics. PEBS and LBR.](https://easyperf.net/blog/2018/06/08/Advanced-profiling-topics-PEBS-and-LBR) (2018)

