# perf-cpp: Effortless Hardware Performance Monitoring for C++ Applications
![LGPL-3.0](https://img.shields.io/github/license/jmuehlig/perf-cpp?) ![LinuxKernel->=4.0](https://img.shields.io/badge/Linux_Kernel-%3E%3D4.0-yellow)
![C++17](https://img.shields.io/badge/C++-17-00599C?logo=cplusplus) [![Ask DeepWiki](https://deepwiki.com/badge.svg)](https://deepwiki.com/jmuehlig/perf-cpp)

[Quick Start](#quick-start) | [How to Build](#building) | [Documentation](#full-documentation) | [System Requirements](#system-requirements) 

**perf-cpp** lets you profile for specific parts of your code, *not the entire program*.

Tools like [Linux Perf](https://perfwiki.github.io/main/), [Intel® VTune™](https://www.intel.com/content/www/us/en/developer/tools/oneapi/vtune-profiler.html), and [AMD uProf](https://www.amd.com/en/developer/uprof.html) profile everything: application startup, configuration parsing, data loading, and all your helper functions.
**perf-cpp** is different: place `start()` and `stop()` **around exactly the code you want to measure**. 
Profile one sorting algorithm. 
Measure cache misses in your hash table lookup. 
Compare two memory allocators. 
*Skip all the noise.*

## What can perf-cpp do?
Built around Linux's [*perf subsystem*](https://man7.org/linux/man-pages/man2/perf_event_open.2.html), **perf-cpp** lets you count and sample hardware events for specific code blocks:

- **Count hardware events** like `perf stat`, but only around the code you care about, *not the entire binary* ([documentation](docs/recording.md))
- **Calculate metrics** like cycles per instruction or cache miss ratios from the counters ([documentation](docs/metrics.md))
- **Read counter values without stopping** for low-overhead measurements in tight loops ([documentation](docs/recording-live-events.md))
- **Sample instructions and memory accesses** like `perf [mem] record`, but targeted at specific functions ([documentation](docs/sampling.md))
- **Export and analyze results** in your code: [write samples to CSV](docs/analyzing-samples-with-csv.md), [generate flame graphs](docs/sampling-symbols-and-flamegraphs.md), or [correlate memory accesses with specific data structures](docs/analyzing-memory-access-patterns.md)
- **Mix built-in and processor-specific events** like cycles, cache misses, or vendor PMU features ([documentation](docs/counters.md))

See various **[practical examples](examples/README.md)** and the **[documentation](#full-documentation)** for more details.

## Quick Start
### Record Hardware Event Statistics
Count hardware events like `perf stat`—instructions, cycles, cache misses—while your code runs.

```cpp
#include <perfcpp/event_counter.hpp>

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
> See the guides on **[recording event statistics](docs/recording.md)** and **[event statistics on multiple CPUs/threads](docs/recording-parallel.md)**.
> Check out the **[hardware events](docs/counters.md)** documentation for built-in and processor-specific events.

### Record Samples
Record snapshots like `perf [mem] record`—instruction pointer, CPU, timestamp—every 50,000 cycles.

```cpp
#include <perfcpp/sampler.hpp>

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

const auto samples = sampler.result();

/// Materialize samples as CSV (-> analyze with python etc) ...
samples.to_csv("samples.csv");

/// ... or print the samples to the console
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
> See the **[sampling guide](docs/sampling.md)** for what data you can record.
> Also check out the **[sampling on multiple CPUs/threads guide](docs/sampling-parallel.md)** for parallel sampling. 

### More Examples
We have [examples](examples/README.md) showing how *perf-cpp* works in the `examples/` directory:
- counting hardware events (`examples/statistics`)
- sampling (`examples/sampling`)

## Building
*perf-cpp* is designed as a library (static or shared) that can be linked to your application.

```bash
# Clone the repository
git clone https://github.com/jmuehlig/perf-cpp.git

# Switch to the repository folder
cd perf-cpp

# Optional: Switch to this development version
git checkout v0.13-dev

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
> See the **[building guide](docs/build.md)** for how to integrate *perf-cpp* into *CMake* projects.

## Full Documentation
- [**Building**](docs/build.md): Integrate *perf-cpp* into your C++ projects.
- **Counting Performance Events**
    - [**Basics**](docs/recording.md): Record hardware event statistics directly in your application (like `perf stat` but fine-grained).
    - [**Parallel and Multithreaded**](docs/recording-parallel.md): Monitor events across threads and CPU cores.
    - [**Metrics**](docs/metrics.md): Combine hardware events into metrics for better analysis.
    - [**Live Access**](docs/recording-live-events.md): Read counters without stopping, great for tight loops.
- **Recording Samples**
    - [**Basics**](docs/sampling.md): Record samples for specific code paths (like `perf record` and `perf mem record` but fine-grained).
    - [**Parallel and Multithreaded**](docs/sampling-parallel.md): Record samples across multiple threads and CPU cores.
- **Analyzing Samples**
    - [**CSV Export**](docs/analyzing-samples-with-csv.md): Export samples for analysis with statistical tools, spreadsheets, or custom scripts.
    - [**Linux Perf Tools**](docs/analyzing-samples-with-perf-report.md): Analyze samples with `perf report` and `perf mem report`.
    - [**Flame Graphs**](docs/sampling-symbols-and-flamegraphs.md): Translate instruction pointers to symbols and generate flame graphs.
    - [**Memory Access Patterns**](docs/analyzing-memory-access-patterns.md): Link samples to data objects for per-instance memory profiling.
- [**Built-in and Hardware-specific Events**](docs/counters.md): Built-in events and how to add new ones for your CPU.
- [**Perf Paranoid**](docs/perf-paranoid.md): Configure perf permissions.

## Further Reading
- **[Examples](examples/README.md)**: See how to set up different features.
- **[Changelog](CHANGELOG.md)**: See what's new.

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
Other profiling tools:

- [PAPI](https://github.com/icl-utk-edu/papi) monitors CPU counters, GPUs, I/O, and more.
- [Likwid](https://github.com/RRZE-HPC/likwid) is a set of command-line tools for benchmarking with an extensive [wiki](https://github.com/RRZE-HPC/likwid/wiki).
- [PerfEvent](https://github.com/viktorleis/perfevent) is a lightweight wrapper for performance counters.
- Intel's [Instrumentation and Tracing Technology](https://github.com/intel/ittapi) lets you control [Intel VTune Profiler](https://www.intel.com/content/www/us/en/developer/tools/oneapi/vtune-profiler.html) from your code.
- Want to go lower-level? Use [perf_event_open](https://man7.org/linux/man-pages/man2/perf_event_open.2.html) directly.

## Resources about (Perf-) Profiling
Papers and articles about profiling (feel free to add your own via pull request):

### Academic Papers
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

