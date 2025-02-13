# perf-cpp: Access Performance Counters from C++ Applications
*perf-cpp* provides **direct access to hardware performance counters** from your C++ application.
The library allows for precise event-counting and sampling of specific code segments and to link sampled data (e.g., memory addresses) with application-specific details (e.g., class instances).

## Key Features
* **[Count Hardware Events](docs/recording.md)**: Seamlessly embed performance statistics (similar to `perf stat`) into your app and profile specific code segments instead of the entire application. *perf-cpp* also supports **[Metrics](docs/metrics.md)** (e.g., *cycles per instruction*) and accessing **[Statistics in Realtime](docs/recording-live-events.md)**.
* **[Record Samples](docs/sampling.md)**: Periodically capture profiling data–such as instruction pointers and memory accesses–directly from your application (similar to `perf [mem] record`).
* **[Customizable Event Configuration](docs/counters.md)**: Mix built-in events (e.g., *cycles*, *instructions*, *cache-misses*) with CPU-specific ones.
* **[Practical Examples](examples/README.md)**: Jumpstart your implementation with the diverse collection of examples that demonstrate practical applications of the library.


## Building
*perf-cpp* is designed as a library that can be linked to your application.

```bash
# Clone the repository
git clone https://github.com/jmuehlig/perf-cpp.git

# Switch to the repository folder
cd perf-cpp

# Optional: Switch to the latest stable version
git checkout v0.9.0

# Build the library (in build/)
# Note: -DBUILD_EXAMPLES=1 is optional and only needed if you want to build the examples
cmake . -B build -DBUILD_EXAMPLES=1
cmake --build build

# Optional: Build examples (in build/examples/bin)
cmake --build build --target examples
```

> [!NOTE]
> Further information and detailed building instructions (e.g., how to integrate into *CMake* projects) are available in the **[Building Guide](docs/build.md)**.

## Usage Examples
### Record Hardware Event Statistics
Recording hardware event statistics operates much like `perf stat`: it quantifies critical events—such as executed *instructions*, CPU *cycles*, and *cache misses*–throughout a code segment's execution.

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

> [!NOTE]
> For additional insights please refer to the guides on **[Recording Events](docs/recording.md)** and **[Recording Events on Multiple CPUs/Threads](docs/recording-parallel.md)**. 
> Also, check out the **[Hardware Events](docs/counters.md)** documentation for comprehensive details on both built-in and hardware-specific events.

### Record Samples
Recording samples functions much like `perf [mem] record`: it captures execution snapshots, e.g., the *instruction pointer*, executing *CPU*, and *timestamp*, at regular intervals (here every `4,000`th CPU cycle).

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

> [!NOTE]
> For additional details—such as the types of data that can be included in samples—please consult the **[Sampling Guide](docs/sampling.md)**.
> Additionally, consult the **[Sampling on Multiple CPUs/Threads Guide](docs/sampling-parallel.md)** for guidance on parallel sampling. 

### Advanced Examples
We include a comprehensive collection of examples demonstrating the advanced capabilities of *perf-cpp*, including, for example, [counting events in parallel settings](examples/multi_thread.cpp) and [sampling memory accesses](examples/address_sampling.cpp).

> [!TIP]
> All code examples are available in the [examples/](examples) folder.


## Further Reading
* **[Full Documentation](docs/README.md)**: Explore detailed guides on every feature of *perf-cpp*.
* **[Examples](examples/README.md)**: Learn how to set up different features from code-examples.
* **[Changelog](CHANGELOG.md)**: Stay updated with the latest changes and improvements.

## System Requirements
* C++ Standard: Requires support for **C++17** features.
* CMake Version: **3.10** or higher.
* Linux Kernel Version: **4.0** or newer (note that some features need a newer Kernel).
* `perf_event_paranoid` setting: Adjust as needed to allow access to performance counters (see the [Paranoid Value](docs/perf-paranoid.md) documentation).

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

