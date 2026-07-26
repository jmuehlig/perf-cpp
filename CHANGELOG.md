# *perf-cpp*: Changelog

## v1.0.1 (WIP)
- `Sampler::close()` unmaps the sample buffers and discards all recorded samples. Calling `Sampler::result()` or `Sampler::to_perf_file()` afterwards previously returned an empty result (respectively wrote an empty perf file) without any indication that samples were dropped; both now throw `perf::CannotGetResultFromClosedSamplerError`. Read the samples after stopping, but before closing the sampler (see the [sampling documentation](https://jmuehlig.github.io/perf-cpp/sampling/#reading-samples-before-closing)). This also applies to `MultiThreadSampler` and `MultiCoreSampler`. `result()` on a sampler that was never opened still returns an empty result, since nothing was recorded in that case.
- Added `CounterDefinition::is_available(name)`, which checks whether an event, metric, or time event can actually be opened on the current hardware by attempting a real `perf_event_open()` call (`supports()` only checks whether it's defined). For metrics, all required events, including recursively referenced ones, are probed too.
- `cmake` now fails immediately with a clear error if the compiler is older than GCC 11 or Clang 14, instead of failing deep inside compilation.
- `create_error_message_from_code()` returns a `std::string_view` instead of an allocated `std::string`, since the caller only ever appends it into another string (thanks to [@horenmar](https://github.com/horenmar), see [PR #13](https://github.com/jmuehlig/perf-cpp/pull/13)).
- The test binary is now registered as a CTest test, so it can be run via `ctest` (including in parallel with `ctest -j`), with failing and skipped tests reported separately (thanks to [@horenmar](https://github.com/horenmar), see [PR #14](https://github.com/jmuehlig/perf-cpp/pull/14)).
- **Bugfixes**:
  - `EventCounter::add()` threw itself into infinite recursion when metrics referenced each other cyclically. It now throws `CannotEvaluateMetricsBecauseOfCycleError`, naming the offending metric.
  - `SharedFileDescriptor` built from an invalid (negative) file descriptor, e.g. an invalid cgroup descriptor, used to report itself as valid anyway. `SharedFileDescriptor{-1}` no longer takes ownership.
  - `EventCounter::close()` and `Sampler::close()` clean up groups and buffers even when `open()` threw partway through, so a failed open no longer leaks file descriptors.
  - The global `CounterDefinition` instance is now allocated lazily and thread-safely.
  - `CounterDefinition::metric_names()` and `time_event_names()` no longer return duplicate names when the same metric or time event exists in both a child instance and its parent.

## v1.0
- **Removed Deprecated Header Files**: All legacy `.h` forwarding headers (e.g., `perfcpp/sampler.h`, `perfcpp/event_counter.h`) have been removed. Use `.hpp` files instead.
- **Richer Branch Stack Entries**: Each entry in a branch stack sample can now carry additional hardware-reported metadata (see the [sampling documentation](https://jmuehlig.github.io/perf-cpp/sampling/#branch-stack)):
  - **Classification** (Linux 4.15+): the type of branch instruction, i.e., conditional, unconditional, call, return, syscall, and more.
  - **Speculation Result** (Linux 6.1+): whether the branch was on the correct or wrong speculative path.
  - **Privilege Level** (Linux 6.1+): whether the branch executed in user space, kernel space, or at hypervisor level.
- **New IBS Sample Fields** (*AMD IBS PMU* only): Three additional fields decoded from IBS raw data, none of which are accessible via the standard `perf_event_open` interface (see the [sampling documentation](https://jmuehlig.github.io/perf-cpp/sampling/#what-can-be-recorded-and-how-to-access-the-data)):
  - **Op Cache Miss** (`record.instruction_execution().cache()->is_op_miss()`): indicates the fetch missed the op cache (decoded instruction cache), even if the L1 instruction cache hit. Enabled via `sampler.values().instruction_cache(true)`. (*IBS Fetch PMU*)
  - **iTLB Refill Latency** (`record.instruction_execution().latency().itlb_refill()`): cycles to refill the instruction TLB after a miss. Only set when an iTLB miss occurred. Enabled via `sampler.values().instruction_latency(true)`. (*IBS Fetch PMU*)
  - **Is Microcode** (`record.instruction_execution().is_microcode()`): indicates the sampled op was dispatched from the microcode ROM sequencer. Enabled via `sampler.values().instruction_type(true)`. (*IBS Op PMU*)
- **Configurable Sample Timestamp Clock**: `SampleConfig::clock(perf::Clock)` selects the POSIX clock used for timestamps, making them directly comparable to `clock_gettime()` values without offset calibration. Note: `Clock::Realtime` and `Clock::Boottime` are rejected by the kernel for hardware PMU events on x86. Use `Clock::Monotonic` or `Clock::MonotonicRaw` instead. See the [sampling documentation](https://jmuehlig.github.io/perf-cpp/sampling/#sample-timestamp-clock).
- **Removed Deprecated Config Setters**: The `Config::is_pinned(bool)` and `Config::is_debug(bool)` setters (deprecated since `v0.13.0`) have been removed. Use `pinned(bool)` and `debug(bool)` instead.
- **Renamed Sample Fields**: Two recording fields have clearer names. The old names still work but are deprecated and will be removed in `v2.0`:
  - `values().sample_id(true)` replaces `values().id(true)` and now matches the `record.metadata().sample_id()` accessor. The CSV column was renamed from `id` to `sample_id` as well.
  - `values().data_access_misaligned(true)` and `record.data_access().is_misaligned()` replace the `*_misalign_penalty()` variants.
- **CMake Package Support**: Fixed the installation process. Installed builds now ship a CMake package config, so `find_package(perf-cpp)` works without manual setup.
- **Raised Compiler Requirements**: Building now requires GCC 11 and newer or Clang 14 and newer (for `std::from_chars` support).
- **Extended Event Configuration**: Added `config3` and `config4` fields for raw event configuration.
- **Tests on ARM**: x86-only tests (Sampler, live events) are skipped on other architectures, so the test suite now runs on ARM (e.g., Raspberry Pi).
- **Bugfixes**:
  - Live counter reads no longer return 0 when they race a concurrent kernel update (seqlock retry in the mmap buffer).
  - The flamegraph export counted every group of identical call stacks one sample too many.
  - Symbol resolver: ELF virtual addresses were treated as file offsets, which produced wrong symbols. Modules that cannot be opened no longer abort resolution, and modules mapped with shared permissions are now resolved.
  - Replaced `select()` with `poll()` in the sample buffer to avoid a possible stack buffer overflow when monitoring many file descriptors.
  - `EventCounter` counted fixed hardware events incorrectly when forming groups.
  - CPU detection: Intel Comet Lake was misclassified as 12th generation (it is 10th), and Intel Rocket Lake was misclassified as well.
  - Hardware counter detection applied a wrong offset for the counter consumed by the NMI watchdog.
  - The perf file export wrote the logical memory address and sample ID in the wrong order.
  - Sampler: fixed an offset when recording performance counter values with samples, multiple IBS decoding bugs, and a missing vendor check for IBS triggers.
  - Event-file parsing no longer depends on the system locale and avoids undefined behavior in the tokenizer.
  - The JSON export now escapes special characters in counter and metric names.
  - Register values are reported as unsigned 64-bit integers.


## v0.14.1
- **Bugfix**: `UniqueFileDescriptor::reset()` did not close the underlying file descriptor (see [issue #12](https://github.com/jmuehlig/perf-cpp/issues/12) – thanks to [@ilyapopov](https://github.com/ilyapopov)).

## v0.14.0
- **Deprecation Warnings Activated**: All legacy `.h` forwarding headers (e.g., `perfcpp/sampler.h`, `perfcpp/event_counter.h`) now emit a compile-time `#pragma message` warning directing users to the `.hpp` replacements introduced in `v0.13.0`. **The old headers will be removed in `v1.0`.** If you see a deprecation message, replace the include with the `.hpp` variant (e.g., `#include <perfcpp/sampler.hpp>`).
- **CGroup Monitoring**: Added support for monitoring all tasks belonging to a cgroup (container). Configure a `perf::CGroupMonitor` (constructed from a path, a cgroup name, a `SharedFileDescriptor`, or a `UniqueFileDescriptor`) and pass it to `Config::cgroup()`. Requires a specific CPU core (`CpuCore::Any` is rejected) and `CAP_PERFMON` or `perf_event_paranoid <= 0`. See the [recording documentation](https://jmuehlig.github.io/perf-cpp/recording/#monitoring-a-cgroup-container).
- **Sampler Input Validation**: Opening a sampler with an invalid period or frequency now raises an exception immediately. Period must be greater than zero; frequency must be between 1 and the kernel's `perf_event_max_sample_rate` limit.
- **RAPL Energy Measurement Example**: Added an example program demonstrating how to measure energy consumption of a code segment using the built-in `watts-pkg`, `watts-cores`, and `watts-ram` metrics.
- **Bugfixes**:
  - `perf::CpuCore` now accepts `std::uint16_t` as a CPU core id
  - Fixed multiple event codes in the event library across AMD Zen5 and Intel Panther Lake 

## v0.13.1
- **Configurable Sampling Triggers**: Typed triggers now fully support hardware-specific configuration:
  - **Intel**: `perf::MemoryLoads` supports configurable `min_latency` for PEBS load latency filtering. `perf::MemoryStores` and `perf::MemoryLoadsAux` provide type-safe alternatives to string-based `mem-stores` and `mem-loads-aux` triggers. String-based triggers remain available and documented.
  - **AMD**: `perf::IbsOp` and `perf::IbsFetch` now build their counter configuration directly from hardware capabilities, fully supporting `is_uop`, `is_l3_miss_only`, and `is_rand` flags. String-based trigger variants (`ibs_op_uops`, `ibs_op_l3missonly`, `ibs_op_uops_l3missonly`, `ibs_fetch_l3missonly`) still work but are no longer documented. Use typed triggers for full configurability.
- **Fixed-Function PMC Scheduling**: On Intel processors, the built-in events `instructions`, `cycles`, `cpu-cycles`, and `ref-cycles` are now automatically scheduled to dedicated pinned groups. This prevents the kernel scheduler from placing fixed-function PMC events alongside generic events, which would distort multiplexing ratios. Fixed groups do not count against the generic PMC limit.
- **Fixed PMC Detection**: Added `HardwareInfo::physical_fixed_performance_counters_per_logical_core()`, which reads the number of fixed-function performance counters on Intel.
- **Built-in Event**: Added `ref-cycles` (reference cycles at a fixed frequency, unaffected by turbo boost or power-saving states) to the built-in hardware event list.
- **Bugfixes**:
  - Fixed out-of-bounds access when reading live counter values.
  - Fixed wrong parsing order for throttle events in the sample decoder.
  - Fixed sample decoder always reporting a data access source even when none was available (e.g., when sampling stores on Intel hardware).
  - Fixed various decoding bugs and hardened the sample decoder against malformed records.
  - Adding events to an already-opened `EventCounter` now raises an exception instead of silently failing.

## v0.13.0
- **Header Restructuring**: Headers have been reorganized into `counter/`, `sample/`, `metric/`, `analyzer/`, and `util/` subdirectories and renamed from `.h` to `.hpp`. The previous `.h` headers remain as forwarding includes with deprecation notices and will be removed in v1.0.
- **Breaking**: `EventCounter::add()` and `start()` (including `Sampler::start()` and all multi-thread/core variants) now return `void` instead of `bool`. Errors are communicated via exceptions; the return values were unused.
- **Compile Flag for AUX Buffer Support**: Added `PERFCPP_NO_SAMPLE_AUX` compile flag to disable auxiliary buffer sampling on systems with Linux kernels older than 5.5 that lack `PERF_SAMPLE_AUX` support. Thanks to [@rconnorlawson](https://github.com/rconnorlawson).
- **Perf File Export**: Fixed bugs in perf format when materializing samples into file that can be read via `perf [mem] report`.
- **NMI Watchdog Detection**: Hardware counter detection now accounts for the NMI watchdog permanently consuming one hw-PMU counter, fixing incorrect counter counts on systems with the watchdog enabled.
- **RAPL Power Metrics**: Added built-in `watts-pkg`, `watts-cores`, and `watts-ram` metrics for measuring power consumption via RAPL energy counters (see the [documentation](docs/metrics.md#available-built-in-metrics)).
- **Conan Package**: Added Conan 2.x package recipe for easier integration.
- **Config Setter Naming**: Standardized `Config` setter naming — setters no longer use the `is_` prefix (e.g., `pinned(bool)` instead of `is_pinned(bool)`). The old `is_pinned(bool)` and `is_debug(bool)` setters are deprecated and will be removed in v1.0.
- **SampleResult CSV Export**: Added `SampleResult::to_csv()` returning a `std::string`, complementing the existing file-based overload.
- **Per-Element Results**: Added `result_of_thread(thread_id)`, `result_of_process(process_id)`, and `result_of_core(core_id)` to query individual results from `MultiThreadEventCounter`, `MultiProcessEventCounter`, and `MultiCoreEventCounter`. Process and core variants return `std::optional<CounterResult>` since the ID may not be present.
- **Documentation**: Rewrote and restructured all documentation pages for consistency, conciseness, and correctness. Documentation is now hosted at [jmuehlig.github.io/perf-cpp](https://jmuehlig.github.io/perf-cpp/).

## v0.12.6
- **CSV Export**: Samples can now be exported to CSV format using `SampleResult::to_csv()`, enabling custom analysis with statistical tools, spreadsheets, or data processing pipelines (see the [documentation](docs/sampling-export-to-csv.md)).
- **Flamegraphs**: Samples can be exported to flamegraphs via `SampleResult::to_flamegraphs(std::string&&)` (see the [documentation](docs/sampling-symbols-and-flamegraphs.md)).
- **Fine-Grained Sampling Configuration**: The sampling API now provides more granular control over which data fields are recorded. Previously coarse-grained options have been split into specific setters, allowing precise selection of hardware-specific metrics. See the [sampling documentation](docs/sampling.md) for more information. New setters include:
  - **Physical Instruction Pointer** (*AMD IBS Fetch PMU* only)
  - **Instruction Type** (*AMD IBS Op PMU* for `Return` and `Branch` types)
  - **Branch Type** (*AMD IBS Op PMU* only)
  - **Instruction Latency** (*Intel*: instruction retirement cycles; *AMD IBS Op PMU*: uOp tag-to-retirement, completion-to-retirement, tag-to-completion; *AMD IBS Fetch PMU*: fetch latency)
  - **Instruction Cache** (*AMD IBS Fetch PMU* only)
  - **Instruction TLB** (*AMD IBS Fetch PMU* only)
  - **Instruction Fetch** (*AMD IBS Fetch PMU* only)
  - **Data TLB Page Size** (*AMD IBS Op PMU* only)
  - **Data TLB Latency** (*AMD IBS Op PMU* only)
  - **Data Access Width** (*AMD IBS Op PMU* only)
  - **Data Access Misalignment Penalty** (*AMD IBS Op PMU* only)
  - **MHB/MAB Allocations** (*AMD IBS Op PMU* only)
  - **Auxiliary Values**
- Added `perf::Config::include_host(bool)` to enable excluding host events when counting hardware events in virtual machines (see the [documentation](docs/recording.md#further-configuration-settings)).
- Added `perf::Config::is_pinned(bool)` to enable pinning events to the PMU (see the [documentation](docs/recording.md#further-configuration-settings)).


## v0.12.5
- **Bugfix**: The library could not compile for specific Linux kernels (see [#10](https://github.com/jmuehlig/perf-cpp/issues/10)).
- **Symbol Translation**: Improved translation from instruction pointer to symbol.

## v0.12.4
This update enables exporting sampled data to the standard perf format for analysis with existing tools.

- **Bugfix**: The library crashed when events loaded from an external CSV file contained empty spaces (see [#8](https://github.com/jmuehlig/perf-cpp/issues/8)). Thanks to [@Liteom](https://github.com/Liteom).
- **Bugfix**: The library could not compile for specific Linux kernels not providing `PERF_MEM_LVLNUM_UNC` and `PERF_MEM_SNOOPX_PEER` (see [#7](https://github.com/jmuehlig/perf-cpp/issues/7)). Thanks to [@Raphalex46](https://github.com/Raphalex46) for pointing out. 
- **Perf Data Export**: Samples can now be written as *perf data files* using `Sampler::to_perf_file()`, enabling analysis with standard perf ecosystem tools like perf report (see the [documentation](docs/analyzing-samples-with-perf-report.md)). **Note that this feature is experimental.**

## v0.12.3
This update simplifies the handling of counter definitions by introducing a default instance.

- **Default Counter Definitions**: Supplying a user-defined `perf::CounterDefinition` to each `perf::EventCounter` or `perf::Sampler` is no longer required. If none is provided, a default instance is used automatically. Custom definitions now extend the default set of events instead of duplicating them.

## v0.12.2
- **Metric Functions**: Metrics now support built-in functions such as `ratio(A, B)` and `sum(A, B, C, ...)`, enabling more expressive and reusable formulas (see the [documentation](docs/metrics.md#built-in-functions)).
- **Optimized Compile-time Event Injection**: The generated runtime event registration class is now only created if it does not already exist, reducing unnecessary recompilation.
- **Improved Live Event Accuracy**: Live event values now account for partial runtime durations via time scaling, improving accuracy when counters were not active for the full measurement window.

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
- **Compile-time Event Injection**: Processor-specific event definitions can now be embedded directly at build time by configuring CMake with `-DGEN_PROCESSOR_EVENTS=1`. These are immediately available via `perf::CounterDefinition` (see the [documentation](docs/counters.md#auto-generating-events-at-compile-time)).
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
* New feature: Create custom metrics using expressions, e.g., `"instructions/cycles"` (see the [documentation](docs/metrics.md#formula-based-metrics)).
* New feature: Use [metric](docs/metrics.md) when sampling [counter values](docs/sampling.md#counter-values).
* New feature: Control scheduling of events to *physical* hardware counters (see the [documentation](docs/recording.md#control-scheduling-of-events-to-hardware-counters)).
* New feature: Added time events (e.g., `seconds`, `milliseconds`, etc.) as *virtual* counters (see the [documentation](docs/counters.md#working-with-built-in-events)).

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
