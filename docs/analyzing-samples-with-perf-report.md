# Analyzing Samples with Linux Perf Tools

*perf-cpp* allows you to export your collected samples into the standard `perf.data` file format, enabling seamless analysis using the full ecosystem of Linux perf tools.

Unlike `perf [mem] record`, which profiles your entire application**, *perf-cpp* allows you to **sample only the specific code sections you care about**, then export that targeted data for analysis with standard Linux perf tools like `perf report`, `perf mem report`, and flame graph generators.

This gives you the best of both worlds:
* **Surgical precision**: Record only critical code paths, hot loops, or specific algorithms
* **Comprehensive analysis**: Use the full perf ecosystem for visualization and reporting
* **Reduced noise**: Eliminate irrelevant samples from application startup, I/O waits, or unrelated code

&rarr; [For a practical implementation, check out our perf data export example.](../examples/sampling/perf_record.cpp)

> [!IMPORTANT]
> This feature is considered as being **experimental**.


---
## Table of Contents
- [Overview](#overview)
- [Exporting Samples to Perf Data Files](#exporting-samples-to-perf-data-files)
    - [Single Sampler Export](#single-sampler-export)
    - [Multi-Sampler Export](#multi-sampler-export)
- [Analyzing the Exported Data](#analyzing-the-exported-data)
    - [Basic Performance Analysis](#basic-performance-analysis)
    - [Memory Access Analysis](#memory-access-analysis)
    - [Symbol Resolution and Flame Graphs](#symbol-resolution-and-flame-graphs)
- [Use Cases and Benefits](#use-cases-and-benefits)
---

## Overview

The `to_perf_file()` method transforms your application's sample data into the standard perf.data format used by Linux perf tools. This enables you to:

* **Leverage existing perf ecosystem tools** for analysis, visualization, and reporting
* **Generate flame graphs** using tools like [FlameGraph](https://github.com/brendangregg/FlameGraph)
* **Perform memory access analysis** with `perf mem report`
* **Annotate source code** with performance data using `perf annotate`
* **Share analysis-ready data** with teams using standard perf workflows

## Exporting Samples to Perf Data Files

### Single Sampler Export
For applications using a single `Sampler` instance, export samples after stopping the sampler:

```cpp
#include <perfcpp/sampler.h>

auto sampler = perf::Sampler{};

/// Configure sampling - example with instruction pointer and timing
sampler.trigger("cycles", perf::Period{ 50000U });
sampler.values()
    .logical_instruction_pointer(true)
    .timestamp(true)
    .cpu_id(true);

/// Sample only the critical section, not the entire application
sampler.start();
execute_critical_workload(); /// <-- Only this code is sampled
sampler.stop();

/// Export to perf.data format
sampler.to_perf_file("perf.data");
```

### Multi-Sampler Export
For parallel sampling scenarios using `MultiThreadSampler`, `MultiCoreSampler`, or other multi-sampler classes:

```cpp
#include <perfcpp/sampler.h>

/// Example: Multi-core sampling
auto sampler = perf::MultiCoreSampler{
    counter_definition,
    {0, 1, 2, 3}, /// CPU cores to sample
    sample_config
};

/// Configure sampling triggers and values
sampler.trigger("cycles", perf::Period{ 50000U });
sampler.values()
    .logical_instruction_pointer(true)
    .timestamp(true)
    .cpu_id(true);

/// Record samples only during parallel computation
sampler.start();
execute_parallel_computation(); /// <-- Only this parallel section is sampled
sampler.stop();

/// Export consolidated samples from all cores
sampler.to_perf_file("perf.data");
```

> [!TIP]
> The exported `perf.data` file contains samples from all samplers in a multi-sampler setup, automatically merged when available.

## Analyzing the Exported Data

### Basic Performance Analysis
Use standard perf tools to analyze the exported data:

```bash
# Basic performance report with symbol resolution
perf report -i perf.data

# Generate a text-based report
perf report -i perf.data --stdio

# Focus on specific functions or modules
perf report -i perf.data --sort comm,dso,symbol
```

### Memory Access Analysis
For memory sampling data (when using memory-capable triggers like `mem-loads` or `ibs_op`):

```bash
# Memory access analysis report
perf mem report -i perf.data

# Detailed memory hierarchy analysis
perf mem report -i perf.data --sort mem,snoop,tlb,locked

# Memory access visualization
perf mem report -i perf.data --stdio
```

> [!NOTE]
> Memory analysis requires that your sampler was configured to record memory-related data such as logical memory addresses, data sources, and latency information.

### Symbol Resolution and Flame Graphs
For flame graphs and perform detailed symbol analysis, the `callchain` needs to be included into samples:

```cpp
sampler.values().callchain(true);
```

Afterwards, generate flame graphs via [FlameGraph](https://github.com/brendangregg/FlameGraph):

```bash
# Ensure symbols are available
perf buildid-list -i perf.data

# Generate flame graph data
perf script -i perf.data | stackcollapse-perf.pl | flamegraph.pl > flame.svg
```

## Use Cases and Benefits

**Selective Performance Profiling**: Unlike `perf record` which captures everything, focus your analysis on specific algorithms, data structure operations, or computational kernels while still leveraging the full perf toolchain.

**Team Collaboration**: Share standardized perf.data files with team members who can analyze them using familiar Linux perf tools, regardless of whether they use *perf-cpp* directly.

**Integration with Existing Pipelines**: Incorporate *perf-cpp* into CI/CD pipelines or automated performance testing, generating `perf.data` files that can be processed by existing performance analysis infrastructure.

**Advanced Visualization**: Access the rich ecosystem of perf-compatible visualization tools, including flame graph generators, timeline analyzers, and custom analysis scripts.

---

For more information on sampling configuration and data collection, refer to the [sampling basics documentation](sampling.md) and the [parallel sampling guide](sampling-parallel.md).