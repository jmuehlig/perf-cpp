# Analyzing Samples with Linux Perf Tools

Export collected samples into the standard `perf-cpp.data` file format for analysis with `perf report`, `perf mem report`, and flame graph generators.

> [!TIP]
> See the example: **[perf_record.cpp](https://github.com/jmuehlig/perf-cpp/tree/dev/examples/sampling/perf_record.cpp)**.

---

## Exporting Samples

### Single Sampler

```cpp
#include <perfcpp/sampler.hpp>

auto sampler = perf::Sampler{};
sampler.trigger("cycles", perf::Period{ 50000U });
sampler.values()
    .logical_instruction_pointer(true)
    .timestamp(true)
    .cpu_id(true);

sampler.start();
/// ... computation here ...
sampler.stop();

/// Export to perf-cpp.data format.
sampler.to_perf_file("perf-cpp.data");

/// Release resources explicitly, or let the destructor handle it.
sampler.close();
```

### Multi-Sampler

For parallel sampling with `MultiThreadSampler` or `MultiCoreSampler`, the same method exports consolidated samples from all instances:

```cpp
auto sampler = perf::MultiCoreSampler{
    {0, 1, 2, 3}, /// CPU cores to sample.
    sample_config
};

sampler.trigger("cycles", perf::Period{ 50000U });
sampler.values()
    .logical_instruction_pointer(true)
    .timestamp(true)
    .cpu_id(true);

sampler.start();
/// ... computation runs on the monitored cores ...
sampler.stop();

sampler.to_perf_file("perf-cpp.data");
sampler.close();
```

---

## Analyzing the Exported Data

### Basic Performance Analysis

```bash
# Performance report with symbol resolution.
perf report -i perf-cpp.data

# Text-based report.
perf report -i perf-cpp.data --stdio

# Focus on specific functions or modules.
perf report -i perf-cpp.data --sort comm,dso,symbol
```

> [!TIP]
> If `perf report` appears to hang, your system may be fetching debug symbols over the network via [debuginfod](https://sourceware.org/elfutils/Debuginfod.html).
> This is especially common with callchain recordings that reference many binaries.
> Disable it by unsetting `DEBUGINFOD_URLS`:
> ```bash
> DEBUGINFOD_URLS='' perf report -i perf-cpp.data
> ```

### Memory Access Analysis

Requires memory-capable triggers (`mem-loads`, `ibs_op`) and memory-related sample fields (addresses, data sources, latency).

```bash
perf mem report -i perf-cpp.data

# Detailed memory hierarchy analysis.
perf mem report -i perf-cpp.data --sort mem,snoop,tlb,locked
```

### Flame Graphs

Include the callchain in your sampler configuration:

```cpp
sampler.values().callchain(true);
```

Then generate flame graphs via [FlameGraph](https://github.com/brendangregg/FlameGraph):

```bash
perf script -i perf-cpp.data | stackcollapse-perf.pl | flamegraph.pl > flame.svg
```
