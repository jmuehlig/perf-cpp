# *perf-cpp*: Changelog

## v0.12.1
This update extends event discovery to ARM platforms, improves hardware counter introspection, and enhances the flexibility of metric definitions.

- **Automatic Event Discovery on ARM**: Hardware event types are now automatically detected on ARM architectures when initializing a `perf::CounterDefinition` instance.
- **Hardware Counter Introspection**: The number of available physical performance counters per logical core, along with the number of events each counter can multiplex, is now determined automatically when creating a `perf::EventCounter`.
- **Recursive and Scientific Metrics**: Metric expressions can now reference other metrics recursively. Support for scientific notation (e.g., `1e5`) in formula-based metrics has also been added.

## v0.12.0
This release expands symbolic analysis capabilities, introduces FlameGraph generation, and improves hardware event management through both runtime and compile-time support.

- **Symbol Resolution**: Instruction pointers captured during sampling can now be resolved to function names using `perf::SymbolResolver` (see the [documentation](docs/sampling-symbols-and-flamegraphs.md#translating-instruction-pointers-into-symbols)).
- **FlameGraph Export**: Sampling data can be converted into formats compatible with visualization tools such as [Brendan Gregg's FlameGraph](https://github.com/brendangregg/FlameGraph), [Speedscope](https://www.speedscope.app/), and [flamegraph.com](https://flamegraph.com/) using `perf::analyzer::FlameGraphGenerator` (see the [documentation](docs/sampling-symbols-and-flamegraphs.md#translating-sampler-results-into-flame-graphs)).
- **Built-in Event Definitions**: A set of `x86`-specific hardware events is now bundled in [events/x86](events/x86) and can be loaded at runtime using `perf::CounterDefinition`. This serves as an alternative to the `make perf-list` target.
- **Compile-time Event Injection**: Processor-specific event definitions can now be embedded directly at build time by configuring CMake with `-DGEN_PROCESSOR_EVENTS=1`. These are immediately available via `perf::CounterDefinition` (see the [documentation](docs/counters.md#generating-processor-specific-events-at-compile-time)).
- **Automatic Event Discovery**: Additional event types—including RAPL energy counters and AMD IO MMU events—are now automatically detected during the creation of a `perf::CounterDefinition` instance ([issue #6](https://github.com/jmuehlig/perf-cpp/issues/6)).

## v0.11.1
- Unified the behaviour of the `time` and `timestamp` fields in the sampling API, removing discrepancies between the two.

## v0.11.0
This version rolls out a redesigned sampling API. 
Recorded data are now grouped into dedicated sub-structures (such as `Metadata`, `InstructionExecution`, and `DataAccess`) inside `perf::Sample` (see the [documentation](docs/sampling.md#what-can-be-recorded-and-how-to-access-the-data)).

The previous flat API is still available but deprecated and will be removed in `v0.12`.

- **New Sampling Interface**: Work with clearly separated sample sections, exposing additional **AMD IBS** fields that are not surfaced by the `perf_event_open` records.
- **Explicit Latency Attributes**: Vendor-specific latency signals–*cache-access* on Intel and *cache-miss* on AMD–are now surfaced as distinct fields.
- **Heterogeneous-core Support**: Sampling can target multiple PMU domains (e.g., *cpu_core* and *cpu_atom*) on hybrid Intel processors.


## v0.10.0
* New feature: The *auxiliary event* is added automatically if required by the (Intel-) hardware (see the [documentation](docs/sampling.md#sapphire-rapids-and-beyond)).
* New feature: The *Memory Access Analyzer* allows to describe complex data objects and maps sampled memory addresses in order to report latency and access information (see the [documentation](docs/analyzing-memory-access-patterns)).
* The number of pages for the sampling buffer is now aligned automatically in case the number is not configured properly, i.e., a power of two plus one page for the header.
* New feature: Copy sampled data from the mmap-ed perf buffer into application-level buffer whenever the buffer comes close to full (see the [documentation](docs/sampling.md#sample-buffer)). 

## v0.9.0
* Removed deprecated warnings about the sampling interface (and the *old* sampling interface).
* New feature: Access interim results from counters without stopping the counter using [live counters](docs/recording-live-events.md).
* New feature: Sampling the user stack (see the [documentation](docs/sampling.md#user-stack)).
* New feature: Create custom metrics using expressions, e.g., `"instructions/cycles"` (see the [documentation](docs/metrics.md#using-formulas)).
* New feature: Use [metric](docs/metrics.md) when sampling [counter values](docs/sampling.md#counter-values).
* New feature: Control scheduling of events to *physical* hardware counters (see the [documentation](docs/recording.md#control-scheduling-of-events-to-hardware-counters)).
* New feature: Added time events (e.g., `seconds`, `milliseconds`, etc.) as *virtual* counters (see the [documentation](docs/counters.md#built-in-events)).

## v0.8.3
* Fixed multiple compatibility issues where the code relied on Linux kernel features that might not available on different versions.

## v0.8.2
* Fixed compatibility for older Linux versions that don't provide `PERF_MEM_BLK`, `PERF_MEM_LVLNUM`, and `PERF_MEM_REMOTE`.

## v0.8.1
* Fixed error using decltype instead of typeof (by [@toge](https://github.com/toge))

## v0.8.0
* Restructured the build-system – thanks to [@foolnotion](https://github.com/foolnotion): 
  * Examples are no longer included into default build and must be activated with `-DBUILD_EXAMPLES=1` (see [documentation](docs/build.md#build-examples)). 
  * New feature: Added option to install the library using `-DCMAKE_INSTALL_PREFIX=/path/to/libperf-cpp` (see [documentation](docs/build.md#install-the-library)).
* New feature: Define period or frequency along with trigger events when sampling (see [documentation](docs/sampling.md#period--frequency)).
* New feature: `cgroup` sampling (see [documentation](docs/sampling.md#cgroup)).
* New feature: Sampling for context switches (see [documentation](docs/sampling.md#context-switches)).
* New feature: Sampling for throttle events (see [documentation](docs/sampling.md#throttle-and-unthrottle)).
* New feature: Sampling for raw values (see [documentation](docs/sampling.md#raw-data)).
* New feature: Sampling for transaction aborts (see [documentation](docs/sampling.md#hardware-transaction-abort)).
* New feature: Print results from `perf::EventCounter` as a table using `perf::CounterResult::to_string()`.
* Automatically discover AMD Instruction Based Sampling (IBS) PMUs when running on AMD hardware ([see documentation](docs/sampling.md#amd-instruction-based-sampling)).
* Automatically discover Intel Processor Event Based Sampling (PEBS) memory events when running on Intel hardware ([see documentation](docs/sampling.md#intel-processor-event-based-sampling)).
* Enable Intel PEBS by default (used interrupt-based sampling so far, if not specified otherwise in `perf::SampleConfig::precise_ip()`).
* Support Linux Kernel down to `4.0` – Kernels no longer need to be specified via compiler defines.
* Close sampler automatically (i.e., free all buffers and close counters) when destructing.

## v0.7.1
* Fixed compilation error on ARM machines (`__builtin_cpu_is()` is not supported) – thanks to [@Tratori](https://github.com/Tratori).

## v0.7.0

This release comes with many new features, especially focusing on the interface for sampling and error handling using exceptions. 
Please note that we will maintain backward compatibility for the "old"-styled interface until `v0.8.0`. 
Deprecated interfaces are marked as such using `[[deprecated()]]` annotations and may yield warnings during compilation.

Changelog:

* Samples can now be asked if they contain losses (and if so, how many). Sample records can be lost, e.g., if the buffer is out of capacity or the CPU is too busy.
* Errors when adding performance counters and opening/starting samplers are now communicated via exceptions instead of an error variable.
* Introduced a new interface for specifying the data that should be recorded for triggers through `Sampler::values()`.
* Introduced a new interface for specifying the triggers for sampling through `Sampler::trigger()`.
* Added the option to use multiple triggers for sampling (including example).
* Added the option to use different precisions for each trigger.
* Added the option to `open()` the sampler separately. If the sampler is not opened separately, `start()` will open the sampler.
* Added option to ask samples if they are precise (depends on the precision level for triggers).


## v0.6.0
* Using Counter-Names from `perf::CounterDefinition` (via `std::string_view`) instead of copying strings for more performance.
* Switched from `PERF_MEM_LVL_*` to newer `PERF_MEM_LVLNUM` namespace as `PERF_MEM_LVL_*` is marked as deprecated in `linux/perf_event.h`.
* Added multithread and multicore recording.
* Added multithread and multicore sampling.

## v0.5.0
* Switched to LGPL (instead of AGPL).
* Added more complex `WeightStruct` sampling (via `PERF_SAMPLE_WEIGHT_STRUCT`) to enable sampling for instruction latencies on newer hardware (e.g., Intel's Sapphire Rapids).
* Implemented debug output for counters by setting an `is_debug` flag in the config.
* Added more complex branch sampling.
* Implemented autocorrect of `precise_ip` configuration if the hardware rejects the initial user-set config.
* Implemented auxiliary counter to enable memory sampling on Intel's Sapphire Rapids.

## v0.4.1
* Disabled counter `cgroup-switches` for Linux Kernel `< 5.13` (was first introduced with that version).
* Disabled sampling for Data Page Size and Code Page Size for Linux Kernel `< 5.11` (was first introduced with that version).

## v0.4.0
* Added support for register sampling.
* Added `make perf-list` to automatically extract perf counters from the underlying hardware.
* Added support for sampling data and code page sizes.

## v0.3.0
* Added support for event sampling.
* Added full documentation.

## v0.2.1
* Fixed `std::move` on `perf::CounterDefintion`.

## v0.2.0
* Added metrics (e.g., CPI).
* Added json/csv conversion from results.
* Added examples.
