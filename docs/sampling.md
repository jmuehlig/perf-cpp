# Event Sampling

*perf-cpp* enables the recording of event samples, capturing information such as instruction pointers, performance counter values, branch behavior, memory addresses, data sources, latencies, and more.  
Sampling occurs at a user-defined period or frequency, allowing precise control over when data is collected.

This mechanism is conceptually similar to tools like `perf record`, but is specifically designed to target defined blocks of code rather than profiling the entire application.

&rarr; [See what data can be recorded and how to access it](#what-can-be-recorded-and-how-to-access-the-data).

The sections below provide a general overview of sampling behavior.  
For information on sampling across multiple threads or cores, refer to the [parallel sampling documentation](sampling-parallel.md).

---
## Table of Contents
- [Interface](#interface)
  - [Setting up *what* to record and *when*](#setting-up-what-to-record-and-when)
  - [Initializing the Sampler *(optional)*](#initializing-the-sampler-optional)
  - [Managing Sampler Lifecycle](#managing-sampler-lifecycle)
  - [Retrieving Samples](#retrieving-samples)
  - [Closing the Sampler (*optional*)](#closing-the-sampler-optional)
- [Trigger](#trigger)
- [Precision](#precision)
- [Period / Frequency](#period--frequency)
- [What can be Recorded and How to Access the Data?](#what-can-be-recorded-and-how-to-access-the-data)
  - [Metadata](#metadata)
  - [Instruction Execution](#instruction-execution)
  - [Data Access](#data-access)
  - [Counter Values](#counter-values)
  - [Branch Stack](#branch-stack)
  - [User Stack](#user-stack)
  - [Registers](#registers)
  - [Context Switches](#context-switches)
  - [CGroup](#cgroup)
  - [Throttle and Unthrottle](#throttle-and-unthrottle)
  - [Lost Samples](#lost-samples)
- [Specific Notes for different CPU Vendors](#specific-notes-for-different-cpu-vendors)
  - [Intel (Processor Event Based Sampling)](#intel-processor-event-based-sampling)
  - [AMD (Instruction Based Sampling)](#amd-instruction-based-sampling)
- [Sample Buffer](#sample-buffer)
- [Troubleshooting Counter Configurations](#troubleshooting-counter-configurations)
---

## Interface
### Setting up *what* to record and *when*
During sampling, the hardware captures a specified set of data fields when a configured trigger event reaches its defined threshold  
([see what data can be recorded](#what-can-be-recorded-and-how-to-access-the-data) and [how trigger events work](#trigger)).

In the following example, a timestamp and the current instruction pointer are recorded every 4000th cycle:
```cpp
#include <perfcpp/sampler.h>
auto counter_definitions = perf::CounterDefinition{};

auto sample_config = perf::SampleConfig{};
sample_config.period(4000U);

auto sampler = perf::Sampler{ counter_definitions, sample_config };
sampler.trigger("cycles");
sampler.values().time(true).instruction_pointer(true);
```

> [!IMPORTANT]
> The `perf::CounterDefinition` instance is used to store event configurations (e.g., names) and passed as a reference.
Consequently, the instance needs to be alive while using the `Sampler` ([as described here](counters.md)).

## Initializing the Sampler *(optional)*
The sampler is initialized using `sampler.start()`, if it is not already done.
This action configures all necessary hardware counters and buffers, a process that may require some time. 
For those requiring **precise timing measurements** and wishing to omit the time spent setting up counters, the `sampler.open()` method can be invoked separately.

```cpp
try {
    sampler.open();
} catch (std::runtime_error& e) {
    std::cerr << e.what() << std::endl;
}
```

### Managing Sampler Lifecycle
Surround your computational code with `start()` and `stop()` methods to sample hardware events:

```cpp
try {
    sampler.start();
} catch (std::runtime_error& e) {
    std::cerr << e.what() << std::endl;
}

/// ... do some computational work here...

sampler.stop();
```

### Retrieving Samples
The output is a series of `perf::Sample` instances, each potentially including extensive data. 
Given the capability to select specific data elements for sampling, each data point is encapsulated within an `std::optional` to manage its potential absence.

&rarr; [See how to query sample results](#what-can-be-recorded-and-how-to-access-the-data)

```cpp
const auto result = sampler.result();

for (const auto& record : result)
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
```

The output may be something like this:

    Time = 124853764466887 | IP = 0x5794c991990c
    Time = 124853764663977 | IP = 0xffffffff8d79d48b
    Time = 124853764861377 | IP = 0x5794c991990c
    Time = 124853765058918 | IP = 0x5794c991990c
    Time = 124853765256328 | IP = 0x5794c991990c

### Closing the Sampler (*optional*)
Closing the sampler releases and un-maps all buffers and deactivates all counters. 
Additionally, the sampler automatically closes upon destruction. 
However, closing the sampler explicitly enables it to be reopened at a future time.

```cpp
sampler.close();
```

---

## Trigger
Each sampler is associated with one or more [trigger](#trigger) events.
When a trigger event reaches a specified (user-defined) threshold, the CPU records a sample containing the desired data. 
Triggers for a sampler can be specified as follows:

```cpp
sampler.trigger("cycles");
```

To define multiple triggers, use a vector of trigger names:

```cpp
sampler.trigger(std::vector<std::string>{"cycles", "instructions"});
```
In this scenario, exceeding either the cycles or instructions counter will prompt the CPU to capture a sample.

### Notes for specific CPUs
When configuring event-based sampling, it's important to understand that different CPU manufacturers support different sets of events that can be used as triggers.

Intel CPUs are generally flexible and allow almost every event as a trigger.
On AMD systems, the range of events that can trigger samples is more restricted: Typically, only the `cycles` event and specific IBS events such as `ibs_fetch` and `ibs_op` are supported.

> [!TIP]
> For more detailed information on configuring event-based sampling for different CPU types and specific notes on memory sampling, refer to the section: [Specific Notes for different CPU Vendors](#specific-notes-for-different-cpu-vendors).

## Precision
Due to deeply pipelined processors, samples might not be precise, i.e., a sample might contain an instruction pointer or memory address that did not generate the overflow (&rarr; see [a blogpost on easyperf.net](https://easyperf.net/blog/2019/04/03/Precise-timing-of-machine-code-with-Linux-perf) and [the perf documentation](https://man7.org/linux/man-pages/man2/perf_event_open.2.html)).
You can request a specific amount if skid through for each trigger, for example,

```cpp
sampler.trigger("cycles", perf::Precision::AllowArbitrarySkid);
```

The precision can have the following values:
* `perf::Precision::AllowArbitrarySkid` (this does **not** enable Intel PEBS)
* `perf::Precision::MustHaveConstantSkid` (default)
* `perf::Precision::RequestZeroSkid`
* `perf::Precision::MustHaveZeroSkid`

If you do not set any precision level through the `.trigger()` interface, you can control the *default* precision through the sample config:

```cpp
auto sample_config = perf::SampleConfig{};
sample_config.precision(perf::Precision::RequestZeroSkid);

auto sampler = perf::Sampler{ counter_definitions, sample_config };
sampler.trigger("cycles");
```

> [!NOTE]
> If the precision setting is too high and the perf subsystem fails to activate the trigger, *perf-cpp* will automatically reduce the precision. 
However, it will not increase precision autonomously.

## Period / Frequency
You can request a specific period **or** frequency for each trigger – basically how often the hardware should write samples –, for example,

```cpp
/// Every 4000th cycle.
sampler.trigger("cycles", perf::Period{4000U /* cycle */});
```

**or**

```cpp
/// With a frequency of 1000 samples per second , i.e., one sample per millisecond.
// (the hardware will adjust the period according to the provided frequency).
sampler.trigger("cycles", perf::Frequency{1000U /* Hz */});
```

You can also combine the configurations, for example, by
```cpp
/// Every 4000th cycle with zero skid.
sampler.trigger("cycles", perf::Precision::RequestZeroSkid, perf::Period{4000U});
```

If you do not set any precision level through the `.trigger()` interface, you can control the *default* period of frequency through the sample config:

```cpp
auto sample_config = perf::SampleConfig{};
sample_config.period(4000U /* trigger event, e.g., cycle */);
/// xor:
sample_config.frequency(1000U /* Hz */);

auto sampler = perf::Sampler{ counter_definitions, sample_config };
sampler.trigger("cycles");
```

---

## What can be Recorded and how to Access the Data?
Prior to activation, the sampler must be configured to specify the data to be recorded. 
For instance:

```cpp
sampler.values()
    .time(true)
    .instruction_pointer(true);
```

This specific configuration captures both the *timestamp* and *instruction pointer* within the sample record. 
Upon completing the sampling and [retrieving the sampling results](#retrieving-samples), the recorded fields can be accessed as follows:

```cpp
for (const auto& record : sampler.result()) {
    const auto timestamp = record.metadata().timestamp();
    const auto instruction = record.instruction_execution().logical_instruction_pointer();
}
```

See the information below to learn *what* information the sampler can record and *how* to access these.

---

> [!NOTE]
> A `record` in the following denotes to one record from the `sampler.result()` list.


### Metadata
Metadata associated with a sample can be accessed via `record.metadata()`.  
All metadata fields are returned as `std::optional`.

| Name           | Description                                                                                                                    | How to record?                      | How to access?                   | Type                                  |
|----------------|--------------------------------------------------------------------------------------------------------------------------------|-------------------------------------|----------------------------------|---------------------------------------|
| **Mode**       | Indicates the execution mode in which the sample was recorded (`Kernel`, `User`, `Hypervisor`, `GuestKernel`, or `GuestUser`). | Always recorded                     | `record.metadata().mode()`       | `std::optional<perf::Metadata::Mode>` |
| **Sample ID**  | Unique identifier for the sample's group leader.                                                                               | `sampler.values().sample_id(true)`  | `record.metadata().sample_id()`  | `std::optional<std::uint64_t>`        |
| **Stream ID**  | Unique identifier for the event that generated the sample.                                                                     | `sampler.values().stream_id(true)`  | `record.metadata().stream_id()`  | `std::optional<std::uint64_t>`        |
| **Timestamp**  | Records the time at which the sample was taken.                                                                                | `sampler.values().timestamp(true)`  | `record.metadata().timestamp()`  | `std::optional<std::uint64_t>`        |
| **Period**     | Indicates the event count threshold that triggered the sample.                                                                 | `sampler.values().period(true)`     | `record.metadata().period()`     | `std::optional<std::uint64_t>`        |
| **CPU ID**     | Identifies the CPU core where the sample was recorded.                                                                         | `sampler.values().cpu_id(true)`     | `record.metadata().cpu_id()`     | `std::optional<std::uint32_t>`        |
| **Process ID** | Identifies the process context in which the sample was recorded.                                                               | `sampler.values().process_id(true)` | `record.metadata().process_id()` | `std::optional<std::uint32_t>`        |
| **Thread ID**  | Identifies the thread context in which the sample was recorded.                                                                | `sampler.values().thread_id(true)`  | `record.metadata().thread_id()`  | `std::optional<std::uint34_t>`        |

### Instruction Execution

Instruction-level information is accessible via `record.instruction_execution()`.  
All fields are returned as `std::optional`, unless otherwise noted.

| Name                             | Description                                                                                                                        | How to record?                                                                              | How to access?                                                  | Type                                                                  |
|----------------------------------|------------------------------------------------------------------------------------------------------------------------------------|---------------------------------------------------------------------------------------------|-----------------------------------------------------------------|-----------------------------------------------------------------------|
| **Instruction Type**             | The type of the sampled instruction (`Return`, `Branch`, or `DataAccess`) (the first two only on [**AMD's Op PMU**](#ibs-op-pmu)). | `sampler.values().data_src(true)` for `DataAccess`; `sampler.values().raw(true)` for others | `record.instruction_execution().type()`                         | `std::optional<perf::InstructionExecution::InstructionType>`          |
| **Logical Instruction Pointer**  | The logical address of the sampled instruction.                                                                                    | `sampler.values().instruction_pointer(true)`                                                | `record.instruction_execution().logical_instruction_pointer()`  | `std::optional<std::uintptr_t>`                                       |
| **Physical Instruction Pointer** | The physical address of the sampled instruction ([**AMD's Fetch PMU**](#ibs-fetch-pmu) only).                                      | `sampler.values().raw(true)`                                                                | `record.instruction_execution().physical_instruction_pointer()` | `std::optional<std::uintptr_t>`                                       |
| **Is Instruction Pointer Exact** | Indicates that the recorded instruction pointer exactly corresponds to the sampled instruction.                                    | `sampler.values().instruction_pointer(true)`                                                | `record.instruction_execution().is_instruction_pointer_exact()` | `bool`                                                                |
| **Is Locked**                    | Indicates that the sampled instruction was a locked operation.                                                                     | `sampler.values().data_src(true)`                                                           | `record.instruction_execution().is_locked()`                    | `std::optional<bool>`                                                 |
| **Branch Type**                  | The type of branch, if applicable (`Taken`, `Retired`, `Mispredicted`, `Fuse`) ([**AMD's Op PMU**](#ibs-op-pmu) only) .            | `sampler.values().raw(true)`                                                                | `record.instruction_execution().branch_type()`                  | `std::optional<perf::InstructionExecution::BranchType>`               |
| **Callchain**                    | The callchain of the sampled instruction.                                                                                          | `sampler.values().callchain(true)` or a `std::uint32_t` for maximum depth                   | `record.instruction_execution().callchain()`                    | `std::optional<std::vector<std::uintptr_t>>`                          |
| **Code Page Size**               | Indicates the page size of the instruction pointer (from Linux `5.11`).                                                            | `sampler.values().code_page_size(true)`                                                     | `record.instruction_execution().page_size()`                    | `std::optional<std::uint64_t>`                                        |
| **Latency**                      | Captures latency information of instruction execution and fetch.                                                                   | [See details below](#instruction-latency)                                                   | `record.instruction_execution().latency()`                      | `perf::InstructionExecution::Latency`                                 |
| **Cache**                        | Captures cache-related information from the instruction fetch stage.                                                               | [See details below](#instruction-cache)                                                     | `record.instruction_execution().cache()`                        | `std::optional<perf::InstructionExecution::Cache>`                    |
| **TLB**                          | Captures TLB information.                                                                                                          | [See details below](#instruction-tlb)                                                       | `record.instruction_execution().tlb()`                          | `std::optional<perf::InstructionExecution::TLB>`                      |
| **Fetch**                        | Captures instruction fetch-specific information.                                                                                   | [See details below](#instruction-fetch)                                                     | `record.instruction_execution().fetch()`                        | `std::optional<perf::InstructionExecution::Fetch>`                    |
| **Hardware Transaction Abort**   | Provides information on transactional memory aborts.                                                                               | [See details below](#hardware-transaction-abort)                                            | `record.instruction_execution().hardware_transaction_abort()`   | `std::optional<perf::InstructionExecution::HardwareTransactionAbort>` |

**Example:** [`examples/instruction_pointer_sampling.cpp`](../examples/instruction_pointer_sampling.cpp)

#### Instruction Latency
Latency information captures timing characteristics for instruction execution or micro-operations (on AMD).  
All fields are returned as `std::optional`.

| Name                             | Description                                                                                                                     | How to record?                       | How to access?                                                            | Type                           |
|----------------------------------|---------------------------------------------------------------------------------------------------------------------------------|--------------------------------------|---------------------------------------------------------------------------|--------------------------------|
| **Instruction Retirement**       | The total latency (in cycles) to execute the instruction, including TLB and memory accesses. (**Intel** only)                   | `sampler.values().latency(true)`     | `record.instruction_execution().latency().instruction_retirement()`       | `std::optional<std::uint32_t>` |
| **uOp Tag-to-Retirement**        | The number of cycles from tagging a uOp to its retirement ([**AMD's Op PMU**](#ibs-op-pmu) only).                               | `sampler.values().latency(true)`     | `record.instruction_execution().latency().uop_tag_to_retirement()`        | `std::optional<std::uint32_t>` |
| **uOp Completion-to-Retirement** | The number of cycles from uOp completion to retirement ([**AMD's Op PMU**](#ibs-op-pmu) only).                                  | `sampler.values().raw(true)`         | `record.instruction_execution().latency().uop_completion_to_retirement()` | `std::optional<std::uint32_t>` |
| **uOp Tag-to-Completion**        | The number of cycles from tagging a uOp to its completion ([**AMD's Op PMU**](#ibs-op-pmu) only).                               | `sampler.values().raw(true)`         | `record.instruction_execution().latency().uop_tag_to_completion()`        | `std::optional<std::uint32_t>` |
| **Fetch**                        | The instruction fetch latency (in cycles) from initiation to delivery to the core ([**AMD's Fetch PMU**](#ibs-fetch-pmu) only). | `sampler.values().raw(true)`         | `record.instruction_execution().latency().fetch()`                        | `std::optional<std::uint32_t>` |

#### Instruction Cache
Provides cache-related information about instruction fetches.  
This is available only on [**AMD's Fetch PMU**](#ibs-fetch-pmu).  
Note that `record.instruction_execution().cache()` returns an `std::optional`.

| Name              | Description                                                           | How to record?               | How to access?                                               | Type   |
|-------------------|-----------------------------------------------------------------------|------------------------------|--------------------------------------------------------------|--------|
| **L1 Cache Miss** | Indicates that the instruction fetch missed the L1 instruction cache. | `sampler.values().raw(true)` | `record.instruction_execution().cache()->is_l1_cache_miss()` | `bool` |
| **L2 Cache Miss** | Indicates that the instruction fetch missed the L2 cache.             | `sampler.values().raw(true)` | `record.instruction_execution().cache()->is_l2_cache_miss()` | `bool` |
| **L3 Cache Miss** | Indicates that the instruction fetch missed the L3 cache.             | `sampler.values().raw(true)` | `record.instruction_execution().cache()->is_l3_cache_miss()` | `bool` |

#### Instruction TLB
Provides TLB information related to instruction fetch.  
This is available only on [**AMD's Fetch PMU**](#ibs-fetch-pmu).  
Note that `record.instruction_execution().tlb()` returns an `std::optional`.

| Name              | Description                                                                | How to record?               | How to access?                                         | Type            |
|-------------------|----------------------------------------------------------------------------|------------------------------|--------------------------------------------------------|-----------------|
| **L1 Cache Miss** | Indicates that the instruction fetch missed the L1 instruction TLB (iTLB). | `sampler.values().raw(true)` | `record.instruction_execution().tlb()->is_l1_miss()`   | `bool`          |
| **L2 Cache Miss** | Indicates that the instruction fetch missed the second-level TLB (STLB).   | `sampler.values().raw(true)` | `record.instruction_execution().tlb()->is_l2_miss()`   | `bool`          |
| **L1 Page Size**  | The page size used in the L1 instruction TLB.                              | `sampler.values().raw(true)` | `record.instruction_execution().tlb()->l1_page_size()` | `std::uint64_t` |

#### Instruction Fetch
Provides details about instruction fetch behavior during micro-op execution.  
This is available only on [**AMD's Fetch PMU**](#ibs-fetch-pmu).  
Note that `record.instruction_execution().fetch()` returns an `std::optional`.

| Name                  | Description                                                 | How to record?               | How to access?                                          | Type   |
|-----------------------|-------------------------------------------------------------|------------------------------|---------------------------------------------------------|--------|
| **Is Fetch Complete** | Indicates that the instruction fetch process completed.     | `sampler.values().raw(true)` | `record.instruction_execution().fetch()->is_complete()` | `bool` |
| **Is Fetch Valid**    | Indicates that the instruction fetch is considered valid.   | `sampler.values().raw(true)` | `record.instruction_execution().fetch()->is_valid()`    | `bool` |

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

> [!NOTE]
> Sampling for memory accesses (memory address, cache information, etc.) is only supported using [**AMD's IBS Op PMU**](#ibs-op-pmu) and [**Intel PEBS `mem-load`/`mem-store` events**](#intel-processor-event-based-sampling).

| Name                        | Description                                                                                       | How to record?                                   | How to access?                                   | Type                                      |
|-----------------------------|---------------------------------------------------------------------------------------------------|--------------------------------------------------|--------------------------------------------------|-------------------------------------------|
| **Is load**                 | Indicates that the access was a load operation.                                                   | `sampler.values().data_src(true)`                | `record.data_access().is_load()`                 | `bool`                                    |
| **Is Store**                | Indicates that the access was a store operation.                                                  | `sampler.values().data_src(true)`                | `record.data_access().is_store()`                | `bool`                                    |
| **Is Software Prefetch**    | Indicates that the access was a software prefetch ([**AMD's Op PMU**](#ibs-op-pmu) only).         | `sampler.values().raw(true)`                     | `record.data_access().is_software_prefetch()`    | `bool`                                    |
| **Logical Memory Address**  | The logical address of the accessed memory.                                                       | `sampler.values().logical_memory_address(true)`  | `record.data_access().logical_memory_address()`  | `std::optional<std::uintptr_t>`           |
| **Physical Memory Address** | The physical address of the accessed memory (from Linux `4.13`).                                  | `sampler.values().physical_memory_address(true)` | `record.data_access().physical_memory_address()` | `std::optional<std::uintptr_t>`           |
| **Source**                  | Provides information about the memory or cache source of the access.                              | [See details below](#data-source)                | `record.data_access().source()`                  | `std::optional<perf::DataAccess::Source>` |
| **Latency**                 | Provides latency details for the data access.                                                     | [See details below](#data-latency)               | `record.data_access().latency()`                 | `perf::DataAccess::Latency`               |
| **TLB**                     | Provides TLB-related information for the access.                                                  | [See details below](#data-tlb)                   | `record.data_access().tlb()`                     | `perf::DataAccess::TLB`                   |
| **Snoop**                   | Provides Snoop-related information for the access.                                                | [See details below](#data-snoop)                 | `record.data_access().snoop()`                   | `std::optional<perf::DataAccess::Snoop>`  |
| **Is Misalign Penalty**     | Indicates that the access incurred a misalignment penalty ([**AMD's Op PMU**](#ibs-op-pmu) only). | `sampler.values().raw(true)`                     | `record.data_access().is_misaligned_penalty()`   | `std::optional<bool>`                     |
| **Access Width**            | The size (in bytes) of the accessed data ([**AMD's Op PMU**](#ibs-op-pmu) only).                  | `sampler.values().raw(true)`                     | `record.data_access().access_width()`            | `std::optional<std::uint8_t>`             |
| **Data Page Size**          | The page size of the instruction pointer (from Linux `5.11`).                                     | `sampler.values().data_page_size(true)`          | `record.data_access().page_size()`               | `std::optional<std::uint64_t>`            |

**Example:** [`examples/address_sampling.cpp`](../examples/address_sampling.cpp)

#### Data Source
Provides detailed information about the memory or cache source involved in a data access.  
Note that `record.data_access().source()` returns an `std::optional`.

| Name                              | Description                                                                                             | How to record?                       | How to access?                                                 | Type                          |
|-----------------------------------|---------------------------------------------------------------------------------------------------------|--------------------------------------|----------------------------------------------------------------|-------------------------------|
| **Is L1 Hit**                     | Indicates that the access hit the L1 data cache (L1d).                                                  | `sampler.values().data_access(true)` | `record.data_access().source()->is_l1_hit()`                   | `bool`                        |
| **Is MHB Hit**                    | Indicates that the access hit the LFB (Intel) or MAB (AMD).                                             | `sampler.values().data_access(true)` | `record.data_access().source()->is_mhb_hit()`                  | `std::optional<bool>`         |
| **Number of Allocated MHB Slots** | The number of MAB (AMD) slots allocated at the time of sampling ([**AMD's Op PMU**](#ibs-op-pmu) only). | `sampler.values().raw(true)`         | `record.data_access().source()->num_mhb_slots_allocated()`     | `std::optional<std::uint8_t>` |
| **Is L2 Hit**                     | Indicates that the access hit the L2 cache.                                                             | `sampler.values().data_access(true)` | `record.data_access().source()->is_l2_hit()`                   | `bool`                        |
| **Is L3 Hit**                     | Indicates that the access hit the L3 cache.                                                             | `sampler.values().data_access(true)` | `record.data_access().source()->is_l3_hit()`                   | `bool`                        |
| **Is Memory Hit**                 | Indicates that the access missed all caches and was served from memory.                                 | `sampler.values().data_access(true)` | `record.data_access().source()->is_memory_hit()`               | `bool`                        |
| **Is Remote**                     | Indicates that the access was served by a remote core or node (cache or memory).                        | `sampler.values().data_access(true)` | `record.data_access().source()->is_remote()`                   | `bool`                        |
| **Is Same Node Remote Core**      | Indicates that the access was served by another core on the same node.                                  | `sampler.values().data_access(true)` | `record.data_access().source()->is_same_node_remote_core()`    | `std::optional<bool>`         |
| **Is Same Socket Remote Node**    | Indicates that the access was served by another node on the same socket.                                | `sampler.values().data_access(true)` | `record.data_access().source()->is_same_socket_remote_node()`  | `std::optional<bool>`         |
| **Is Same Board Remote Socket**   | Indicates that the access was served by another socket on the same board.                               | `sampler.values().data_access(true)` | `record.data_access().source()->is_same_board_remote_socket()` | `std::optional<bool>`         |
| **Is Remote Board**               | Indicates that the access was served by another board.                                                  | `sampler.values().data_access(true)` | `record.data_access().source()->is_remote_board()`             | `std::optional<bool>`         |
| **Is Uncachable Memory**          | Indicates that the access targeted uncachable memory.                                                   | `sampler.values().data_access(true)` | `record.data_access().source()->is_uncachable_memory()`        | `std::optional<bool>`         |
| **Is Write Combine Memory**       | Indicates that the access targeted write-combine memory.                                                | `sampler.values().data_access(true)` | `record.data_access().source()->is_write_combine()`            | `std::optional<bool>`         |

#### Data Latency
Provides latency measurements associated with data access operations.  
All fields are returned as `std::optional`.

| Name            | Description                                                                                             | How to record?                   | How to access?                                 | Type                           |
|-----------------|---------------------------------------------------------------------------------------------------------|----------------------------------|------------------------------------------------|--------------------------------|
| **Data Access** | The latency (in cycles) for completing the data access (**Intel** only).                                | `sampler.values().latency(true)` | `record.data_access().latency().data_access()` | `std::optional<std::uint32_t>` |
| **Cache Miss**  | The latency (in cycles) caused by an L1d cache miss ([**AMD's Op PMU**](#ibs-op-pmu) only).             | `sampler.values().latency(true)` | `record.data_access().latency().cache_miss()`  | `std::optional<std::uint32_t>` |
| **dTLB Refill** | The latency (in cycles) for refilling the data TLB after a miss ([**AMD's Op PMU**](#ibs-op-pmu) only). | `sampler.values().raw(true)`     | `record.data_access().latency().dtlb_refill()` | `std::optional<std::uint32_t>` |

#### Data TLB
Provides information about dTLB and STLB access behavior.  
All fields are returned as `std::optional`.

| Name             | Description                                                                                           | How to record?                       | How to access?                              | Type                           |
|------------------|-------------------------------------------------------------------------------------------------------|--------------------------------------|---------------------------------------------|--------------------------------|
| **Is L1 Hit**    | Indicates that the data access hit the L1 data TLB (dTLB).                                            | `sampler.values().data_source(true)` | `record.data_access().tlb().is_l1_hit()`    | `std::optional<bool>`          |
| **Is L2 Hit**    | Indicates that the data access hit the second-level TLB (STLB).                                       | `sampler.values().data_source(true)` | `record.data_access().tlb().is_l2_hit()`    | `std::optional<bool>`          |
| **L1 Page Size** | The page size of the translation associated with the dTLB hit ([**AMD's Op PMU**](#ibs-op-pmu) only). | `sampler.values().raw(true)`         | `record.data_access().tlb().l1_page_size()` | `std::optional<std::uint64_t>` |
| **L2 Page Size** | The page size of the translation associated with the STLB hit ([**AMD's Op PMU**](#ibs-op-pmu) only). | `sampler.values().raw(true)`         | `record.data_access().tlb().l2_page_size()` | `std::optional<std::uint64_t>` |

> [!NOTE]  
> **Intel** systems do not distinguish between L1 and L2 TLB hits.  
> If a TLB hit occurs, both `is_l1_hit()` and `is_l2_hit()` will return `true`.

#### Data Snoop
Provides information about snooping access behavior.  
All fields are returned as `std::optional`.

| Name                      | Description                                                                 | How to record?                       | How to access?                                          | Type                  |
|---------------------------|-----------------------------------------------------------------------------|--------------------------------------|---------------------------------------------------------|-----------------------|
| **Is Hit**                | Indicates that the data access is a snoop hit (`true`) or a miss (`false`). | `sampler.values().data_source(true)` | `record.data_access().snoop()->is_hit()`                | `std::optional<bool>` |
| **Is Hit Modified**       | `True` if the hit cache line is dirty.                                      | `sampler.values().data_source(true)` | `record.data_access().snoop()->is_hit_modified()`       | `std::optional<bool>` |
| **Is Forward**            | Indicates that the cache line is forwarded.                                 | `sampler.values().data_source(true)` | `record.data_access().snoop()->is_fardwarded()`         | `std::optional<bool>` |
| **Is Transfer from Peer** | Indicates that the cache line is transferred from another node.             | `sampler.values().data_source(true)` | `record.data_access().snoop()->is_transfer_from_peer()` | `std::optional<bool>` |

### Counter Values
Records hardware performance event values (e.g., `cycles`, `L1-dcache-loads`, etc.) and derived metrics at the time each sample is taken.  
Refer to the documentation on [recording events](recording.md) and [metrics](metrics.md) for more information.

| Name               | Description                                              | How to record?                                                                                           | How to access?     | Type                                                                            |
|--------------------|----------------------------------------------------------|----------------------------------------------------------------------------------------------------------|--------------------|---------------------------------------------------------------------------------|
| **Counter Values** | Captures the values of the specified performance events. | `sampler.values().counter({"cycles", "instructions", "cycles-per-instruction"})` (example counter names) | `record.counter()` | `perf::CounterResult` (see the [recording events](recording.md) documentation). |

**Example:** [`examples/counter_sampling.cpp`](../examples/counter_sampling.cpp)

### Branch Stack
Captures the branch stack recorded by the CPU at the time of sampling.  
This can include call and jump instructions, conditional branches, and transactional memory branches.  
Note that `record.branch_stack()` returns an `std::optional`.

| Name             | Description                                                | How to record?                                                                                             | How to access?          | Type                                       |
|------------------|------------------------------------------------------------|------------------------------------------------------------------------------------------------------------|-------------------------|--------------------------------------------|
| **Branch Stack** | Records the current branch stack of the CPU.               | `sampler.values().branch_stack({perf::BranchType::Call, perf::BranchType::Conditional})` (see types below) | `record.branch_stack()` | `std::optional<std::vector<perf::Branch>>` |

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

| Name                         | Description                                                       | How to access?                                            | Type                           |
|------------------------------|-------------------------------------------------------------------|-----------------------------------------------------------|--------------------------------|
| **Instruction Pointer From** | The instruction pointer where the branch originated.              | `record.branch_stack()->at(i).instruction_pointer_from()` | `std::uintptr_t`               |
| **Instruction Pointer To**   | The instruction pointer where the branch target landed.           | `record.branch_stack()->at(i).instruction_pointer_to()`   | `std::uintptr_t`               |
| **Is Mispredicted**          | Indicates that the branch was mispredicted.                       | `record.branch_stack()->at(i).is_mispredicted()`          | `bool`                         |
| **Is Predicted**             | Indicates that the branch was predicted correctly.                | `record.branch_stack()->at(i).is_predicted()`             | `bool`                         |
| **Is In Transaction**        | Indicates that the branch occurred during a hardware transaction. | `record.branch_stack()->at(i).is_in_transaction()`        | `bool`                         |
| **Is Transaction Abort**     | Indicates that the branch aborted a hardware transaction.         | `record.branch_stack()->at(i).is_transaction_abort()`     | `bool`                         |
| **Cycles**                   | The number of cycles for the branch (if supported).               | `record.branch_stack()->at(i).cycles()`                   | `std::optional<std::uint64_t>` |

**Example:** [`examples/branch_sampling.cpp`](../examples/branch_sampling.cpp)

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

| Name               | Description                                      | How to access?                                                                               | Type                          |
|--------------------|--------------------------------------------------|----------------------------------------------------------------------------------------------|-------------------------------|
| **Register Value** | The value of a specific register.                | `record.user_registers()->value(perf::Registers::x86::AX)` (example register)                | `std::optional<std::int64_t>` |
| **ABI**            | The ABI used when capturing the register values. | `record.user_registers()->abi()`                                                             | `perf::ABI`                   |

**Example:** [`examples/register_sampling.cpp`](../examples/register_sampling.cpp)

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

If recorded, the following [metadata fields](#metadata) will also be included:
- Timestamp
- Stream ID
- CPU ID
- Sample ID

**Example:** [`examples/context_switch_sampling.cpp`](../examples/context_switch_sampling.cpp)

### CGroup
Captures information about control groups (cgroups) associated with each sample.  
Sampling cgroups requires a Linux kernel version of `5.7` or higher.  
Note that `record.cgroup()` returns an `std::optional`.

| Name                | Description                                 | How to record?                  | How to access?            | Type                           |
|---------------------|---------------------------------------------|---------------------------------|---------------------------|--------------------------------|
| **CGroup ID**       | The ID of the cgroup the sample belongs to. | `sampler.values().cgroup(true)` | `record.cgroup_id()`      | `std::optional<std::uint64_t>` |
| **New CGroup ID**   | The ID of a newly added cgroup.             | `sampler.values().cgroup(true)` | `record.cgroup()->id()`   | `std::uint64_t`                |
| **New CGroup Path** | The path of a newly added cgroup.           | `sampler.values().cgroup(true)` | `record.cgroup()->path()` | `std::string`                  |

If recorded, the following [metadata fields](#metadata) will also be included:
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

If recorded, the following [metadata fields](#metadata) will also be included:
- Timestamp
- Process ID
- Thread ID
- Stream ID
- CPU ID
- Sample ID

### Lost Samples
Sample loss can occur when buffers overflow or the CPU is under high load.  
This section records how many samples were lost during profiling.  
Note that `record.count_loss()` returns an `std::optional`.

| Name                  | Description                 | How to record?  | How to access?        | Type                           |
|-----------------------|-----------------------------|-----------------|-----------------------|--------------------------------|
| **Count Loss Events** | The number of lost samples. | Always recorded | `record.count_loss()` | `std::optional<std::uint64_t>` |

If recorded, the following [metadata fields](#metadata) will also be included:
- Timestamp
- Process ID
- Thread ID
- Stream ID
- CPU ID
- Sample ID

---

## Specific Notes for different CPU Vendors
### Intel (Processor Event Based Sampling)
Especially for sampling memory addresses, latency, and data source, the perf subsystem needs specific events as triggers.
On Intel, the `perf list` command reports these triggers as "*Supports address when precise*".

*perf-cpp*  will discover `mem-loads` and `mem-stores` events when running on Intel hardware that supports sampling for memory.

Additionally, memory sampling typically requires a [precision](#precision) setting of at least `perf::Precision::RequestZeroSkid`.

#### Before Sapphire Rapids
From our experience, Intel's Cascade Lake architecture (and earlier architectures) only reports latency and source for memory loads, not stores – this changes from Sapphire Rapids.

You can add load and store events like this:

```cpp
sampler.trigger("mem-loads", perf::Precision::MustHaveZeroSkid); /// Only load events
```
&rarr; [See code example](../examples/address_sampling.cpp)

or
```cpp
sampler.trigger("mem-stores", perf::Precision::MustHaveZeroSkid); /// Only store events
```
or
```cpp
/// Load and store events
sampler.trigger(std::vector<std::vector<perf::Sampler::Trigger>>{
    {
      perf::Sampler::Trigger{ "mem-loads", perf::Precision::RequestZeroSkid } /// Loads
    },
    { perf::Sampler::Trigger{ "mem-stores", perf::Precision::MustHaveZeroSkid } } /// Stores
  });
```
&rarr; [See code example](../examples/multi_event_sampling.cpp)

#### Sapphire Rapids and Beyond
To use memory latency sampling on Intel's Sapphire Rapids architecture, the perf subsystem **needs an auxiliary counter** to be added to the group, before the first "real" counter is added (see [this commit](https://lore.kernel.org/lkml/1612296553-21962-3-git-send-email-kan.liang@linux.intel.com/)).

> [!IMPORTANT]
> Starting with version `0.10.0`, *perf-cpp* will **automatically define and enable this counter** as a trigger when the hardware requires it. 
> In such cases, you can continue as normal by simply adding the mem-loads counter.
> However, if the detection fails but the system needs it, you can add it yourself:

```cpp
sampler.trigger({
    { 
        perf::Sampler::Trigger{"mem-loads-aux", perf::Precision::MustHaveZeroSkid},     /// Helper
        perf::Sampler::Trigger{"mem-loads", perf::Precision::RequestZeroSkid}           /// First "real" counter
    },
    { perf::Sampler::Trigger{"mem-stores", perf::Precision::MustHaveZeroSkid} }         /// Other "real" counters.
  });
```

> [!TIP]
> You can check if the auxiliary counter is required by checking if the following file exists in the system:

```
/sys/bus/event_source/devices/cpu/events/mem-loads-aux
```

### AMD (Instruction Based Sampling)
AMD uses Instruction Based Sampling to tag instructions randomly for sampling and collect various information for each sample ([see the programmer reference](https://www.amd.com/content/dam/amd/en/documents/processor-tech-docs/programmer-references/24593.pdf)).
IBS comes with two different PMUs of which only one can be actively selected at a time (also see the [perf documentation](https://man7.org/linux/man-pages/man1/perf-amd-ibs.1.html)).

#### IBS Op PMU
The *IBS Op PMU* offers information on micro-op execution, including data cache hit/miss, data TLB hit/miss, latency, load/store data source, branch behavior, and so on.
In contrast to Intel's mechanism, IBS cannot tag specific load and store instructions (and apply a filter on the latency).
In case the instruction was a load/store instruction, the sample will include data source, latency, and a memory address ([see kernel mailing list](https://lore.kernel.org/all/20220616113638.900-2-ravi.bangoria@amd.com/T/)).

*perf-cpp* –or the `perf::CounterDefinition` class to be precise– will detect IBS support on AMD devices and adds the following counters that can be used as **trigger** for sampling on AMD:
- `ibs_op` selects instructions during the execution pipeline. CPU cycles (on the specified period/frequency) will lead to tag an instruction.
- `ibs_op_uops` selects instructions during the execution pipeline, **but** the period/frequency refers to the number of executed micro-operations, **not** CPU cycles.
- `ibs_op_l3missonly` selects instructions during the execution pipeline that miss the L3 cache. CPU cycles are used as the trigger.
- `ibs_op_uops_l3missonly` selects instructions during the execution pipeline that miss the L3 cache, using micro-operations as the trigger.

#### IBS Fetch PMU
The *IBS Fetch PMU* offers information on instruction fetch, including data such as instruction cache hit/miss, instruction TLB hit/miss, fetch latency, and more.

*perf-cpp* provides IBS support on AMD devices and adds the following counters that can be used as **trigger** for sampling on AMD:
- `ibs_fetch` selects instructions in the fetch-state (frontend) using cycles as the trigger.
- `ibs_fetch_l3missonly` selects instructions in the fetch-state (frontend) that miss the L3 cache, again, using cycles as a trigger.


---

## Sample Buffer
The hardware transfers collected samples into an mmap-ed [ring buffer](https://docs.kernel.org/userspace-api/perf_ring_buffer.html).
You can configure the size of this buffer using the `SampleConfig` class as demonstrated below:

```cpp
auto sample_config = perf::SampleConfig{};
sample_config.buffer_pages(4096U); // This sets the buffer to 16MB (4096 pages x 4kB per page).

auto sampler = perf::Sampler{ counter_definitions, sample_config };
```

Because the ring buffer has a finite size, it needs to be drained before it becomes full.
*perf-cpp* handles this automatically, though copying the data can be expensive.
Choosing the right buffer size involves balancing memory usage against the cost of frequent data copying.
By default, the buffer is set to `16`MB.

> [!NOTE]
> The number of buffer pages must be a power of two; any non-power-of-two value will be rounded up accordingly.


## Troubleshooting Counter Configurations
Debugging and configuring hardware counters can sometimes be complex, as settings (e.g., the precision – `precise_ip`) may need to be adjusted for different machines.
Utilize *perf-cpp*'s debugging features to gain insights into the internal workings of performance counters and troubleshoot any configuration issues:

```cpp
auto config = perf::SampleConfig{};
config.is_debug(true);

auto sampler = perf::Sampler{ counter_definitions, config };
```

The idea is borrowed from *Linux Perf*, which can be asked to print counter configurations as follows:
```bash
perf --debug perf-event-open stat -- sleep 1
```

This command helps visualize configurations for various counters, which is also beneficial for retrieving event codes (for more details, see the [counters documentation](counters.md)).