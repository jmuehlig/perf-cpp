# Analyzing Samples with Linux Perf Tools

Export collected samples into the `perf.data` format used by the Linux perf tools for analysis with `perf report`, `perf mem report`, and flame graph generators.
The examples below write to a file named `perf-cpp.data` to avoid clashing with files produced by `perf record`.

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

/// Export the samples in perf.data format.
sampler.to_perf_file("perf-cpp.data");

/// Release resources explicitly, or let the destructor handle it.
sampler.close();
```

### Multi-Sampler

For parallel sampling with `MultiThreadSampler` or `MultiCoreSampler`, the same method exports consolidated samples from all instances:

```cpp
auto sample_config = perf::SampleConfig{};

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

> [!IMPORTANT]
> Export **before** closing the sampler: `close()` discards all recorded samples.
> Calling `to_perf_file()` on a closed sampler throws `perf::CannotGetResultFromClosedSamplerError` instead of writing an empty file (see the [sampling documentation](https://jmuehlig.github.io/perf-cpp/sampling/#reading-samples-before-closing)).

---

## Analyzing the Exported Data

### Basic Performance Analysis

```bash
# Performance report with symbol resolution.
perf report -i perf-cpp.data

# Text-based report.
perf report -i perf-cpp.data --stdio

# Sort by command, shared object, and symbol.
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
See the [memory analysis documentation](sampling-memory-analysis.md) for how to configure them.

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

See [symbols and flame graphs](sampling-symbols-and-flamegraphs.md) for symbol resolution and flame graph generation without the perf tools.
