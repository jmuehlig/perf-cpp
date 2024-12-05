# perf-cpp: Access Performance Counters from C++ Applications
Welcome to *perf-cpp*, a C++ library designed to streamline the use of the Linux perf subsystem, providing direct access to hardware performance counters from within the application. 
Many modern profiling tools fail to offer precise profiling of specific code segments and to associate profiled data like memory addresses with application-specific details. 
With *perf-cpp*, you can manage profiling directly within your application and handle the profiled data seamlessly.


## Key Features
* **[Count Hardware Events](docs/recording.md)**: Integrate performance monitoring seamlessly into your development process. Directly interact with hardware counters to focus on critical code segments.
* **[Sampling](docs/sampling.md)**: Leverage sampling to gather performance data periodically, e.g., instruction pointers, memory addresses, load and store latency, branches, registers, and more.
* **[Customizable Event Configuration](docs/counters.md)**: Extend the built-in hardware events (e.g., cache-misses) with those specific to your underlying hardware substrate. Additionally, define and utilize  **[Metrics](docs/metrics.md)**–quantitative measurements like *cycles per instruction*–to gain deeper insights into performance and efficiency.
* **[Practical Examples](examples/README.md)**: Jumpstart your implementation with our diverse collection of examples that demonstrate practical applications of the library.


## Quick Start
Get up and running with *perf-cpp* in seconds:

```bash
# Clone the repository
git clone https://github.com/jmuehlig/perf-cpp.git

# Switch to the repository folder
cd perf-cpp

# Optional: Switch to the latest stable version
git checkout v0.9.0

# Build the library (in build/)
cmake . -B build -DBUILD_EXAMPLES=1
cmake --build build

# Optional: Build examples (in build/examples/bin)
cmake --build build --target examples
```

For detailed building instructions, including how to integrate *perf-cpp* into your *CMake* projects, visit our **[build guide](docs/build.md)**.

## Usage Examples
### Count Hardware Events
Quickly set up hardware event monitoring:
```cpp
#include <perfcpp/event_counter.h>

/// Initialize the counter
auto counters = perf::CounterDefinition{};
auto event_counter = perf::EventCounter{ counters };

/// Specify hardware events to count
event_counter.add({"seconds", "instructions", "cycles", "cache-misses"});

/// Run the workload
event_counter.start();
your_workload(); /// <-- Your code to profile
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

For further details, including how to count events in parallel settings, visit our **[guide on recording events](docs/recording.md)**.

### Record Samples
Implement detailed sampling with control over the recorded content:

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
your_workload(); /// <-- Your code to profile
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

Possible output:
```
Time = 365449130714033 | CPU = 8 | Instruction = 0x5a6e84b2075c
Time = 365449130913157 | CPU = 8 | Instruction = 0x64af7417c75c
Time = 365449131112591 | CPU = 8 | Instruction = 0x5a6e84b2075c
Time = 365449131312005 | CPU = 8 | Instruction = 0x64af7417c75c 
```

For further details, for example, which metrics can be included into samples, visit our **[sampling guide](docs/sampling.md)**.

### Advanced Examples
We include a comprehensive collection of examples demonstrating the advanced capabilities of *perf-cpp*, including, for example, [counting events in parallel settings](examples/multi_thread.cpp) and [sampling memory accesses](examples/address_sampling.cpp).

All code examples are available in the [examples/](examples) folder.


## Further Reading
* **[Full Documentation](docs/README.md)**: Explore detailed guides on every feature of *perf-cpp*.
* **[Examples](examples/README.md)**: Learn how to set up different features from code-examples.
* **[Changelog](CHANGELOG.md)**: Stay updated with the latest changes and improvements.

## System Requirements
* C++ Standard: Requires support for **C++17** features.
* CMake Version: **3.10** or higher.
* Linux Kernel Version: **4.0** or newer (note that some features need a newer Kernel).
* `perf_event_paranoid` Setting: Adjust as needed to allow access to performance counters (see the [Paranoid Value Section](#adjusting-perf_event_paranoid-value) below).

### Adjusting `perf_event_paranoid` Value
The `perf_event_paranoid` setting controls access to performance counters:
* `-1`: No restrictions (full access). 
* `0`: Allow normal users access, but no raw tracepoint samples. 
* `1`: Allow user and kernel-level profiling (default since Linux 4.6). 
* `>= 2`: Only user-level measurements allowed.

#### Checking the Current Value
```bash
cat /proc/sys/kernel/perf_event_paranoid
```

#### Changing the Value Temporarily
```bash
sudo sysctl -w kernel.perf_event_paranoid=-1
```

**Note**: To make this change permanent, edit `/etc/sysctl.conf`  and add `kernel.perf_event_paranoid = -1`.

## Contribute and Contact
We welcome contributions and feedback to make *perf-cpp* even better.
For feature requests, feedback, or bug reports, please reach out via our issue tracker or submit a pull request.

Alternatively, you can email me: `jan.muehlig@tu-dortmund.de`.

---

## Further Profiling Projects
While *perf-cpp* is dedicated to providing developers with clear insights into application performance, it is part of a broader ecosystem of tools that facilitate performance analysis. 
Below is a non-exhaustive list of some other valuable profiling projects:

* [PAPI](https://github.com/icl-utk-edu/papi) offers access not only to CPU performance counters but also to a variety of other hardware components including GPUs, I/O systems, and more.
* [Likwid](https://github.com/RRZE-HPC/likwid) is a collection of several command line tools for benchmarking, including an extensive [wiki](https://github.com/RRZE-HPC/likwid/wiki).
* [PerfEvent](https://github.com/viktorleis/perfevent) provides lightweight access to performance counters, facilitating streamlined performance monitoring.
* Intel's [Instrumentation and Tracing Technology](https://github.com/intel/ittapi) allows applications to manage the collection of trace data effectively when used in conjunction with [Intel VTune Profiler](https://www.intel.com/content/www/us/en/developer/tools/oneapi/vtune-profiler.html).
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

