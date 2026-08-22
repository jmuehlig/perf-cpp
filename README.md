# perf-cpp: Hardware Performance Monitoring for C++
![Apache-2.0](https://img.shields.io/github/license/jmuehlig/perf-cpp?) ![LinuxKernel->=4.0](https://img.shields.io/badge/Linux_Kernel-%3E%3D4.0-yellow)
![C++17](https://img.shields.io/badge/C++-17-00599C?logo=cplusplus) [![Build and Test](https://github.com/jmuehlig/perf-cpp/actions/workflows/build-and-test.yml/badge.svg)](https://github.com/jmuehlig/perf-cpp/actions/workflows/build-and-test.yml) [![Ask DeepWiki](https://deepwiki.com/badge.svg)](https://deepwiki.com/jmuehlig/perf-cpp)

[Quick Start](#quick-start) | [Building](#building) | [Documentation](https://jmuehlig.github.io/perf-cpp) | [System Requirements](#system-requirements)

**perf-cpp** lets you profile specific parts of your code, *not the entire program*.

Tools like [Linux Perf](https://perfwiki.github.io/main/), [Intel® VTune™](https://www.intel.com/content/www/us/en/developer/tools/oneapi/vtune-profiler.html), and [AMD uProf](https://www.amd.com/en/developer/uprof.html) profile everything: application startup, configuration parsing, data loading, and all your helper functions.
**perf-cpp** is different: place `start()` and `stop()` **around exactly the code you want to measure**.
Profile one sorting algorithm, or count cache misses in a single hash table lookup.
Wrap two memory allocators separately, and you get a fair comparison.

## Features
Built around Linux's [*perf subsystem*](https://man7.org/linux/man-pages/man2/perf_event_open.2.html), **perf-cpp** supports counting and sampling hardware events for specific code blocks:

- **Record hardware events** like `perf stat`, but only around the code you care about ([documentation](https://jmuehlig.github.io/perf-cpp/recording/))
- **Calculate metrics** like cycles per instruction or cache miss ratios from the counters ([documentation](https://jmuehlig.github.io/perf-cpp/metrics/))
- **Read counter values without stopping the counter** for low-overhead measurements in tight loops ([documentation](https://jmuehlig.github.io/perf-cpp/recording-live-events/))
- **Sample instructions and memory accesses** like `perf record` and `perf mem record`, but targeted at specific functions ([documentation](https://jmuehlig.github.io/perf-cpp/sampling/))
- **Export and visualize results**: [write samples to CSV](https://jmuehlig.github.io/perf-cpp/sampling-export-to-csv/), [generate flame graphs](https://jmuehlig.github.io/perf-cpp/sampling-symbols-and-flamegraphs/), or [correlate memory accesses with specific data structures](https://jmuehlig.github.io/perf-cpp/sampling-memory-analysis/)
- **Mix built-in events** like cycles and cache misses **with processor-specific PMU events** ([documentation](https://jmuehlig.github.io/perf-cpp/counters/))

See the **[examples](examples/README.md)** and **[full documentation](https://jmuehlig.github.io/perf-cpp/)** for details.

## Quick Start
### Record Hardware Event Statistics

```cpp
#include <perfcpp/event_counter.hpp>

// Initialize the counter
auto event_counter = perf::EventCounter{};

// Specify events to count
event_counter.add({"seconds", "instructions", "cycles", "cache-misses"});

// Run the workload
event_counter.start();
code_to_profile(); // <-- Statistics recorded during execution
event_counter.stop();

// Print the result to the console
const auto result = event_counter.result();
for (const auto [event_name, value] : result)
{
    std::cout << event_name << ": " << value << std::endl;
}
```

Example output:
```
seconds:      0.0955897
instructions: 5.92087e+07
cycles:       4.70254e+08
cache-misses: 1.35633e+07
```

> [!NOTE]
> See the guides on **[recording event statistics](https://jmuehlig.github.io/perf-cpp/recording/)** and **[event statistics on multiple CPUs/threads](https://jmuehlig.github.io/perf-cpp/recording-parallel/)**.
> Check out the **[hardware events](https://jmuehlig.github.io/perf-cpp/counters/)** documentation for built-in and processor-specific events.

### Record Samples

```cpp
#include <perfcpp/sampler.hpp>

// Create the sampler
auto sampler = perf::Sampler{};

// Specify when a sample is recorded: every 50,000th cycle
sampler.trigger("cycles", perf::Period{50000U});

// Specify what data is included in a sample: time, CPU ID, instruction
sampler.values()
    .timestamp(true)
    .cpu_id(true)
    .logical_instruction_pointer(true);

// Run the workload
sampler.start();
code_to_profile(); // <-- Samples recorded during execution
sampler.stop();

const auto samples = sampler.result();

// Export samples to CSV.
samples.to_csv("samples.csv");

// Or iterate samples directly.
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

Example output:
```
Time = 365449130714033 | CPU = 8 | Instruction = 0x5a6e84b2075c
Time = 365449130913157 | CPU = 8 | Instruction = 0x64af7417c75c
Time = 365449131112591 | CPU = 8 | Instruction = 0x5a6e84b2075c
Time = 365449131312005 | CPU = 8 | Instruction = 0x64af7417c75c
```

> [!NOTE]
> See the **[sampling guide](https://jmuehlig.github.io/perf-cpp/sampling/)** for what data you can record.
> Also check out the **[sampling on multiple CPUs/threads guide](https://jmuehlig.github.io/perf-cpp/sampling-parallel/)** for parallel sampling.

## Building
*perf-cpp* can be built as a static or shared library.

```bash
git clone https://github.com/jmuehlig/perf-cpp.git
cd perf-cpp
cmake . -B build
cmake --build build
```

> [!NOTE]
> See the **[building guide](https://jmuehlig.github.io/perf-cpp/build/)** for CMake integration and build options.

## Documentation

The full documentation is available at **[jmuehlig.github.io/perf-cpp](https://jmuehlig.github.io/perf-cpp/)**.

See also: **[Examples](examples/README.md)** | **[Changelog](CHANGELOG.md)**

## System Requirements
- *GCC* **11** or newer, or *Clang* **14** or newer, with **C++17** support.
- *CMake* version **3.10** or higher.
- *Linux Kernel* **4.0** or newer (some features require a newer kernel).
- `perf_event_paranoid` setting: Adjust as needed to allow access to performance counters (see the [perf paranoid](https://jmuehlig.github.io/perf-cpp/perf-paranoid/) documentation).
- *Python 3*, if you use [processor-specific hardware event generation](https://jmuehlig.github.io/perf-cpp/build/#auto-generating-events-at-compile-time).

## Contributing
Contributions are welcome. Open an issue or submit a pull request.

To build and run the tests:
```bash
cmake . -B build -DBUILD_TESTS=ON
cmake --build build --target tests
./build/bin/tests
```

> [!NOTE]
> Most tests require access to hardware performance counters via `perf_event_open`. If your system restricts access (e.g., in containers or VMs), some tests will fail. See the [perf paranoid](https://jmuehlig.github.io/perf-cpp/perf-paranoid/) documentation.

For questions or feedback: `jan.muehlig@tu-dortmund.de`.

---

## Related Projects
Other profiling tools:

- [PAPI](https://github.com/icl-utk-edu/papi) monitors CPU counters, GPUs, I/O, and more.
- [Likwid](https://github.com/RRZE-HPC/likwid) is a set of command-line tools for benchmarking with an extensive [wiki](https://github.com/RRZE-HPC/likwid/wiki).
- [PerfEvent](https://github.com/viktorleis/perfevent) is a lightweight wrapper for performance counters.
- Intel's [Instrumentation and Tracing Technology](https://github.com/intel/ittapi) lets you control [Intel VTune Profiler](https://www.intel.com/content/www/us/en/developer/tools/oneapi/vtune-profiler.html) from your code.
- Want to go lower-level? Use [perf_event_open](https://man7.org/linux/man-pages/man2/perf_event_open.2.html) directly.

## Resources on Profiling

### Academic Papers
- [Quantitative Evaluation of Intel PEBS Overhead for Online System-Noise Analysis](https://soramichi.jp/pdf/ROSS2017.pdf) (2017)
- [Analyzing memory accesses with modern processors](https://dl.acm.org/doi/abs/10.1145/3399666.3399896) (2020)
- [On the Precision of Precise Event Based Sampling](https://dl.acm.org/doi/10.1145/3409963.3410490) (2020)
- [CachePerf: A Unified Cache Miss Classifier via Hybrid Hardware Sampling](https://dl.acm.org/doi/10.1145/3530901) (2022)
- [Precise Event Sampling on AMD Versus Intel: Quantitative and Qualitative Comparison](https://ieeexplore.ieee.org/stamp/stamp.jsp?arnumber=10068807&tag=1) (2023)
- [Efficient Cross-platform Multiplexing of Hardware Performance Counters via Adaptive Grouping](https://dl.acm.org/doi/full/10.1145/3629525) (2024)
- [Multi-level Memory-Centric Profiling on ARM Processors with ARM SPE](https://arxiv.org/html/2410.01514v1) (2024)
- [Breaking the Cycle - A Short Overview of Memory-Access Sampling Differences on Modern x86 CPUs](https://dl.acm.org/doi/pdf/10.1145/3736227.3736241) (2025)

### Blog Posts
- [C2C - False Sharing Detection in Linux Perf](https://joemario.github.io/blog/2016/09/01/c2c-blog/) (2016)
- [PMU counters and profiling basics](https://easyperf.net/blog/2018/06/01/PMU-counters-and-profiling-basics) (2018)
- [Advanced profiling topics. PEBS and LBR](https://easyperf.net/blog/2018/06/08/Advanced-profiling-topics-PEBS-and-LBR) (2018)
- [The Linux perf Event Scheduling Algorithm](https://hadibrais.wordpress.com/2019/09/06/the-linux-perf-event-scheduling-algorithm/) (2019)
- [Performance Speed Limits](https://travisdowns.github.io/blog/2019/06/11/speed-limits.html) (2019)
- [Detect false sharing with Data Address Profiling](https://easyperf.net/blog/2019/12/17/Detecting-false-sharing-using-perf) (2019)
- [Data-type profiling for perf](https://lwn.net/Articles/955709/) (2023)
- [Analyze cache behavior with Perf C2C on Arm](https://learn.arm.com/learning-paths/servers-and-cloud-computing/false-sharing-arm-spe/) (2023)
- [How Small Can a Measured Region Be Before perf Counters Lie?](https://jmuehlig.github.io/when-can-you-trust-your-hardware-counters/) (2026)

