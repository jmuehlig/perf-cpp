# Event Sampling

Sampling captures detailed information, such as instruction pointers, memory addresses, counter values, branches, and latencies, at a user-defined period or frequency.

> [!NOTE]
> `Sampler` monitors a single thread. For multi-threaded or multi-core sampling, use `MultiThreadSampler` or `MultiCoreSampler` (see [parallel sampling](sampling-parallel.md)).

> [!TIP]
> See the examples: **[instruction_pointer.cpp](https://github.com/jmuehlig/perf-cpp/tree/dev/examples/sampling/instruction_pointer.cpp)**, **[branch.cpp](https://github.com/jmuehlig/perf-cpp/tree/dev/examples/sampling/branch.cpp)**, **[counter.cpp](https://github.com/jmuehlig/perf-cpp/tree/dev/examples/sampling/counter.cpp)**, **[memory_address.cpp](https://github.com/jmuehlig/perf-cpp/tree/dev/examples/sampling/memory_address.cpp)**.

---

## Basic Lifecycle

Configure what to sample, which event triggers sampling, then start/stop around your code:

```cpp
#include <perfcpp/sampler.hpp>

/// Configure the trigger period.
auto sample_config = perf::SampleConfig{};
sample_config.period(50000U);

/// Create the sampler and specify trigger and recorded fields.
auto sampler = perf::Sampler{ sample_config };
sampler.trigger("cycles");
sampler.values().timestamp(true).logical_instruction_pointer(true);

/// Optionally open before start() to exclude setup time from measurements.
sampler.open();

/// Start and stop around the code to sample.
sampler.start();
/// ... computation here ...
sampler.stop();

/// Retrieve samples. Each field is std::optional (absent if not configured).
for (const auto& record : sampler.result())
{
    const auto timestamp = record.metadata().timestamp();
    const auto instruction = record.instruction_execution().logical_instruction_pointer();
    if (timestamp.has_value() && instruction.has_value())
    {
        std::cout
            << "Time = " << timestamp.value()
            << " | IP = 0x" << std::hex << instruction.value() << std::dec << std::endl;
    }
}

/// Release resources explicitly, or let the destructor handle it.
sampler.close();
```

`sampler.result()` returns the samples sorted by timestamp (if timestamps are recorded); pass `sampler.result(false)` to keep the raw buffer order.

### Reading Samples Before Closing
`sampler.close()` unmaps the sample buffers and **discards all recorded samples**.
Read the samples before closing, either via `sampler.result()` or `sampler.to_perf_file()`; calling either one on a closed sampler throws `perf::CannotGetResultFromClosedSamplerError`.

```cpp
sampler.stop();

const auto result = sampler.result();   /// Read the samples ...
sampler.close();                        /// ... then release the buffers.

sampler.result();                       /// Throws CannotGetResultFromClosedSamplerError.
```

The returned `perf::SampleResult` is independent of the sampler and stays valid after closing (it only borrows event names from the `perf::CounterDefinition`, not from the sampler).
Note that this differs from `perf::EventCounter`, whose `result()` remains available after `close()`: an event counter holds a handful of counter values, whereas a sampler holds the entire recorded sample stream, which `close()` releases.

A closed sampler is not dead – opening it again starts a new recording:

```cpp
sampler.start();    /// Re-opens the sampler; samples of the previous run are gone.
```

Example output:

    Time = 124853764466887 | IP = 0x5794c991990c
    Time = 124853764663977 | IP = 0xffffffff8d79d48b
    Time = 124853764861377 | IP = 0x5794c991990c
    Time = 124853765058918 | IP = 0x5794c991990c
    Time = 124853765256328 | IP = 0x5794c991990c

### Exporting to CSV
Sample results can be exported to CSV, either as a string or directly to a file.
Only fields configured via `sampler.values()` will contain data; unconfigured fields appear as empty cells.

```cpp
const auto result = sampler.result();

/// Export to a CSV-formatted string.
const auto csv_string = result.to_csv();

/// Export directly to a file.
result.to_csv("samples.csv");
```

Both overloads accept optional delimiter parameters:

```cpp
/// Custom column delimiter and list delimiter.
const auto csv_string = result.to_csv(/* delimiter = */ ';', /* list_delimiter = */ '|');
result.to_csv("samples.csv", /* delimiter = */ ';', /* list_delimiter = */ '|');
```

See the [full CSV field reference](sampling-export-to-csv.md) for details.

---

## Trigger
A trigger event determines *when* the CPU captures a sample: every time the event counter reaches the configured threshold, the CPU records one sample.

```cpp
sampler.trigger("cycles");
```

Multiple triggers can be specified; a sample is captured when any of them fires:

```cpp
sampler.trigger(std::vector<std::vector<std::string>>{{"cycles"}, {"instructions"}});
```

### Notes for Specific CPUs
Intel CPUs allow almost every event as a trigger.
AMD systems are more restricted: typically only `cycles` and IBS events (`ibs_fetch`, `ibs_op`) are supported.

> [!TIP]
> For memory sampling and vendor-specific configuration, see [Specific Notes for Different CPU Vendors](#specific-notes-for-different-cpu-vendors).

### Typed Triggers
In addition to string-based event names, *perf-cpp* provides typed trigger classes that handle vendor detection, event resolution, and hardware-specific configuration automatically.
Typed triggers are defined in `<perfcpp/sample/trigger.hpp>` (included transitively via `<perfcpp/sampler.hpp>`).

#### Available Typed Triggers

| Trigger Class | Vendor | Description | Parameters |
|---|---|---|---|
| `perf::Cycles` | Any | CPU cycles sampling. | — |
| `perf::MemoryLoads` | Intel | PEBS memory-load sampling (Haswell+). Supports minimum latency filtering. | `min_latency` (cycles, default: 30) |
| `perf::MemoryStores` | Intel | PEBS memory-store sampling (Haswell+). | — |
| `perf::MemoryLoadsAux` | Intel | Auxiliary event for memory-load sampling (Sapphire Rapids+). | — |
| `perf::IbsFetch` | AMD | IBS fetch pipeline sampling. | `is_rand` (default: true), `is_l3_miss_only` (default: false) |
| `perf::IbsOp` | AMD | IBS op (execute) pipeline sampling. | `is_uop` (default: false), `is_l3_miss_only` (default: false) |

#### Usage

Typed triggers can be passed directly to `sampler.trigger()`; they convert implicitly to `Sampler::Trigger`:

```cpp
/// Sample memory loads with a minimum latency of 50 cycles (Intel PEBS).
sampler.trigger(perf::MemoryLoads{/* min_latency */ 50});

/// AMD IBS op sampling, micro-ops only.
sampler.trigger(perf::IbsOp{/* is_uop */ true});

/// Typed triggers support precision and period/frequency, just like string triggers.
sampler.trigger(perf::MemoryLoads{/* min_latency */ 50}, perf::Precision::RequestZeroSkid);
sampler.trigger(perf::IbsOp{}, perf::Period{50000U});
sampler.trigger(perf::Cycles{}, perf::Precision::MustHaveConstantSkid, perf::Frequency{1000U});
```

Multiple typed triggers are specified as a vector of groups, where each inner vector is one group (the auxiliary event is auto-inserted when needed):

```cpp
sampler.trigger(std::vector<std::vector<perf::Sampler::Trigger>>{
    { perf::Sampler::Trigger{ perf::MemoryLoads{/* min_latency */ 50} } },
    { perf::Sampler::Trigger{ perf::MemoryStores{} } }
});
```

> [!TIP]
> Typed triggers are the recommended way to configure memory and IBS sampling. They spare you from remembering event name strings and take care of hardware-specific configuration such as `ldlat` patching and IBS variant selection.

## Precision
Due to deep pipelining, a sample's instruction pointer or memory address may not exactly match the instruction that caused the overflow (see [easyperf.net](https://easyperf.net/blog/2019/04/03/Precise-timing-of-machine-code-with-Linux-perf) and the [perf documentation](https://man7.org/linux/man-pages/man2/perf_event_open.2.html)).
You can request a specific amount of skid per trigger:

```cpp
sampler.trigger("cycles", perf::Precision::AllowArbitrarySkid);
```

Available precision levels:

- `perf::Precision::AllowArbitrarySkid` (does **not** enable Intel PEBS)
- `perf::Precision::MustHaveConstantSkid` (default)
- `perf::Precision::RequestZeroSkid`
- `perf::Precision::MustHaveZeroSkid`

The default precision can also be set via `SampleConfig`:

```cpp
auto sample_config = perf::SampleConfig{};
sample_config.precision(perf::Precision::RequestZeroSkid);

auto sampler = perf::Sampler{ sample_config };
sampler.trigger("cycles");
```

> [!NOTE]
> If the precision is too high for the perf subsystem, *perf-cpp* will automatically reduce it. It will not increase precision autonomously.

## Period / Frequency
Each trigger can specify a period (sample every N events) **or** a frequency (samples per second):

```cpp
/// Every 50,000th cycle.
sampler.trigger("cycles", perf::Period{50000U});

/// 1000 samples per second (hardware adjusts the period automatically).
sampler.trigger("cycles", perf::Frequency{1000U});
```

Period/frequency and precision can be combined:
```cpp
/// Every 50,000th cycle with zero skid.
sampler.trigger("cycles", perf::Precision::RequestZeroSkid, perf::Period{50000U});
```

The default period or frequency can also be set via `SampleConfig`:

```cpp
auto sample_config = perf::SampleConfig{};
sample_config.period(50000U);
/// or (mutually exclusive):
sample_config.frequency(1000U);

auto sampler = perf::Sampler{ sample_config };
sampler.trigger("cycles");
```

### Valid Values and Errors

| Parameter | Valid Range |
|---|---|
| `perf::Period{N}` | `N ≥ 1` |
| `perf::Frequency{F}` | `1 ≤ F ≤ /proc/sys/kernel/perf_event_max_sample_rate` |

- A period of `0` is rejected by *perf-cpp*: a zero period means "never sample", which produces no output and is almost certainly a misconfiguration.
- A frequency of `0` is rejected by the kernel (`EINVAL`); *perf-cpp* catches it before the syscall.
- A frequency above the system maximum is rejected by the kernel (`EINVAL`). The limit is read from `/proc/sys/kernel/perf_event_max_sample_rate` (default: 100,000 Hz) and can be raised by root: `echo 500000 > /proc/sys/kernel/perf_event_max_sample_rate`.
- The period has no upper bound; the kernel accepts any `uint64_t` value, and very large values simply produce rare samples.

`sampler.open()` throws an exception if any of these constraints are violated.

---

## Sample Timestamp Clock

By default, perf timestamps samples using its internal TSC-based clock (`local_clock()`), which is *not* the same as any POSIX clock.
The resulting `PERF_SAMPLE_TIME` values are not directly comparable to `clock_gettime()` values from user space; correlating them requires an offset calibration step.

Setting a clock via `SampleConfig` replaces the default with a POSIX clock. The recorded timestamps are then directly comparable to user-space time sources:

```cpp
#include <perfcpp/sampler.hpp> /// perf::Clock is included transitively

auto sample_config = perf::SampleConfig{};
sample_config.clock(perf::Clock::Monotonic);  /// set a POSIX clock

auto sampler = perf::Sampler{ sample_config };
sampler.values().timestamp(true);
```

Pass `std::nullopt` to revert to the perf default:

```cpp
sample_config.clock(std::nullopt);  /// revert to internal TSC-based clock
```

### Available Clocks

| Clock | POSIX equivalent | Notes |
|---|---|---|
| `perf::Clock::Monotonic` | `CLOCK_MONOTONIC` | Monotonic; does not count time spent suspended. |
| `perf::Clock::MonotonicRaw` | `CLOCK_MONOTONIC_RAW` | Monotonic; not subject to NTP frequency adjustments. |
| `perf::Clock::MonotonicCoarse` | `CLOCK_MONOTONIC_COARSE` | Low-resolution monotonic; cheaper to read. |
| `perf::Clock::Realtime` | `CLOCK_REALTIME` | Wall clock (UTC); can jump on NTP adjustments. |
| `perf::Clock::RealtimeCoarse` | `CLOCK_REALTIME_COARSE` | Low-resolution wall clock; cheaper to read. |
| `perf::Clock::Boottime` | `CLOCK_BOOTTIME` | Monotonic; includes time the system was suspended. |
| `perf::Clock::Tai` | `CLOCK_TAI` | International Atomic Time; no leap-second smearing. |

> [!WARNING]
> **`Clock::Realtime` and `Clock::Boottime` are rejected by the kernel (`EINVAL`) for hardware PMU events on x86** (e.g., `cycles`, `instructions`).
> Use `Clock::Monotonic` or `Clock::MonotonicRaw` with hardware events.
> `Clock::Realtime` is accepted for software events (`PERF_TYPE_SOFTWARE`).

---

## What Can Be Recorded and How to Access the Data?
Configure which fields to record via `sampler.values()`, then access them on each record from `sampler.result()`.

> [!NOTE]
> A `record` in the following refers to one record from the `sampler.result()` list.


### Metadata
Metadata associated with a sample can be accessed via `record.metadata()`.  
All metadata fields are returned as `std::optional`.

| Name           | Description                                                                                                                    | How to record?                       | How to access?                   | Type                                  |
|----------------|--------------------------------------------------------------------------------------------------------------------------------|--------------------------------------|----------------------------------|---------------------------------------|
| **Mode**       | Indicates the execution mode in which the sample was recorded (`Kernel`, `User`, `Hypervisor`, `GuestKernel`, or `GuestUser`). | Always recorded                      | `record.metadata().mode()`       | `std::optional<perf::Metadata::Mode>` |
| **Sample ID**  | Unique identifier for the sample's group leader.                                                                               | `sampler.values().sample_id(true)`   | `record.metadata().sample_id()`  | `std::optional<std::uint64_t>`        |
| **Stream ID**  | Unique identifier for the event that generated the sample.                                                                     | `sampler.values().stream_id(true)`   | `record.metadata().stream_id()`  | `std::optional<std::uint64_t>`        |
| **Timestamp**  | Records the time at which the sample was taken. The clock source can be changed via `SampleConfig::clock()` (see [Sample Timestamp Clock](#sample-timestamp-clock)). | `sampler.values().timestamp(true)`   | `record.metadata().timestamp()`  | `std::optional<std::uint64_t>`        |
| **Period**     | Indicates the event count threshold that triggered the sample.                                                                 | `sampler.values().period(true)`      | `record.metadata().period()`     | `std::optional<std::uint64_t>`        |
| **CPU ID**     | Identifies the CPU core where the sample was recorded.                                                                         | `sampler.values().cpu_id(true)`      | `record.metadata().cpu_id()`     | `std::optional<std::uint32_t>`        |
| **Process ID** | Identifies the process context in which the sample was recorded.                                                               | `sampler.values().thread_id(true)`   | `record.metadata().process_id()` | `std::optional<std::uint32_t>`        |
| **Thread ID**  | Identifies the thread context in which the sample was recorded.                                                                | `sampler.values().thread_id(true)`   | `record.metadata().thread_id()`  | `std::optional<std::uint32_t>`        |

### Instruction Execution

Instruction-level information is accessible via `record.instruction_execution()`.  
All fields are returned as `std::optional`, unless otherwise noted.

| Name                             | Description                                                                                                                         | How to record?                                                            | How to access?                                                  | Type                                                                  |
|----------------------------------|-------------------------------------------------------------------------------------------------------------------------------------|---------------------------------------------------------------------------|-----------------------------------------------------------------|-----------------------------------------------------------------------|
| **Instruction Type**             | The type of the sampled instruction (`Return`, `Branch`, or `DataAccess`) (the first two only on [**AMD's Op PMU**](#ibs-op-pmu)).  | `sampler.values().instruction_type(true)`               | `record.instruction_execution().type()`                         | `std::optional<perf::InstructionExecution::InstructionType>`          |
| **Is Microcode**                 | Indicates that the sampled op was dispatched from the microcode ROM sequencer rather than the regular decoder ([**AMD's Op PMU**](#ibs-op-pmu) only). | `sampler.values().instruction_type(true)` | `record.instruction_execution().is_microcode()` | `std::optional<bool>` |
| **Logical Instruction Pointer**  | The logical address of the sampled instruction.                                                                             | `sampler.values().logical_instruction_pointer(true)`                              | `record.instruction_execution().logical_instruction_pointer()`  | `std::optional<std::uintptr_t>`                                       |
| **Physical Instruction Pointer** | The physical address of the sampled instruction ([**AMD's Fetch PMU**](#ibs-fetch-pmu) only).                                       | `sampler.values().physical_instruction_pointer(true)`                     | `record.instruction_execution().physical_instruction_pointer()` | `std::optional<std::uintptr_t>`                                       |
| **Is Instruction Pointer Exact** | Indicates that the recorded instruction pointer exactly corresponds to the sampled instruction.                                     | `sampler.values().logical_instruction_pointer(true)`                              | `record.instruction_execution().is_instruction_pointer_exact()` | `bool`                                                                |
| **Branch Type**                  | The type of branch, if applicable (`Taken`, `Retired`, `Mispredicted`, `Fuse`) ([**AMD's Op PMU**](#ibs-op-pmu) only) .             | `sampler.values().branch_type(true)`                                      | `record.instruction_execution().branch_type()`                  | `std::optional<perf::InstructionExecution::BranchType>`               |
| **Callchain**                    | The callchain of the sampled instruction.                                                                                           | `sampler.values().callchain(true)` or a `std::uint16_t` for maximum depth | `record.instruction_execution().callchain()`                    | `std::optional<std::vector<std::uintptr_t>>`                          |
| **Code Page Size**               | Indicates the page size of the instruction pointer (from Linux `5.11`).                                                             | `sampler.values().code_page_size(true)`                                   | `record.instruction_execution().page_size()`                    | `std::optional<std::uint64_t>`                                        |
| **Latency**                      | Captures latency information of instruction execution and fetch.                                                                    | [See details below](#instruction-latency)                                 | `record.instruction_execution().latency()`                      | `perf::InstructionExecution::Latency`                                 |
| **Cache**                        | Captures cache-related information from the instruction fetch stage.                                                                | [See details below](#instruction-cache)                                   | `record.instruction_execution().cache()`                        | `std::optional<perf::InstructionExecution::Cache>`                    |
| **TLB**                          | Captures TLB information.                                                                                                           | [See details below](#instruction-tlb)                                     | `record.instruction_execution().tlb()`                          | `std::optional<perf::InstructionExecution::TLB>`                      |
| **Fetch**                        | Captures instruction fetch-specific information.                                                                                    | [See details below](#instruction-fetch)                                   | `record.instruction_execution().fetch()`                        | `std::optional<perf::InstructionExecution::Fetch>`                    |
| **Hardware Transaction Abort**   | Provides information on transactional memory aborts.                                                                                | [See details below](#hardware-transaction-abort)                          | `record.instruction_execution().hardware_transaction_abort()`   | `std::optional<perf::InstructionExecution::HardwareTransactionAbort>` |

**Example:** [`instruction_pointer.cpp`](https://github.com/jmuehlig/perf-cpp/tree/dev/examples/sampling/instruction_pointer.cpp)

#### Instruction Latency
Latency information captures timing characteristics for instruction execution or micro-operations (on AMD).  
All fields are returned as `std::optional`.

| Name                             | Description                                                                                                                     | How to record?                                | How to access?                                                            | Type                           |
|----------------------------------|---------------------------------------------------------------------------------------------------------------------------------|-----------------------------------------------|---------------------------------------------------------------------------|--------------------------------|
| **Instruction Retirement**       | The total latency (in cycles) to execute the instruction, including TLB and memory accesses. (**Intel** only)                   | `sampler.values().instruction_latency(true)`              | `record.instruction_execution().latency().instruction_retirement()`       | `std::optional<std::uint32_t>` |
| **uOp Tag-to-Retirement**        | The number of cycles from tagging a uOp to its retirement ([**AMD's Op PMU**](#ibs-op-pmu) only).                               | `sampler.values().instruction_latency(true)`              | `record.instruction_execution().latency().uop_tag_to_retirement()`        | `std::optional<std::uint32_t>` |
| **uOp Completion-to-Retirement** | The number of cycles from uOp completion to retirement ([**AMD's Op PMU**](#ibs-op-pmu) only).                                  | `sampler.values().instruction_latency(true)`  | `record.instruction_execution().latency().uop_completion_to_retirement()` | `std::optional<std::uint32_t>` |
| **uOp Tag-to-Completion**        | The number of cycles from tagging a uOp to its completion ([**AMD's Op PMU**](#ibs-op-pmu) only).                               | `sampler.values().instruction_latency(true)`  | `record.instruction_execution().latency().uop_tag_to_completion()`        | `std::optional<std::uint32_t>` |
| **Fetch**                        | The instruction fetch latency (in cycles) from initiation to delivery to the core ([**AMD's Fetch PMU**](#ibs-fetch-pmu) only). | `sampler.values().instruction_latency(true)`  | `record.instruction_execution().latency().fetch()`                        | `std::optional<std::uint32_t>` |
| **iTLB Refill**                  | The number of cycles to refill the instruction TLB after an iTLB miss ([**AMD's Fetch PMU**](#ibs-fetch-pmu) only). Only set when an iTLB miss occurred. | `sampler.values().instruction_latency(true)`  | `record.instruction_execution().latency().itlb_refill()`                  | `std::optional<std::uint32_t>` |

#### Instruction Cache
Provides cache-related information about instruction fetches.  
This is available only on [**AMD's Fetch PMU**](#ibs-fetch-pmu).  
Note that `record.instruction_execution().cache()` returns an `std::optional`.

| Name              | Description                                                           | How to record?                             | How to access?                                               | Type   |
|-------------------|-----------------------------------------------------------------------|--------------------------------------------|--------------------------------------------------------------|--------|
| **L1 Cache Miss** | Indicates that the instruction fetch missed the L1 instruction cache. | `sampler.values().instruction_cache(true)` | `record.instruction_execution().cache()->is_l1_miss()` | `bool` |
| **L2 Cache Miss** | Indicates that the instruction fetch missed the L2 cache.             | `sampler.values().instruction_cache(true)` | `record.instruction_execution().cache()->is_l2_miss()` | `bool` |
| **L3 Cache Miss** | Indicates that the instruction fetch missed the L3 cache.             | `sampler.values().instruction_cache(true)` | `record.instruction_execution().cache()->is_l3_miss()` | `bool` |
| **Op Cache Miss** | Indicates that the instruction fetch missed the op cache (decoded instruction cache). An op cache miss can occur even when the L1 instruction cache hits, since the processor must re-decode instructions from raw bytes. | `sampler.values().instruction_cache(true)` | `record.instruction_execution().cache()->is_op_miss()` | `bool` |

#### Instruction TLB
Provides TLB information related to instruction fetch.  
This is available only on [**AMD's Fetch PMU**](#ibs-fetch-pmu).  
Note that `record.instruction_execution().tlb()` returns an `std::optional`.

| Name              | Description                                                                | How to record?                             | How to access?                                         | Type            |
|-------------------|----------------------------------------------------------------------------|--------------------------------------------|--------------------------------------------------------|-----------------|
| **L1 TLB Miss**   | Indicates that the instruction fetch missed the L1 instruction TLB (iTLB). | `sampler.values().instruction_tlb(true)`   | `record.instruction_execution().tlb()->is_l1_miss()`   | `bool`          |
| **L2 TLB Miss**   | Indicates that the instruction fetch missed the second-level TLB (STLB).   | `sampler.values().instruction_tlb(true)`   | `record.instruction_execution().tlb()->is_l2_miss()`   | `bool`          |
| **L1 Page Size**  | The page size used in the L1 instruction TLB.                              | `sampler.values().instruction_tlb(true)`   | `record.instruction_execution().tlb()->l1_page_size()` | `std::uint64_t` |

#### Instruction Fetch
Provides details about instruction fetch behavior during micro-op execution.  
This is available only on [**AMD's Fetch PMU**](#ibs-fetch-pmu).  
Note that `record.instruction_execution().fetch()` returns an `std::optional`.

| Name                  | Description                                                 | How to record?                               | How to access?                                          | Type   |
|-----------------------|-------------------------------------------------------------|----------------------------------------------|---------------------------------------------------------|--------|
| **Is Fetch Complete** | Indicates that the instruction fetch process completed.     | `sampler.values().instruction_fetch(true)`   | `record.instruction_execution().fetch()->is_complete()` | `bool` |
| **Is Fetch Valid**    | Indicates that the instruction fetch is considered valid.   | `sampler.values().instruction_fetch(true)`   | `record.instruction_execution().fetch()->is_valid()`    | `bool` |

#### Hardware Transaction Abort
Provides information about hardware transactional memory aborts.  
This is available only on **Intel**.  
Note that `record.instruction_execution().hardware_transaction_abort()` returns an `std::optional`.

| Name                                  | Description                                                               | How to record?                                      | How to access?                                                                                     | Type            |
|---------------------------------------|---------------------------------------------------------------------------|-----------------------------------------------------|----------------------------------------------------------------------------------------------------|-----------------|
| **Is Elision Transaction**            | Indicates that the abort originated from an elision-type transaction.     | `sampler.values().hardware_transaction_abort(true)` | `record.instruction_execution().hardware_transaction_abort()->is_elision_transaction()`            | `bool`          |
| **Is Generic Transaction**            | Indicates that the abort originated from a generic hardware transaction.  | `sampler.values().hardware_transaction_abort(true)` | `record.instruction_execution().hardware_transaction_abort()->is_generic_transaction()`            | `bool`          |
| **Is Synchronous Transaction**        | Indicates that the abort occurred due to a synchronous condition.         | `sampler.values().hardware_transaction_abort(true)` | `record.instruction_execution().hardware_transaction_abort()->is_synchronous_abort()`              | `bool`          |
| **Is Retryable**                      | Indicates that the transaction can be retried after the abort.            | `sampler.values().hardware_transaction_abort(true)` | `record.instruction_execution().hardware_transaction_abort()->is_retryable()`                      | `bool`          |
| **Is Due to Memory Conflict**         | Indicates that the abort was caused by a memory conflict.                 | `sampler.values().hardware_transaction_abort(true)` | `record.instruction_execution().hardware_transaction_abort()->is_due_to_memory_conflict()`         | `bool`          |
| **Is Due to Write Capacity Conflict** | Indicates that the abort was caused by a write capacity conflict.         | `sampler.values().hardware_transaction_abort(true)` | `record.instruction_execution().hardware_transaction_abort()->is_due_to_write_capacity_conflict()` | `bool`          |
| **Is Due to Read Capacity Conflict**  | Indicates that the abort was caused by a read capacity conflict.          | `sampler.values().hardware_transaction_abort(true)` | `record.instruction_execution().hardware_transaction_abort()->is_due_to_read_capacity_conflict()`  | `bool`          |
| **User Specified Code**               | User-specified code associated with the abort, if provided.               | `sampler.values().hardware_transaction_abort(true)` | `record.instruction_execution().hardware_transaction_abort()->user_specified_code()`               | `std::uint32_t` |

### Data Access
Provides information about memory, cache, and TLB behavior during data access.  
All fields can be accessed via `record.data_access()`.  
Note that most fields are returned as `std::optional`.

> [!IMPORTANT]
> Sampling for memory accesses (memory address, cache information, etc.) is only supported using [**AMD's IBS Op PMU**](#ibs-op-pmu) and [**Intel PEBS `mem-load`/`mem-store` events**](#intel-processor-event-based-sampling).

| Name                        | Description                                                                                         | How to record?                                        | How to access?                                   | Type                                      |
|-----------------------------|-----------------------------------------------------------------------------------------------------|-------------------------------------------------------|--------------------------------------------------|-------------------------------------------|
| **Is load**                 | Indicates that the access was a load operation.                                                     | `sampler.values().data_source(true)`                  | `record.data_access().is_load()`                 | `bool`                                    |
| **Is Store**                | Indicates that the access was a store operation.                                                    | `sampler.values().data_source(true)`                  | `record.data_access().is_store()`                | `bool`                                    |
| **Is Software Prefetch**    | Indicates that the access was a software prefetch ([**AMD's Op PMU**](#ibs-op-pmu) only).           | `sampler.values().instruction_type(true)`             | `record.data_access().is_software_prefetch()`    | `bool`                                    |
| **Is Locked**               | Indicates that the sampled data access was a locked operation.                                      | `sampler.values().data_source(true)`                  | `record.data_access().is_locked()`               | `std::optional<bool>`                                                 |
| **Logical Memory Address**  | The logical address of the accessed memory.                                                         | `sampler.values().logical_memory_address(true)`       | `record.data_access().logical_memory_address()`  | `std::optional<std::uintptr_t>`           |
| **Physical Memory Address** | The physical address of the accessed memory (from Linux `4.13`).                                    | `sampler.values().physical_memory_address(true)`      | `record.data_access().physical_memory_address()` | `std::optional<std::uintptr_t>`           |
| **Source**                  | Provides information about the memory or cache source of the access.                                | [See details below](#data-source)                     | `record.data_access().source()`                  | `std::optional<perf::DataAccess::Source>` |
| **Latency**                 | Provides latency details for the data access.                                                       | [See details below](#data-latency)                    | `record.data_access().latency()`                 | `perf::DataAccess::Latency`               |
| **TLB**                     | Provides TLB-related information for the access.                                                    | [See details below](#data-tlb)                        | `record.data_access().tlb()`                     | `perf::DataAccess::TLB`                   |
| **Snoop**                   | Provides Snoop-related information for the access.                                                  | [See details below](#data-snoop)                      | `record.data_access().snoop()`                   | `std::optional<perf::DataAccess::Snoop>`  |
| **Is Misaligned**           | Indicates that the access incurred a misalignment ([**AMD's Op PMU**](#ibs-op-pmu) only).           | `sampler.values().data_access_misaligned(true)`       | `record.data_access().is_misaligned()`           | `std::optional<bool>`                     |
| **Access Width**            | The size (in bytes) of the accessed data ([**AMD's Op PMU**](#ibs-op-pmu) only).                    | `sampler.values().data_access_width(true)`            | `record.data_access().access_width()`            | `std::optional<std::uint8_t>`             |
| **Data Page Size**          | The page size of the data page (from Linux `5.11`).                                                 | `sampler.values().data_page_size(true)`               | `record.data_access().page_size()`               | `std::optional<std::uint64_t>`            |

**Example:** [`memory_address.cpp`](https://github.com/jmuehlig/perf-cpp/tree/dev/examples/sampling/memory_address.cpp)

#### Data Source
Provides detailed information about the memory or cache source involved in a data access.  
Note that `record.data_access().source()` returns an `std::optional`.

| Name                              | Description                                                                                             | How to record?                             | How to access?                                                 | Type                          |
|-----------------------------------|---------------------------------------------------------------------------------------------------------|--------------------------------------------|----------------------------------------------------------------|-------------------------------|
| **Is L1 Hit**                     | Indicates that the access hit the L1 data cache (L1d).                                                  | `sampler.values().data_source(true)`       | `record.data_access().source()->is_l1_hit()`                   | `bool`                        |
| **Is MHB Hit**                    | Indicates that the access hit the LFB (Intel) or MAB (AMD).                                             | `sampler.values().data_source(true)`       | `record.data_access().source()->is_mhb_hit()`                  | `std::optional<bool>`         |
| **Number of Allocated MHB Slots** | The number of MAB (AMD) slots allocated at the time of sampling ([**AMD's Op PMU**](#ibs-op-pmu) only). | `sampler.values().mhb_allocations(true)`   | `record.data_access().source()->num_mhb_slots_allocated()`     | `std::optional<std::uint8_t>` |
| **Is L2 Hit**                     | Indicates that the access hit the L2 cache.                                                             | `sampler.values().data_source(true)`       | `record.data_access().source()->is_l2_hit()`                   | `bool`                        |
| **Is L3 Hit**                     | Indicates that the access hit the L3 cache.                                                             | `sampler.values().data_source(true)`       | `record.data_access().source()->is_l3_hit()`                   | `bool`                        |
| **Is L4 Hit**                     | Indicates that the access hit the L4 cache.                                                             | `sampler.values().data_source(true)`       | `record.data_access().source()->is_l4_hit()`                   | `bool`                        |
| **Is Memory Hit**                 | Indicates that the access missed all caches and was served from memory.                                 | `sampler.values().data_source(true)`       | `record.data_access().source()->is_memory_hit()`               | `bool`                        |
| **Is Remote**                     | Indicates that the access was served by a remote core or node (cache or memory).                        | `sampler.values().data_source(true)`       | `record.data_access().source()->is_remote()`                   | `bool`                        |
| **Is Same Node Remote Core**      | Indicates that the access was served by another core on the same node.                                  | `sampler.values().data_source(true)`       | `record.data_access().source()->is_same_node_remote_core()`    | `std::optional<bool>`         |
| **Is Same Socket Remote Node**    | Indicates that the access was served by another node on the same socket.                                | `sampler.values().data_source(true)`       | `record.data_access().source()->is_same_socket_remote_node()`  | `std::optional<bool>`         |
| **Is Same Board Remote Socket**   | Indicates that the access was served by another socket on the same board.                               | `sampler.values().data_source(true)`       | `record.data_access().source()->is_same_board_remote_socket()` | `std::optional<bool>`         |
| **Is Remote Board**               | Indicates that the access was served by another board.                                                  | `sampler.values().data_source(true)`       | `record.data_access().source()->is_remote_board()`             | `std::optional<bool>`         |
| **Is Uncachable Memory**          | Indicates that the access targeted uncachable memory.                                                   | `sampler.values().data_source(true)`       | `record.data_access().source()->is_uncachable_memory()`        | `std::optional<bool>`         |
| **Is Write Combine Memory**       | Indicates that the access targeted write-combine memory.                                                | `sampler.values().data_source(true)`       | `record.data_access().source()->is_write_combine()`            | `std::optional<bool>`         |

#### Data Latency
Provides latency measurements associated with data access operations.  
All fields are returned as `std::optional`.

| Name             | Description                                                                                                                          | How to record?                               | How to access?                                  | Type                           |
|------------------|--------------------------------------------------------------------------------------------------------------------------------------|----------------------------------------------|-------------------------------------------------|--------------------------------|
| **Cache Access** | The latency (in cycles) for completing the data access ([**Intel** `mem-load`](#intel-processor-event-based-sampling) trigger only). | `sampler.values().data_access_latency(true)` | `record.data_access().latency().cache_access()` | `std::optional<std::uint32_t>` |
| **Cache Miss**   | The latency (in cycles) caused by an L1d cache miss ([**AMD's Op PMU**](#ibs-op-pmu) only).                                          | `sampler.values().data_access_latency(true)`             | `record.data_access().latency().cache_miss()`   | `std::optional<std::uint32_t>` |
| **dTLB Refill**  | The latency (in cycles) for refilling the data TLB after a miss ([**AMD's Op PMU**](#ibs-op-pmu) only).                              | `sampler.values().data_tlb_latency(true)`    | `record.data_access().latency().dtlb_refill()`  | `std::optional<std::uint32_t>` |

#### Data TLB
Provides information about dTLB and STLB access behavior.  
All fields are returned as `std::optional`.

| Name             | Description                                                                                           | How to record?                                | How to access?                              | Type                           |
|------------------|-------------------------------------------------------------------------------------------------------|-----------------------------------------------|---------------------------------------------|--------------------------------|
| **Is L1 Hit**    | Indicates that the data access hit the L1 data TLB (dTLB).                                            | `sampler.values().data_source(true)`          | `record.data_access().tlb().is_l1_hit()`    | `std::optional<bool>`          |
| **Is L2 Hit**    | Indicates that the data access hit the second-level TLB (STLB).                                       | `sampler.values().data_source(true)`          | `record.data_access().tlb().is_l2_hit()`    | `std::optional<bool>`          |
| **L1 Page Size** | The page size of the translation associated with the dTLB hit ([**AMD's Op PMU**](#ibs-op-pmu) only). | `sampler.values().data_tlb_page_size(true)`   | `record.data_access().tlb().l1_page_size()` | `std::optional<std::uint64_t>` |
| **L2 Page Size** | The page size of the translation associated with the STLB hit ([**AMD's Op PMU**](#ibs-op-pmu) only). | `sampler.values().data_tlb_page_size(true)`   | `record.data_access().tlb().l2_page_size()` | `std::optional<std::uint64_t>` |

> [!IMPORTANT]  
> **Intel** systems do not distinguish between L1 and L2 TLB hits.  
> If a TLB hit occurs, both `is_l1_hit()` and `is_l2_hit()` will return `true`.

#### Data Snoop
Provides information about snooping access behavior.  
All fields are returned as `std::optional`.

| Name                      | Description                                                                 | How to record?                       | How to access?                                          | Type                  |
|---------------------------|-----------------------------------------------------------------------------|--------------------------------------|---------------------------------------------------------|-----------------------|
| **Is Hit**                | Indicates that the data access is a snoop hit (`true`) or a miss (`false`). | `sampler.values().data_source(true)` | `record.data_access().snoop()->is_hit()`                | `std::optional<bool>` |
| **Is Hit Modified**       | `True` if the hit cache line is dirty.                                      | `sampler.values().data_source(true)` | `record.data_access().snoop()->is_hit_modified()`       | `std::optional<bool>` |
| **Is Forward**            | Indicates that the cache line is forwarded.                                 | `sampler.values().data_source(true)` | `record.data_access().snoop()->is_forward()`            | `std::optional<bool>` |
| **Is Transfer from Peer** | Indicates that the cache line is transferred from another node.             | `sampler.values().data_source(true)` | `record.data_access().snoop()->is_transfer_from_peer()` | `std::optional<bool>` |

### Counter Values
Records hardware performance event values (e.g., `cycles` or `L1-dcache-loads`) and derived metrics at the time each sample is taken.  
Refer to the documentation on [recording events](recording.md) and [metrics](metrics.md) for more information.

| Name               | Description                                              | How to record?                                                                                           | How to access?     | Type                                                                            |
|--------------------|----------------------------------------------------------|----------------------------------------------------------------------------------------------------------|--------------------|---------------------------------------------------------------------------------|
| **Counter Values** | Captures the values of the specified performance events. | `sampler.values().counter({"cycles", "instructions", "cycles-per-instruction"})` (example counter names) | `record.counter()` | `perf::CounterResult` (see the [recording events](recording.md) documentation). |

**Example:** [`counter.cpp`](https://github.com/jmuehlig/perf-cpp/tree/dev/examples/sampling/counter.cpp)

### Branch Stack
Captures the branch stack recorded by the CPU at the time of sampling.  
This can include call and jump instructions, conditional branches, and transactional memory branches.  
Note that `record.branch_stack()` returns an `std::optional`.

| Name             | Description                                                | How to record?                                                                                             | How to access?          | Type                                       |
|------------------|------------------------------------------------------------|------------------------------------------------------------------------------------------------------------|-------------------------|--------------------------------------------|
| **Branch Stack** | Records the current branch stack of the CPU.               | `sampler.values().branch_stack({perf::BranchType::Call, perf::BranchType::Conditional}, /*is_record_branch_classification=*/false, /*is_record_branch_privilege_level=*/false)` (see types below) | `record.branch_stack()` | `std::optional<std::vector<perf::Branch>>` |

#### Branch Types to Record
You can configure which types of branches to record. The following types are supported (and can be combined):

- `perf::BranchType::Any`
- `perf::BranchType::User`
- `perf::BranchType::Kernel`
- `perf::BranchType::HyperVisor`
- `perf::BranchType::Call` (available from Linux `4.4.0`)
- `perf::BranchType::DirectCall` (available from Linux `4.4.0`)
- `perf::BranchType::IndirectCall`
- `perf::BranchType::Return`
- `perf::BranchType::IndirectJump` (available from Linux `4.2.0`)
- `perf::BranchType::Conditional`
- `perf::BranchType::TransactionalMemoryAbort`
- `perf::BranchType::InTransaction`
- `perf::BranchType::NotInTransaction`

#### Branch
Each entry in the branch stack contains the following information:

| Name                         | Description                                                                                                                        | How to access?                                            | Type                                             |
|------------------------------|------------------------------------------------------------------------------------------------------------------------------------|-----------------------------------------------------------|--------------------------------------------------|
| **Instruction Pointer From** | The instruction pointer where the branch originated.                                                                               | `record.branch_stack()->at(i).instruction_pointer_from()` | `std::uintptr_t`                                 |
| **Instruction Pointer To**   | The instruction pointer where the branch target landed.                                                                            | `record.branch_stack()->at(i).instruction_pointer_to()`   | `std::uintptr_t`                                 |
| **Is Mispredicted**          | Indicates that the branch was mispredicted.                                                                                        | `record.branch_stack()->at(i).is_mispredicted()`          | `bool`                                           |
| **Is Predicted**             | Indicates that the branch was predicted correctly.                                                                                 | `record.branch_stack()->at(i).is_predicted()`             | `bool`                                           |
| **Is In Transaction**        | Indicates that the branch occurred during a hardware transaction.                                                                  | `record.branch_stack()->at(i).is_in_transaction()`        | `bool`                                           |
| **Is Transaction Abort**     | Indicates that the branch aborted a hardware transaction.                                                                          | `record.branch_stack()->at(i).is_transaction_abort()`     | `bool`                                           |
| **Cycles**                   | The number of cycles for the branch (from Linux `4.3`).                                                                            | `record.branch_stack()->at(i).cycles()`                   | `std::optional<std::uint16_t>`                   |
| **Classification**           | The hardware classification of the branch instruction (from Linux `4.15`).                                                      | `record.branch_stack()->at(i).classification()`           | `std::optional<perf::Branch::Classification>`    |
| **Speculation Result**       | The speculation outcome of the branch (from Linux `6.1`).                                                                       | `record.branch_stack()->at(i).speculation_result()`       | `std::optional<perf::Branch::Speculation>`       |
| **Privilege Level**          | The privilege level at which the branch was executed (from Linux `6.1`).                                                        | `record.branch_stack()->at(i).privilege_level()`          | `std::optional<perf::Branch::PrivilegeLevel>`    |

##### Branch Classification Values
`perf::Branch::Classification` describes what kind of branch instruction the hardware recorded:

| Value                                    | Description                                      |
|------------------------------------------|--------------------------------------------------|
| `perf::Branch::Classification::Unknown`          | Classification not available or not recognized.  |
| `perf::Branch::Classification::Conditional`      | Conditional branch (e.g. `je`, `jne`).           |
| `perf::Branch::Classification::Unconditional`    | Unconditional direct branch (e.g. `jmp`).        |
| `perf::Branch::Classification::Indirect`         | Indirect branch (e.g. `jmp [rax]`).              |
| `perf::Branch::Classification::Call`             | Direct function call (e.g. `call foo`).          |
| `perf::Branch::Classification::IndirectCall`     | Indirect function call (e.g. `call [rax]`).      |
| `perf::Branch::Classification::Return`           | Function return (e.g. `ret`).                    |
| `perf::Branch::Classification::Syscall`          | System call entry.                               |
| `perf::Branch::Classification::SyscallReturn`    | System call return.                              |
| `perf::Branch::Classification::ConditionalCall`  | Conditional function call.                       |
| `perf::Branch::Classification::ConditionalReturn`| Conditional function return.                     |
| `perf::Branch::Classification::ExceptionReturn`  | Exception return (e.g. ARM `eret`).              |
| `perf::Branch::Classification::Interrupt`        | Interrupt branch.                                |
| `perf::Branch::Classification::SystemError`      | System error branch.                             |
| `perf::Branch::Classification::NotInTransaction` | Branch not inside a hardware transaction.        |

##### Branch Speculation Values
`perf::Branch::Speculation` describes whether the branch was executed speculatively and whether it was on the correct path:

| Value                                              | Description                                                        |
|----------------------------------------------------|--------------------------------------------------------------------|
| `perf::Branch::Speculation::Wrong`                 | Branch was executed speculatively on the wrong path.               |
| `perf::Branch::Speculation::Correct`               | Branch was not speculative and retired on the correct path.        |
| `perf::Branch::Speculation::SpeculativeCorrect`    | Branch was executed speculatively and was on the correct path.     |

> [!NOTE]
> `std::nullopt` is returned when the hardware did not record a speculation outcome (`PERF_BR_SPEC_NA`).

##### Branch Privilege Level Values
`perf::Branch::PrivilegeLevel` describes the privilege level at which the branch was executed.
Enable recording via the third parameter of `branch_stack({...}, false, /*is_record_branch_privilege_level=*/true)`.

| Value                                       | Description                                      |
|---------------------------------------------|--------------------------------------------------|
| `perf::Branch::PrivilegeLevel::User`        | Branch executed in user space.                   |
| `perf::Branch::PrivilegeLevel::Kernel`      | Branch executed in kernel space.                 |
| `perf::Branch::PrivilegeLevel::Hypervisor`  | Branch executed at hypervisor level.             |

> [!NOTE]
> `std::nullopt` is returned when the hardware did not record a privilege level (`PERF_BR_PRIV_UNKNOWN`) or `PERF_SAMPLE_BRANCH_PRIV_SAVE` was not requested.

**Example:**

```cpp
/// Pass true as the second argument to enable per-entry branch classification (Linux 4.15+).
sampler.values().branch_stack({perf::BranchType::Any}, /*is_record_branch_classification=*/true);

for (const auto& record : sampler.result())
{
    if (record.branch_stack().has_value())
    {
        for (const auto& branch : record.branch_stack().value())
        {
            if (branch.classification() == perf::Branch::Classification::Call)
            {
                std::cout << "Call from 0x" << std::hex << branch.instruction_pointer_from()
                          << " to 0x" << branch.instruction_pointer_to() << "\n";
            }
        }
    }
}
```

**Example:** [`branch.cpp`](https://github.com/jmuehlig/perf-cpp/tree/dev/examples/sampling/branch.cpp)

### User Stack
Captures a snapshot of the user-level stack at the time of sampling.  
Note that `record.user_stack()` returns an `std::optional`.

| Name           | Description                                                   | How to record?                                                                     | How to access?        | Type                                    |
|----------------|---------------------------------------------------------------|------------------------------------------------------------------------------------|-----------------------|-----------------------------------------|
| **User Stack** | Records a specified number of bytes from the user stack.      | `sampler.values().user_stack(64U)` (`64U` specifies the number of bytes to record) | `record.user_stack()` | `std::optional<std::vector<std::byte>>` |

### Registers
Captures register values at the time of sampling, based on the system's ABI.  
Both user-space and kernel-space registers can be recorded.  
Note that `record.user_registers()` and `record.kernel_registers()` return an `std::optional`.

| Name                 | Description                                                    | How to record?                                                                                                          | How to access?                        | Type                   |
|----------------------|----------------------------------------------------------------|-------------------------------------------------------------------------------------------------------------------------|---------------------------------------|------------------------|
| **User Registers**   | User-level register values at the time the sample was taken.   | `sampler.values().user_registers({perf::Registers::x86::AX, perf::Registers::x86::R10})` (example registers shown)      | [See details below](#register-values) | `perf::RegisterValues` |
| **Kernel Registers** | Kernel-level register values at the time the sample was taken. | `sampler.values().kernel_registers({perf::Registers::x86::AX, perf::Registers::x86::R10})` (example registers shown)    | [See details below](#register-values) | `perf::RegisterValues` |

#### Register Values
Register values (user or kernel) can be accessed via `record.user_registers()` or `record.kernel_registers()`.  
The following fields are available:

| Name               | Description                                      | How to access?                                                              | Type                          |
|--------------------|--------------------------------------------------|-----------------------------------------------------------------------------|-------------------------------|
| **Register Value** | The value of a specific register.                | `record.user_registers()->get(perf::Registers::x86::AX)` (example register) | `std::optional<std::uint64_t>` |
| **ABI**            | The ABI used when capturing the register values. | `record.user_registers()->abi()`                                            | `perf::ABI`                   |

**Example:** [`register.cpp`](https://github.com/jmuehlig/perf-cpp/tree/dev/examples/sampling/register.cpp)

### Raw Data
Captures the raw data output from the underlying Performance Monitoring Unit.  
This can be used to manually parse additional information not exposed through the standard *perf-cpp* interface.  
For example, *perf-cpp* uses raw data to expose [AMD IBS](#amd-instruction-based-sampling) records that are otherwise inaccessible via the [*perf_event_open*](https://man7.org/linux/man-pages/man2/perf_event_open.2.html) system call.

| Name         | Description                               | How to record?               | How to access? | Type                                    |
|--------------|-------------------------------------------|------------------------------|----------------|-----------------------------------------|
| **Raw Data** | The raw PMU output for manual inspection. | `sampler.values().raw(true)` | `record.raw()` | `std::optional<std::vector<std::byte>>` |

### Context Switches
Captures context switch events, including switch-in, switch-out, and preemption information.  
This feature requires a Linux kernel version of `4.3` or higher.  
Note that `record.context_switch()` returns an `std::optional`.

| Name              | Description                                                         | How to record?                          | How to access?                          | Type                           |
|-------------------|---------------------------------------------------------------------|-----------------------------------------|-----------------------------------------|--------------------------------|
| **Is Switch In**  | Indicates that the process was switched in.                         | `sampler.values().context_switch(true)` | `record.context_switch()->is_in()`      | `bool`                         |
| **Is Switch Out** | Indicates that the process was switched out.                        | `sampler.values().context_switch(true)` | `record.context_switch()->is_out()`     | `bool`                         |
| **Is Preempt**    | Indicates that the process was preempted.                           | `sampler.values().context_switch(true)` | `record.context_switch()->is_preempt()` | `bool`                         |
| **Thread ID**     | The thread ID involved in the switch (available in CPU-wide mode).  | `sampler.values().context_switch(true)` | `record.context_switch()->thread_id()`  | `std::optional<std::uint32_t>` |
| **Process ID**    | The process ID involved in the switch (available in CPU-wide mode). | `sampler.values().context_switch(true)` | `record.context_switch()->process_id()` | `std::optional<std::uint32_t>` |

If requested, the following [metadata fields](#metadata) will also be included:

- Timestamp
- Stream ID
- CPU ID
- Sample ID

**Example:** [`context_switch.cpp`](https://github.com/jmuehlig/perf-cpp/tree/dev/examples/sampling/context_switch.cpp)

### CGroup
Captures information about control groups (cgroups) associated with each sample.  
Sampling cgroups requires a Linux kernel version of `5.7` or higher.  
Note that `record.cgroup()` returns an `std::optional`.

| Name                | Description                                 | How to record?                  | How to access?            | Type                           |
|---------------------|---------------------------------------------|---------------------------------|---------------------------|--------------------------------|
| **CGroup ID**       | The ID of the cgroup the sample belongs to. | `sampler.values().cgroup(true)` | `record.cgroup_id()`      | `std::optional<std::uint64_t>` |
| **New CGroup ID**   | The ID of a newly added cgroup.             | `sampler.values().cgroup(true)` | `record.cgroup()->id()`   | `std::uint64_t`                |
| **New CGroup Path** | The path of a newly added cgroup.           | `sampler.values().cgroup(true)` | `record.cgroup()->path()` | `std::string`                  |

If requested, the following [metadata fields](#metadata) will also be included:

- Timestamp
- Process ID
- Thread ID
- Stream ID
- CPU ID
- Sample ID

### Throttle and Unthrottle
Captures events where sampling was throttled or unthrottled by the kernel.  
Note that `record.throttle()` returns an `std::optional`.

| Name              | Description                                                   | How to record?                    | How to access?                       | Type   |
|-------------------|---------------------------------------------------------------|-----------------------------------|--------------------------------------|--------|
| **Is Throttle**   | Indicates that the sample corresponds to a throttle event.    | `sampler.values().throttle(true)` | `record.throttle()->is_throttle()`   | `bool` |
| **Is Unthrottle** | Indicates that the sample corresponds to an unthrottle event. | `sampler.values().throttle(true)` | `record.throttle()->is_unthrottle()` | `bool` |

If requested, the following [metadata fields](#metadata) will also be included:

- Timestamp
- Process ID
- Thread ID
- Stream ID
- CPU ID
- Sample ID

### Lost Samples
Samples can get lost when buffers overflow or the CPU is under high load.  
Loss records report how many samples were dropped during profiling.  
Note that `record.count_loss()` returns an `std::optional`.

| Name                  | Description                 | How to record?  | How to access?        | Type                           |
|-----------------------|-----------------------------|-----------------|-----------------------|--------------------------------|
| **Count Loss Events** | The number of lost samples. | Always recorded | `record.count_loss()` | `std::optional<std::uint64_t>` |

If requested, the following [metadata fields](#metadata) will also be included:

- Timestamp
- Process ID
- Thread ID
- Stream ID
- CPU ID
- Sample ID

---

## Specific Notes for Different CPU Vendors

### Intel (Processor Event Based Sampling)

Memory address, latency, and data source sampling requires specific trigger events.
Intel's `perf list` reports these as "*Supports address when precise*".

*perf-cpp* discovers `mem-loads` and `mem-stores` events automatically on supported Intel hardware.
Memory sampling requires a [precision](#precision) of at least `perf::Precision::RequestZeroSkid`.

#### Before Sapphire Rapids

On Cascade Lake and earlier architectures, latency and source are only reported for memory loads, not stores. This changes starting with Sapphire Rapids.

Using [typed triggers](#typed-triggers) (recommended):
```cpp
/// Loads only; filters for accesses with at least 50 cycles of latency.
sampler.trigger(perf::MemoryLoads{/* min_latency */ 50}, perf::Precision::MustHaveZeroSkid);

/// Stores only.
sampler.trigger(perf::MemoryStores{}, perf::Precision::MustHaveZeroSkid);

/// Loads and stores together.
sampler.trigger(std::vector<std::vector<perf::Sampler::Trigger>>{
    { perf::Sampler::Trigger{ perf::MemoryLoads{/* min_latency */ 50} } },
    { perf::Sampler::Trigger{ perf::MemoryStores{} } }
});
```

Using string-based triggers:
```cpp
/// Loads only.
sampler.trigger("mem-loads", perf::Precision::MustHaveZeroSkid);

/// Stores only.
sampler.trigger("mem-stores", perf::Precision::MustHaveZeroSkid);

/// Loads and stores together.
sampler.trigger(std::vector<std::vector<perf::Sampler::Trigger>>{
    {
      perf::Sampler::Trigger{ "mem-loads", perf::Precision::RequestZeroSkid }
    },
    { perf::Sampler::Trigger{ "mem-stores", perf::Precision::MustHaveZeroSkid } }
  });
```

> [!TIP]
> See the examples: **[memory_address.cpp](https://github.com/jmuehlig/perf-cpp/tree/dev/examples/sampling/memory_address.cpp)**, **[multi_event.cpp](https://github.com/jmuehlig/perf-cpp/tree/dev/examples/sampling/multi_event.cpp)**.

#### Sapphire Rapids and Beyond

Memory latency sampling on Sapphire Rapids requires an **auxiliary counter** in the trigger group before the first real counter ([kernel patch](https://lore.kernel.org/lkml/1612296553-21962-3-git-send-email-kan.liang@linux.intel.com/)).

> [!IMPORTANT]
> *perf-cpp* detects this automatically and adds the auxiliary counter when needed, both for typed `perf::MemoryLoads` triggers and string-based `"mem-loads"` triggers.
> If auto-detection fails, add it manually:

Using [typed triggers](#typed-triggers) (recommended):
```cpp
sampler.trigger(std::vector<std::vector<perf::Sampler::Trigger>>{
    {
        perf::Sampler::Trigger{ perf::MemoryLoadsAux{}, perf::Precision::MustHaveZeroSkid },
        perf::Sampler::Trigger{ perf::MemoryLoads{/* min_latency */ 50}, perf::Precision::RequestZeroSkid }
    },
    { perf::Sampler::Trigger{ perf::MemoryStores{}, perf::Precision::MustHaveZeroSkid } }
});
```

Using string-based triggers:
```cpp
sampler.trigger({
    {
        perf::Sampler::Trigger{"mem-loads-aux", perf::Precision::MustHaveZeroSkid},
        perf::Sampler::Trigger{"mem-loads", perf::Precision::RequestZeroSkid}
    },
    { perf::Sampler::Trigger{"mem-stores", perf::Precision::MustHaveZeroSkid} }
  });
```

> [!TIP]
> Check whether the auxiliary counter is required: `ls /sys/bus/event_source/devices/cpu/events/mem-loads-aux`

### AMD (Instruction Based Sampling)

AMD uses Instruction Based Sampling (IBS) to randomly tag instructions and collect detailed execution data per sample.
IBS provides two PMUs, only one of which can be active at a time.

For details, see the [AMD programmer reference](https://www.amd.com/content/dam/amd/en/documents/processor-tech-docs/programmer-references/24593.pdf) and the [perf IBS documentation](https://man7.org/linux/man-pages/man1/perf-amd-ibs.1.html).

#### IBS Op PMU

The Op PMU captures micro-op execution details: data cache and TLB hit/miss, latency, load/store data source, and branch behavior.
Unlike Intel's mechanism, IBS does not tag specific load or store instructions. If the sampled instruction happens to be a load/store, the sample includes data source, latency, and memory address ([kernel patch](https://lore.kernel.org/all/20220616113638.900-2-ravi.bangoria@amd.com/T/)).

*perf-cpp* detects IBS support automatically and registers the base events `ibs_op` and `ibs_fetch`.
Use [typed triggers](#typed-triggers) to configure IBS behavior:

- **`is_uop`** controls the counting source for the sampling interval.
  With `is_uop = false` (default), the hardware counts CPU cycles and triggers a sample after the configured period of cycles.
  With `is_uop = true`, the hardware counts dispatched micro-operations instead; a sample is triggered after the configured number of micro-ops, which samples instructions of varying latency more uniformly.
- **`is_l3_miss_only`** restricts sampling to operations that miss the L3 cache, filtering out samples that hit in L1/L2/L3.

| Typed Trigger | Selection | Period/Frequency Unit |
|---|---|---|
| `perf::IbsOp{}` | Instructions in the execution pipeline | CPU cycles |
| `perf::IbsOp{/* is_uop */ true}` | Instructions in the execution pipeline | Micro-operations |
| `perf::IbsOp{/* is_uop */ false, /* is_l3_miss_only */ true}` | Instructions that miss L3 | CPU cycles |
| `perf::IbsOp{/* is_uop */ true, /* is_l3_miss_only */ true}` | Instructions that miss L3 | Micro-operations |

> [!NOTE]
> String-based triggers (`ibs_op`, `ibs_fetch`) still work but do not support configuration flags.
> Use typed triggers (`perf::IbsOp`, `perf::IbsFetch`) for full control over uop counting, L3 miss filtering, and randomization.

#### IBS Fetch PMU

The Fetch PMU captures instruction fetch details: instruction cache and TLB hit/miss, fetch latency, and page size.

- **`is_rand`** enables randomized fetch-count tagging (`IbsFetchCtl.IbsFetchRandEn`).
  With `is_rand = true` (default), the hardware adds a random offset to the fetch counter, preventing sampling bias from repetitive instruction patterns.
  With `is_rand = false`, sampling occurs at exact interval boundaries.
- **`is_l3_miss_only`** restricts sampling to fetches that miss the L3 cache.

| Typed Trigger | Selection | Period/Frequency Unit |
|---|---|---|
| `perf::IbsFetch{}` | Instructions in the fetch stage (frontend) | CPU cycles |
| `perf::IbsFetch{/* is_rand */ true, /* is_l3_miss_only */ true}` | Instructions in the fetch stage that miss L3 | CPU cycles |


---

## Sample Buffer
Samples are transferred into an mmap-ed [ring buffer](https://docs.kernel.org/userspace-api/perf_ring_buffer.html).
The buffer size (default: 16 MB) can be configured via `SampleConfig`:

```cpp
auto sample_config = perf::SampleConfig{};
sample_config.buffer_pages(4096U); /// 16 MB (4096 pages × 4 kB per page).

auto sampler = perf::Sampler{ sample_config };
```

*perf-cpp* drains the buffer automatically before it becomes full.

> [!NOTE]
> The number of buffer pages must be a power of two; other values are rounded up. *perf-cpp* adds one extra page for the buffer metadata automatically.

## Troubleshooting Counter Configurations
Enable debug mode to print the counter configuration passed to the perf subsystem:

```cpp
auto config = perf::SampleConfig{};
config.debug(true);

auto sampler = perf::Sampler{ config };
```

The equivalent in *Linux Perf*:
```bash
perf --debug perf-event-open record -- sleep 1
```

See the [counters documentation](counters.md) for more details on event codes and configuration.