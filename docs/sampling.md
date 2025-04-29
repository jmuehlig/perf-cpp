# Event Sampling

*perf-cpp* enables the recording of event samples, capturing details like instruction pointers, multiple counters, branches, memory addresses, data sources, latency, and more. 
Essentially, you define a sampling period or frequency at which data is captured. 
At its core, this functionality is akin to traditional profiling tools, like `perf record`, but uniquely tailored to record specific blocks of code rather than the entire application.

&rarr; [See details below](#what-can-be-recorded-and-how-to-access-the-data).

The details below provide an overview of how sampling works.
For specific information about sampling in parallel settings (i.e., sampling multiple threads and cores) take a look [into the "parallel sampling" documentation](sampling-parallel.md).

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
- [Sample Buffer](#sample-buffer)
- [What can be Recorded and how to Access the Data?](#what-can-be-recorded-and-how-to-access-the-data)
  - [Metadata](#metadata)
  - [Instruction Execution](#instruction-execution)
  - [Context Switches](#context-switches)
  - [CGroup](#cgroup)
  - [Throttle and Unthrottle Events](#throttle-and-unthrottle-events)
- [Sample mode](#sample-mode)
- [Lost Samples](#lost-samples)
- [Specific Notes for different CPU Vendors](#specific-notes-for-different-cpu-vendors)
  - [Intel (PEBS)](#intel-pebs)
  - [AMD (Instruction Based Sampling)](#amd-instruction-based-sampling)
- [Troubleshooting Counter Configurations](#troubleshooting-counter-configurations)
---

## Interface
### Setting up *what* to record and *when*
For sampling, the hardware records a set of data ([see more details](#what-can-be-recorded-and-how-to-access-the-data)) upon reaching the threshold of a specific trigger event ([see more details](#trigger)).
In the following example, we record a timestamp and the current instruction pointer every 4000th cycle:

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

for (const auto& sample_record : result)
{
    const auto timestamp = sample_record.metadata().timestamp();
    const auto instruction = sample_record.instruction_execution().logical_instruction_pointer();
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
sampler.trigger("cycles", perf::Period{4000U});
```

**or**

```cpp
/// With a frequency of 1000 samples per second , i.e., one sample per millisecond.
// (the hardware will adjust the period according to the provided frequency).
sampler.trigger("cycles", perf::Frequency{1000U});
```

You can also combine the configurations, for example, by
```cpp
/// Every 4000th cycle with zero skid.
sampler.trigger("cycles", perf::Precision::RequestZeroSkid, perf::Period{4000U});
```

If you do not set any precision level through the `.trigger()` interface, you can control the *default* period of frequency through the sample config:

```cpp
auto sample_config = perf::SampleConfig{};
sample_config.period(4000U);
/// xor:
sample_config.frequency(1000U);

auto sampler = perf::Sampler{ counter_definitions, sample_config };
sampler.trigger("cycles");
```

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

## What can be Recorded and how to Access the Data?
Prior to activation, the sampler must be configured to specify the data to be recorded. For instance:

```cpp
sampler.values()
    .time(true)
    .instruction_pointer(true);
```

This specific configuration captures both the *timestamp* and *instruction pointer* within the sample record. 
Upon completing the sampling and [retrieving the sampling results](#retrieving-samples), the recorded fields can be accessed as follows:

```cpp
for (const auto& sample_record : sampler.results()) {
    const auto timestamp = sample_record.metadata().timestamp();
    const auto instruction = sample_record.instruction_execution().logical_instruction_pointer();
}
```

See the information below to learn *what* information the sampler can record and *how* to access these.

---

### Metadata
Recorded metadata can be accessed via `sample_record.metadata()` and contains the following information.
Note that all fields of `sample_record.metadata()` are `std::optional`.

| Name           | Description                                                                                                 | How to record?                      | How to access?                          | Type                   |
|----------------|-------------------------------------------------------------------------------------------------------------|-------------------------------------|-----------------------------------------|------------------------|
| **Mode**       | The mode in which the sample was recorded (`Kernel`, `User`, `Hypervisor`, `GuestKernel`, or `GuestUser`).  | Always recorded                     | `sample_record.metadata().mode()`       | `perf::Metadata::Mode` |
| **Sample ID**  | Unique ID for the sample's group leader.                                                                    | `sampler.values().sample_id(true)`  | `sample_record.metadata().sample_id()`  | `std::uint64_t`        |
| **Stream ID**  | Unique ID for the sample's event.                                                                           | `sampler.values().stream_id(true)`  | `sample_record.metadata().stream_id()`  | `std::uint64_t`        |
| **Timestamp**  | Timestamp of the sample.                                                                                    | `sampler.values().timestamp(true)`  | `sample_record.metadata().timestamp()`  | `std::uint64_t`        |
| **Period**     | Period of the sample.                                                                                       | `sampler.values().period(true)`     | `sample_record.metadata().period()`     | `std::uint64_t`        |
| **CPU ID**     | ID of the CPU core the sample was recorded on.                                                              | `sampler.values().cpu_id(true)`     | `sample_record.metadata().cpu_id()`     | `std::uint32_t`        |
| **Process ID** | ID of the process the sample was recorded in.                                                               | `sampler.values().process_id(true)` | `sample_record.metadata().process_id()` | `std::uint32_t`        |
| **Thread ID**  | ID of the thread the sample was recorded in.                                                                | `sampler.values().thread_id(true)`  | `sample_record.metadata().thread_id()`  | `std::uint34_t`        |

### Instruction Execution
Information about the instruction execution can be accessed via `sample_record.instruction_execution()` and contain the following information.

| Name                             | Description                                                                                                             | How to record?                                                                                                                                          | How to access?                                                         | Type                                                   |
|----------------------------------|-------------------------------------------------------------------------------------------------------------------------|---------------------------------------------------------------------------------------------------------------------------------------------------------|------------------------------------------------------------------------|--------------------------------------------------------|
| **Instruction Type**             | Type of the sampled instruction (`Return`, `Branch`, `MemoryLoad`, `MemoryStore`, or `SoftwarePrefetch`).               | `sampler.values().data_src(true)` for `Memory*` types and `sampler.values().raw(true)` for others (works only with [**AMD's IBS Op PMU**](#ibs-op-pmu)) | `sample_record.instruction_execution().type()`                         | `perf::InstructionExecution::InstructionType`          |
| **Logical Instruction Pointer**  | Logical pointer of the sampled instruction.                                                                             | `sampler.values().instruction_pointer(true)`                                                                                                            | `sample_record.instruction_execution().logical_instruction_pointer()`  | `std::uintptr_t`                                       |
| **Physical Instruction Pointer** | Physical pointer of the sampled instruction.                                                                            | `sampler.values().raw(true)` (works only with [**AMD's IBS Fetch PMU**](#ibs-fetch-pmu))                                                                | `sample_record.instruction_execution().physical_instruction_pointer()` | `std::uintptr_t`                                       |
| **Is Instruction Pointer Exact** | Indicates if the sampled instruction pointer is exact, i.e., the sampled information belong to the sampled instruction. | Always recorded with instruction pointer.                                                                                                               | `sample_record.instruction_execution().is_instruction_pointer_exact()` | `bool`                                                 |
| **Is Locked**                    | Indicates if the sampled instruction was a locked operation.                                                            | `sampler.values().data_src(true)`                                                                                                                       | `sample_record.instruction_execution().is_locked()`                    | `bool`                                                 |
| **Branch Type**                  | Information about the branch, if the sampled instruction is a branch (`Taken`, `Retired`, `Mispredicted`, `Fuse`).      | `sampler.values().raw(true)` (works only with [**AMD's IBS Op PMU**](#ibs-op-pmu))                                                                      | `sample_record.instruction_execution().branch_type()`                  | `perf::InstructionExecution::BranchType`               |
| **Callchain**                    | Callchain of the sampled instruction.                                                                                   | `sampler.values().callchain(true)` (you can also use an `std::uint32_t` to dictate the maximum callchain)                                               | `sample_record.instruction_execution().callchain()`                    | `std::vector<std::uintptr_t>`                          |
| **Code Page Size**               | Page size of the instruction pointer.                                                                                   | `sampler.values().data_page_size(true)`                                                                                                                 | `sample_record.instruction_execution().data_page_size()`               | `std::uintptr_t`                                       |
| **Latency**                      | Latency information of the execution and the instruction fetch.                                                         | [See details below](#instruction-latency)                                                                                                               | `sample_record.instruction_execution().latency()`                      | `perf::InstructionExecution::Latency`                  |
| **TLB**                          | TLB information of the execution.                                                                                       | [See details below](#instruction-tlb)                                                                                                                   | `sample_record.instruction_execution().tlb()`                          | `perf::InstructionExecution::TLB`                      |
| **Fetch**                        | Information about the instruction fetch.                                                                                | [See details below](#instruction-fetch)                                                                                                                 | `sample_record.instruction_execution().fetch()`                        | `perf::InstructionExecution::Fetch`                    |
| **Hardware Transaction Abort**   | Information about hardware transactional memory aborts.                                                                 | [See details below](#hardware-transaction-abort)                                                                                                        | `sample_record.instruction_execution().hardware_transaction_abort()`   | `perf::InstructionExecution::HardwareTransactionAbort` |

#### Instruction Latency
Latency information regarding the execution of an instruction (or micro-op on AMD).

| Name                             | Description                                                                                     | How to record?                                                                           | How to access?                                                                   | Type            |
|----------------------------------|-------------------------------------------------------------------------------------------------|------------------------------------------------------------------------------------------|----------------------------------------------------------------------------------|-----------------|
| **Instruction Retirement**       | Latency for executing the entire instruction (including TLB access, cache/memory access, etc.). | `sampler.values().latency(true)` (works only with **Intel PEBS**)                        | `sample_record.instruction_execution().latency().instruction_retirement()`       | `std::uint32_t` |
| **uOp Tag-to-Retirement**        | Cycles of the tagged uOp from tagging to retirement.                                            | `sampler.values().latency(true)` (works only with [**AMD's IBS Op PMU**](#ibs-op-pmu))   | `sample_record.instruction_execution().latency().uop_tag_to_retirement()`        | `std::uint32_t` |
| **uOp Completion-to-Retirement** | Cycles of the tagged uOp from completion to retirement.                                         | `sampler.values().raw(true)` (works only with [**AMD's IBS Op PMU**](#ibs-op-pmu))       | `sample_record.instruction_execution().latency().uop_completion_to_retirement()` | `std::uint32_t` |
| **uOp Tag-to-Completion**        | Cycles of the tagged uOp from tagging to completion.                                            | `sampler.values().raw(true)` (works only with [**AMD's IBS Op PMU**](#ibs-op-pmu))       | `sample_record.instruction_execution().latency().uop_tag_to_completion()`        | `std::uint32_t` |
| **Fetch**                        | Instruction fetch latency from initiating the fetch to delivering to the core.                  | `sampler.values().raw(true)` (works only with [**AMD's IBS Fetch PMU**](#ibs-fetch-pmu)) | `sample_record.instruction_execution().latency().fetch()`                        | `std::uint32_t` |

#### Instruction TLB
TLB information of the micro-op fetch.
This is only available on [**AMD's IBS Fetch PMU**](#ibs-fetch-pmu).

| Name              | Description                                         | How to record?                | How to access?                                                   | Type            |
|-------------------|-----------------------------------------------------|-------------------------------|------------------------------------------------------------------|-----------------|
| **L1 Cache Miss** | Indicates if the instruction fetch missed the iTLB. | `sampler.values().raw(true)`  | `sample_record.instruction_execution().tlb().is_l1_cache_miss()` | `bool`          |
| **L2 Cache Miss** | Indicates if the instruction fetch missed the STLB. | `sampler.values().raw(true)`  | `sample_record.instruction_execution().tlb().is_l2_cache_miss()` | `bool`          |


#### Instruction Fetch
Information regarding micro-op fetch.
This is only available on [**AMD's IBS Fetch PMU**](#ibs-fetch-pmu).

| Name                  | Description                                              | How to record?               | How to access?                                                     | Type            |
|-----------------------|----------------------------------------------------------|------------------------------|--------------------------------------------------------------------|-----------------|
| **L1 Cache Miss**     | Indicates if the instruction fetch missed the L1i cache. | `sampler.values().raw(true)` | `sample_record.instruction_execution().fetch().is_l1_cache_miss()` | `bool`          |
| **L2 Cache Miss**     | Indicates if the instruction fetch missed the L2 cache.  | `sampler.values().raw(true)` | `sample_record.instruction_execution().fetch().is_l2_cache_miss()` | `bool`          |
| **L3 Cache Miss**     | Indicates if the instruction fetch missed the L3 cache.  | `sampler.values().raw(true)` | `sample_record.instruction_execution().fetch().is_l3_cache_miss()` | `bool`          |
| **Is Fetch Complete** | Indicates if the instruction fetch is complete.          | `sampler.values().raw(true)` | `sample_record.instruction_execution().fetch().is_complete()`      | `bool`          |
| **Is Fetch Valid**    | Indicates if the instruction fetch is valid.             | `sampler.values().raw(true)` | `sample_record.instruction_execution().fetch().is_valid()`         | `bool`          |

#### Hardware Transaction Abort
Information regarding aborts of hardware-transactional memory instructions.
This is only available on **Intel PEBS**.

| Name                                  | Description                                                    | How to record?                             | How to access?                                                                                                 | Type            |
|---------------------------------------|----------------------------------------------------------------|--------------------------------------------|----------------------------------------------------------------------------------------------------------------|-----------------|
| **Is Elision Transaction**            | Indicates if the abort comes from an elision type transaction. | `sampler.values().transaction_abort(true)` | `sample_record.instruction_execution().hardware_transaction_abort().is_elision_transaction()`                  | `bool`          |
| **Is Generic Transaction**            | Indicates if the abort comes from a generic transaction.       | `sampler.values().transaction_abort(true)` | `sample_record.instruction_execution().hardware_transaction_abort().is_generic_transaction()`                  | `bool`          |
| **Is Synchronous Transaction**        | Indicates if the abort comes from a synchronous transaction.   | `sampler.values().transaction_abort(true)` | `sample_record.instruction_execution().hardware_transaction_abort().is_synchronous_abort()`                    | `bool`          |
| **Is Retryable**                      | Indicates if the aborted transaction is retryable.             | `sampler.values().transaction_abort(true)` | `sample_record.instruction_execution().hardware_transaction_abort().is_retryable()`                            | `bool`          |
| **Is Due to Memory Conflict**         | Indicates if the abort is due to a memory conflict.            | `sampler.values().transaction_abort(true)` | `sample_record.instruction_execution().hardware_transaction_abort().is_abort_due_to_memory_conflict()`         | `bool`          |
| **Is Due to Write Capacity Conflict** | Indicates if the abort is due to a write capacity conflict.    | `sampler.values().transaction_abort(true)` | `sample_record.instruction_execution().hardware_transaction_abort().is_abort_due_to_write_capacity_conflict()` | `bool`          |
| **Is Due to Read Capacity Conflict**  | Indicates if the abort is due to a read capacity conflict.     | `sampler.values().transaction_abort(true)` | `sample_record.instruction_execution().hardware_transaction_abort().is_abort_due_to_read_capacity_conflict()`  | `bool`          |
| **User Specified Code**               | The user-specific code provided for the abort (if any).        | `sampler.values().transaction_abort(true)` | `sample_record.instruction_execution().hardware_transaction_abort().user_specified_code()`                     | `std::uint32_t` |

### Data Access

### Counter Values

### Branch Stack

### User Stack

### User Registers

### Kernel Registers

---

### Context Switches
Occurrence of context switches.
Sampling context switches requires a Linux Kernel version of `4.3` or higher.
* Request by `sampler.values().context_switch(true);`
* Read from the results by `sample_record.context_switch().value();` (if `sample_record.context_switch().has_value();`), which returns a `perf::ContextSwitch` object. The context switch contains
  * a flag if the process was switched in or out (`context_switch.is_in()` or `context_switch.is_out()`),
  * a flag of the process was preempted (`context_switch.is_preempt()`) (**only** from Linux Kernel `4.17`),
  * the id of the in or out process, if sampling cpu-wide (`context_switch.process_id()`),
  * and the id of the in or out thread, if sampling cpu-wide (`context_switch.thread_id()`).
  * In addition, the following data will be set in a sample:
    * `sample_record.process_id()` and `sample_record.thread_id()`, if `sampler.thread_id(true)` was specified,
    * `sample_record.timestamp()`, if `sampler.time(true)` was specified,
    * `sample_record.stream_id()`, if `sampler.stream_id(true)` was specified,
    * `sample_record.cpu_id()`, if `sampler.cpu_id(true)` was specified, and
    * `sample_record.id()`, if `sampler.identifier(true)` was specified.

&rarr; [See code example](../examples/context_switch_sampling.cpp)

### CGroup
Sampling cgroups requires a Linux Kernel version of `5.7` or higher.

* Request by `sampler.values().cgroup(true);`
* CGroup IDs are included into samples and can be read by `sample_record.cgroup_id().value();` 
* Whenever new cgroups are created or activated, the sample can include a `perf::CGroup` item, containing the ID of the created/activated cgroup (`sample_record.cgroup().value().id();`), which matches one of the `cgroup_id()`s of the sample. `perf::CGroup` also contains a path, which can be accessed by `sample_record.cgroup().value().path();`.
* In addition, the following data will be set in a sample:
  * `sample_record.process_id()` and `sample_record.thread_id()`, if `sampler.thread_id(true)` was specified,
  * `sample_record.timestamp()`, if `sampler.time(true)` was specified,
  * `sample_record.stream_id()`, if `sampler.stream_id(true)` was specified,
  * `sample_record.cpu_id()`, if `sampler.cpu_id(true)` was specified, and
  * `sample_record.id()`, if `sampler.identifier(true)` was specified.

### Throttle and Unthrottle Events
* Request by `sampler.values().throttle(true);`
* Throttle events are included into samples and can be read by `sample_record.throttle().value();`, which returns an optional `perf::Throttle` object. The throttle object contains a flag indicating
  * that it was a throttle event (`sample_record.throttle().value().is_throttle();`)
  * or it was an unthrottle event (`sample_record.throttle().value().is_unthrottle();`). Only one of both will return `true`.
* In addition, the following data will be set in a sample:
  * `sample_record.process_id()` and `sample_record.thread_id()`, if `sampler.thread_id(true)` was specified,
  * `sample_record.timestamp()`, if `sampler.time(true)` was specified,
  * `sample_record.stream_id()`, if `sampler.stream_id(true)` was specified,
  * `sample_record.cpu_id()`, if `sampler.cpu_id(true)` was specified, and
  * `sample_record.id()`, if `sampler.identifier(true)` was specified.

## Sample mode
Each sample is recorded in one of the following modes:
* `perf::Sample::Mode::Unknown`
* `perf::Sample::Mode::Kernel`
* `perf::Sample::Mode::User`
* `perf::Sample::Mode::Hypervisor`
* `perf::Sample::Mode::GuestKernel`
* `perf::Sample::Mode::GuestUser`

You can check the mode via `sample_record.mode()`, for example:
```cpp
for (const auto& sample_record : result)
{
  if (sample_record.mode() == perf::Sample::Mode::Kernel)
  {
    std::cout << "Sample in Kernel" << std::endl;      
  }
  else if (sample_record.mode() == perf::Sample::Mode::User)
  {
    std::cout << "Sample in User" << std::endl;      
  }
}
```

## Lost Samples
Sample records may be lost, for example, if the buffer is full or the CPU is heavily loaded. 
Such losses are documented and reported through `sample_record.count_loss()`, which returns `std::nullopt` for regular samples and an integer for the number of samples lost, as reported by the perf subsystem.

In addition, the following data will be set in a sample:
* `sample_record.process_id()` and `sample_record.thread_id()`, if `sampler.thread_id(true)` was specified,
* `sample_record.timestamp()`, if `sampler.time(true)` was specified,
* `sample_record.stream_id()`, if `sampler.stream_id(true)` was specified,
* `sample_record.cpu_id()`, if `sampler.cpu_id(true)` was specified, and
* `sample_record.id()`, if `sampler.identifier(true)` was specified.

## Specific Notes for different CPU Vendors
### Intel (PEBS)
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