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
  - [1) Define What is Recorded and When](#1-define-what-is-recorded-and-when)
  - [2) Open the Sampler *(optional)*](#2-open-the-sampler-optional)
  - [3) Wrap `start()` and `stop()` around the Processing Code](#3-wrap-start-and-stop-around-the-processing-code)
  - [4) Access the Recorded Samples](#4-access-the-recorded-samples)
  - [5) Closing the Sampler](#5-closing-the-sampler)
- [Trigger](#trigger)
- [Precision](#precision)
- [Period / Frequency](#period--frequency)
- [What can be Recorded and how to Access the Data?](#what-can-be-recorded-and-how-to-access-the-data)
  - [Time](#time)
  - [Stream ID](#stream-id)
  - [Period](#period)
  - [Identifier](#identifier)
  - [Instruction Pointer](#instruction-pointer)
  - [Callchain](#callchain)
  - [Registers in user-level](#registers-in-user-level)
  - [Registers in kernel-level](#registers-in-kernel-level)
  - [ID of the recording Thread](#id-of-the-recording-thread)
  - [ID of the recording CPU](#id-of-the-recording-cpu)
  - [Size of the Code Page](#size-of-the-code-page)
  - [Performance Counter Values](#performance-counter-values)
  - [Branch Stack (LBR)](#branch-stack-lbr)
  - [Physical Memory Address](#physical-memory-address)
  - [Logical Memory Address](#logical-memory-address)
  - [Memory Access Latency](#memory-access-latency)
  - [Data Source of a Memory Load](#data-source-of-a-memory-load)
  - [Size of the Data Page](#size-of-the-data-page)
  - [Transaction Abort](#transaction-abort)
  - [Raw Values](#raw-values)
  - [Context Switches](#context-switches)
  - [CGroup](#cgroup)
  - [Throttle and Unthrottle Events](#throttle-and-unthrottle-events)
- [Sample mode](#sample-mode)
- [Lost Samples](#lost-samples)
- [Specific Notes for different CPU Vendors](#specific-notes-for-different-cpu-vendors)
  - [Intel (PEBS)](#intel-pebs)
  - [AMD (Instruction Based Sampling)](#amd-instruction-based-sampling)
- [Debugging Counter Settings](#debugging-counter-settings)
---

## Interface
### 1) Define What is Recorded and When
```cpp
#include <perfcpp/sampler.h>
/// The perf::CounterDefinition object holds all counter names and must be alive when counters are accessed.
auto counter_definitions = perf::CounterDefinition{};

/// Define when the data is recorded.
/// This can be every Nth event (e.g., every 1000th cycle):
auto sample_config = perf::SampleConfig{};
sample_config.period(1000U);
/// or with a frequency of N (e.g., every ms with a clock-frequency of 2GHz):
sample_config.frequency(2000000U);
/// Note that you can use only frequency XOR period

/// Create the sampler.
auto sampler = perf::Sampler{ counter_definitions, sample_config };

/// Define the trigger (cycles in this example)...
sampler.trigger("cycles");

// and what to record (time and instruction pointer in this example)
sampler.values().time(true).instruction_pointer(true);
```

### 2) Open the Sampler *(optional)* 
The sampler will be opened by `sampler.start()`, if it is not already opened.
Opening the sampler means setting up all the counters and buffers, which can take some time.
If you need precise time measurements and want to exclude the counter setup, you can call open individually.

```cpp
try {
    sampler.open();
} catch (std::runtime_error& e) {
    std::cerr << e.what() << std::endl;
}
```

### 3) Wrap `start()` and `stop()` around the Processing Code
```cpp
try {
    sampler.start();
} catch (std::runtime_error& e) {
    std::cerr << e.what() << std::endl;
}

/// ... do some computational work here...

sampler.stop();
```

### 4) Access the Recorded Samples
The output consists of a list of `perf::Sample` instances, where each sample may contain comprehensive data. 
As you have the flexibility to specify which data elements to sample, each piece of data is encapsulated within an `std::optional` to handle its potential absence.
```cpp
const auto result = sampler.result();

for (const auto& sample_record : result)
{
    if (sample_record.time().has_value() && sample_record.instruction_pointer().has_value())
    {
        std::cout 
            << "Time = " << sample_record.time().value() 
            << " | IP = 0x" << std::hex << sample_record.instruction_pointer().value() << std::dec << std::endl;
    }
}
```

The output may be something like this:

    Time = 124853764466887 | IP = 0x5794c991990c
    Time = 124853764663977 | IP = 0xffffffff8d79d48b
    Time = 124853764861377 | IP = 0x5794c991990c
    Time = 124853765058918 | IP = 0x5794c991990c
    Time = 124853765256328 | IP = 0x5794c991990c

### 5) Closing the Sampler
Closing the sampler will free and un-map all buffers.
```cpp
sampler.close();
```

---

## Trigger
Each sampler has one or more **trigger** events.
If the trigger event reaches the count specified in `SampleConfig::period`, the CPU will write a sample containing the requested data.
The trigger(s) of a sampler will be defined by, for example,

```cpp
sampler.trigger("cycles");
```
.
Multiple triggers can be defined using a vector of trigger names:

```cpp
sampler.trigger(std::vector<std::string>{"cycles", "instructions"});
```
In that case, both, an overflow of the cycles and of the instructions counter will trigger the CPU to write a sample.

## Precision
Due to deeply pipelined processors, samples might not be precise, i.e., a sample might contain an instruction pointer or memory address that did not generate the overflow (&rarr; see [a blogpost on easyperf.net](https://easyperf.net/blog/2019/04/03/Precise-timing-of-machine-code-with-Linux-perf) and [the perf documentation](https://man7.org/linux/man-pages/man2/perf_event_open.2.html)).
You can request a specific amount if skid through for each trigger, for example,

```cpp
sampler.trigger("cycles", perf::Precision::AllowArbitrarySkid);
```

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
sample_config.precise_ip(perf::Precision::RequestZeroSkid);

auto sampler = perf::Sampler{ counter_definitions, sample_config };
sampler.trigger("cycles");
```

## Period / Frequency
You can request a specific period **or** frequency for each trigger – basically how often the hardware should write samples –, for example,

```cpp
/// Every 4000th cycle.
sampler.trigger("cycles", perf::Period{4000U});
```

**or**

```cpp
/// With a frequency of 1000 samples per second 
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
/// or:
sample_config.frequency(1000U);

auto sampler = perf::Sampler{ counter_definitions, sample_config };
sampler.trigger("cycles");
```

## What can be Recorded and how to Access the Data?
Before starting, the sampler need to be instructed what data should be recorded, for example:
```cpp
sampler.values()
    .time(true)
    .instruction_pointer(true);
```

This includes the **timestamp** and **instruction pointer** into the sample record.
After sampling and retrieving the results, the recorded fields can be accessed by
```cpp
for (const auto& sample_record : sampler.results()) {
    const auto timestamp = sample_record.time().value();
    const auto instruction_pointer = sample_record.instruction_pointer().value();
}
```

### Time
* Request by `sampler.values().time(true);`
* Read from the results by `sample_record.time().value()`

&rarr; [See code example](../examples/instruction_pointer_sampling.cpp)

### Stream ID
Unique ID of an opened event.
* Request by `sampler.values().stream_id(true);`
* Read from the results by `sample_record.stream_id().value()`

### Period
* Request by `sampler.values().period(true);`
* Read from the results by `sample_record.period().value();`

### Identifier
* Request by `sampler.values().identifier(true);`
* Read from the results by `sample_record.id().value();`

### Instruction Pointer
* Request by `sampler.values().instruction_pointer(true);`
* Read from the results by `sample_record.instruction_pointer().value()`

You may need to adjust the `sample_config.precise_ip(X)` setting on different hardware (ranging from `0` to `3`). 

&rarr; [See code example](../examples/instruction_pointer_sampling.cpp)

### Callchain
Callchain as a list of instruction pointers.

* Request by `sampler.values().callchain(true);` or `sampler.values().callchain(M);` where `M` is a `std::uint16_t` defining the maximum call stack size.
* Read from the results by `sample_record.callchain().value();`, which returns a `std::vector<std::uintptr_t>` of instruction pointers.

### Registers in user-level
Values of user registers (the values in the process before the kernel was called).

* Request by `sampler.values().user_registers( { perf::Registers::x86::IP, perf::Registers::x86::DI, perf::Registers::x86::R10 });`
* Read from the results by `sample_record.user_registers().value()[0];` (`0` for the first register, `perf::Registers::x86::IP` in this example)

The ABI can be queried using `sample_record.user_registers_abi()`.

&rarr; [See code example](../examples/register_sampling.cpp)

### Registers in kernel-level
Values of user registers (the values in the process when the kernel was called).

* Request by `sampler.values().kernel_registers( { perf::Registers::x86::IP, perf::Registers::x86::DI, perf::Registers::x86::R10 });`
* Read from the results by `sample_record.kernel_registers().value()[0];` (`0` for the first register, `perf::Registers::x86::IP` in this example)

The ABI can be queried using `sample_record.kernel_registers_abi()`.

&rarr; [See example](../examples/register_sampling.cpp)

### ID of the recording Thread
* Request by `sampler.values().thread_id(true);`
* Read from the results by `sample_record.thread_id().value()`

### ID of the recording CPU

* Request by `sampler.values().cpu_id(true);`
* Read from the results by `sample_record.cpu_id().value();`

&rarr; [See code example](../examples/instruction_pointer_sampling.cpp)

### Size of the Code Page
Size of pages of sampled instruction pointers (e.g., when sampling for instruction pointers).
Sampling the code page size requires a Linux Kernel version of `5.11` or higher.

* Request by `sampler.values().code_page_size(true);`
* Read from the results by `sample_record.code_page_size().value();`

### Performance Counter Values
Record hardware performance counter values at the time of the record.

* Request by `sampler.values().counter({"instructions", "cache-misses"});`
* Read from the results by `sample_record.counter_result().value().get("cache-misses");`. This can be accessed in the same manner as when recording counters.

&rarr; [See code example](../examples/counter_sampling.cpp)

### Branch Stack (LBR)
Branch history with jump addresses and flag if predicted correctly.

#### Request recording
```cpp
sampler.values().branch_stack({ perf::BranchType::User, perf::BranchType::Conditional });
```
The possible branch type values are:
* `perf::BranchType::User`: Sample branches in user mode.
* `perf::BranchType::Kernel`: Sample kernel branches.
* `perf::BranchType::HyperVisor`: Sample branches in HV mode.
* `perf::BranchType::Any` (the default): Sample all branches.
* `perf::BranchType::Call`: Sample any call (direct, indirect, far jumps).
* `perf::BranchType::DirectCall`: Sample direct call (requires Linux Kernel `4.4` or higher).
* `perf::BranchType::IndirectCall`: Sample indirect call.
* `perf::BranchType::Return`: Sample return branches.
* `perf::BranchType::IndirectJump`: Sample indirect jumps (requires Linux Kernel `4.2` or higher).
* `perf::BranchType::Conditional`: Sample conditional branches.
* `perf::BranchType::TransactionalMemoryAbort`: Sample branches that abort transactional memory.
* `perf::BranchType::InTransaction`: Sample branches in transactions of transactional memory.
* `perf::BranchType::NotInTransaction`: Sample branches not in transactions of transactional memory.

#### Read from the results
Read from the results by `sample_record.branches()`, which returns a vector of `perf::Branch`.

Each `perf::Branch` instance has the following values:
* Instruction pointer from where the branch started (`branch.instruction_pointer_from()`).
* Instruction pointer from where the branch ended (`branch.instruction_pointer_to()`).
* A flag that indicates if the branch was correctly predicted (`branch.is_predicted()`) (`0` if not supported by the hardware).
* A flag that indicates if the branch was mispredicted (`branch.is_mispredicted()`) (`0` if not supported by the hardware).
* A flag that indicates if the branch was within a transaction (`branch.is_in_transaction()`).
* A flag that indicates if the branch was a transaction abort (`branch.is_transaction_abort()`).
* Cycles since the last branch (`branch.cycles()`) (`0` if not supported by the hardware).

&rarr; [See code example](../examples/branch_sampling.cpp)

### Physical Memory Address
Sampling the physical memory address requires a Linux Kernel version of `4.13` or higher.

* Request by `sampler.values().physical_memory_address(true);`
* Read from the results by `sample_record.physical_memory_address().value();`

### Logical Memory Address
* Request by `sampler.values().logical_memory_address(true);`
* Read from the results by `sample_record.logical_memory_address().value()`

**Note**: Recording memory addresses (logical and physical) requires an appropriate trigger. 
On **Intel**, `perf list` reports these triggers as "*Supports address when precise*".
On **AMD**, memory sampling is implemented through Instruction Based sampling (IBS) ([see kernel mailing list](https://lore.kernel.org/all/20220616113638.900-2-ravi.bangoria@amd.com/T/)).
*perf-cpp* will add IBS-related counter at runtime if the underlying (AMD) system supports IBS (&rarr; see [documentation about AMD IBS](#amd-instruction-based-sampling)).

In addition, you may need to adjust the `sample_config.precise_ip(X)` setting on different hardware (ranging from `perf::Precision::AllowArbitrarySkid` to `perf::Precision::MustHaveZeroSkid`).

&rarr; [See code example](../examples/address_sampling.cpp)

### Memory Access Latency
The weight (and weight struct) indicates how costly the event was (basically the latency).
Since Linux Kernel version `5.12`, the Kernel might generate more information than only a single value, which is used to differentiate between **memory-** (from cache towards memory) and **instruction latency**.

* Request by `sampler.values().weight(true);` or `sampler.values().weight_struct(true);` (**the latter only from Kernel `5.12`**)
* Read from the results by `sample_record.weight().value();`, which returns a `perf::Weight` class, which has the following attributes:
  * `sample_record.weight().value().cache_latency()` returns the cache latency of the sampled data address (for both `sampler.values().weight(true)` and `sampler.values().weight_struct(true)`).
  * `sample_record.weight().value().instruction_retirement_latency()` returns the latency of retiring the instruction (including the cache access) **but** only for `sampler.values().weight_struct(true)`. To the best of our knowledge, this feature is only supported by new Intel generations.
  * `sample_record.weight().value().var3()` returns "other information" (not specified by perf) **but** only for `sampler.values().weight_struct(true)`.

&rarr; [See code example](../examples/address_sampling.cpp)

**Note** that memory sampling depends on the underlying sampling mechanism. &rarr; [See hardware-specific information (e.g., Intel PEBS vs AMD IBS)](#specific-notes-for-different-cpu-vendors)

### Data Source of a Memory Load
Data source where the data was sampled (e.g., local mem, remote mem, L1d, L2, ...).

* Request by `sampler.values().data_src(true);`
* Read from the results by `sample_record.data_src().value();` which returns a specific `perf::DataSource` object

The `perf::DataSource` object can be queried for the following information:

| Query                                               | Information                                                                            |
|-----------------------------------------------------|----------------------------------------------------------------------------------------|
| `sample_record.data_source().value().is_load()`            | `True`, if the access was a load operation.                                            |
| `sample_record.data_source().value().is_store()`           | `True`, if the access was a store operation.                                           |
| `sample_record.data_source().value().is_prefetch()`        | `True`, if the access was a prefetch operation.                                        |
| `sample_record.data_source().value().is_exec()`            | `True`, if the access was an execute operation.                                        |
| `sample_record.data_source().value().is_mem_hit()`         | `True`, if the access was a hit.                                                       |
| `sample_record.data_source().value().is_mem_miss()`        | `True`, if the access was a miss.                                                      |
| `sample_record.data_source().value().is_mem_hit()`         | `True`, if the access was a hit.                                                       |
| `sample_record.data_source().value().is_mem_l1()`          | `True`, if the data was found in the L1 cache.                                         |
| `sample_record.data_source().value().is_mem_l2()`          | `True`, if the data was found in the L2 cache.                                         |
| `sample_record.data_source().value().is_mem_l3()`          | `True`, if the data was found in the L3 cache.                                         |
| `sample_record.data_source().value().is_mem_l4()`          | `True`, if the data was found in the L4 cache.                                         |
| `sample_record.data_source().value().is_mem_lfb()`         | `True`, if the data was found in the Line Fill Buffer (or Miss Address Buffer on AMD). |
| `sample_record.data_source().value().is_mem_ram()`         | `True`, if the data was found in any RAM.                                              |
| `sample_record.data_source().value().is_mem_local_ram()`   | `True`, if the data was found in the local RAM.                                        |
| `sample_record.data_source().value().is_mem_remote_ram()`  | `True`, if the data was found in any remote RAM.                                       |
| `sample_record.data_source().value().is_mem_hops0()`       | `True`, if the data was found locally.                                                 |
| `sample_record.data_source().value().is_mem_hops1()`       | `True`, if the data was found on the same node.                                        |
| `sample_record.data_source().value().is_mem_hops2()`       | `True`, if the data was found on a remote socket.                                      |
| `sample_record.data_source().value().is_mem_hops3()`       | `True`, if the data was found on a remote board.                                       |
| `sample_record.data_source().value().is_mem_remote_ram1()` | `True`, if the data was found in a remote RAM on the same node.                        |
| `sample_record.data_source().value().is_mem_remote_ram2()` | `True`, if the data was found in a remote RAM on a different socket.                   |
| `sample_record.data_source().value().is_mem_remote_ram3()` | `True`, if the data was found in a remote RAM on a different board.                    |
| `sample_record.data_source().value().is_mem_remote_cce1()` | `True`, if the data was found in cache with one hop distance.                          |
| `sample_record.data_source().value().is_mem_remote_cce2()` | `True`, if the data was found in cache with two hops distance.                         |
| `sample_record.data_source().value().is_pmem()`            | `True`, if the data was found on a PMEM device.                                        |
| `sample_record.data_source().value().is_cxl()`             | `True`, if the data was transferred via Compute Express Link.                          |
| `sample_record.data_source().value().is_tlb_hit()`         | `True`, if the access was a TLB hit.                                                   |
| `sample_record.data_source().value().is_tlb_miss()`        | `True`, if the access was a TLB miss.                                                  |
| `sample_record.data_source().value().is_tlb_l1()`          | `True`, if the access can be associated with the dTLB.                                 |
| `sample_record.data_source().value().is_tlb_l2()`          | `True`, if the access can be associated with the STLB.                                 |
| `sample_record.data_source().value().is_tlb_walk()`        | `True`, if the access can be associated with the hardware walker.                      |
| `sample_record.data_source().value().is_locked()`          | `True`, If the address was accessed via lock instruction.                              |
| `sample_record.data_source().value().is_data_blocked()`    | `True` in case the data could not be forwarded.                                        |
| `sample_record.data_source().value().is_address_blocked()` | `True` in case of an address conflict.                                                 |
| `sample_record.data_source().value().is_snoop_hit()`       | `True`, if access was a snoop hit.                                                     |
| `sample_record.data_source().value().is_snoop_miss()`      | `True`, if access was a snoop miss.                                                    |
| `sample_record.data_source().value().is_snoop_hit_modified()`          | `True`, if access was a snoop hit modified.                                            |

All these queries wrap around the `perf_mem_data_src` data structure.
Since we may have missed specific operations, you can also access each particular data structure:
* `sample_record.data_source().value().op()` accesses the `PERF_MEM_OP` structure.
* `sample_record.data_source().value().lvl()` accesses the `PERF_MEM_LVL` structure. Note that this structure is deprecated, use `sample_record.data_source().value().lvl_num()` instead.
* `sample_record.data_source().value().lvl_num()` accesses the `PERF_MEM_LVL_NUM` structure.
* `sample_record.data_source().value().remote()` accesses the `PERF_MEM_REMOTE` structure.
* `sample_record.data_source().value().snoop()` accesses the `PERF_MEM_SNOOP` structure.
* `sample_record.data_source().value().snoopx()` accesses the `PERF_MEM_SNOOPX` structure.
* `sample_record.data_source().value().lock()` accesses the `PERF_MEM_LOCK` structure.
* `sample_record.data_source().value().blk()` accesses the `PERF_MEM_BLK` structure.
* `sample_record.data_source().value().tlb()` accesses the `PERF_MEM_TLB` structure.
* `sample_record.data_source().value().hops()` accesses the `PERF_MEM_HOPS` structure.

&rarr; [See code example](../examples/address_sampling.cpp)

**Note**: Recording memory addresses (logical and physical) requires an appropriate trigger.
On Intel, `perf list` reports these triggers as "*Supports address when precise*".
On AMD, you need the `ibs_op` counter (&rarr;[see kernel mailing list](https://lore.kernel.org/all/20220616113638.900-2-ravi.bangoria@amd.com/T/)).

You may need to adjust the `sample_config.precise_ip(X)` setting on different hardware (ranging from `0` to `3`).

### Size of the Data Page
Size of pages of sampled data addresses (e.g., when sampling for logical memory address).
Sampling the data page size requires a Linux Kernel version of `5.11` or higher.

* Request by `sampler.values().data_page_size(true);`
* Read from the results by `sample_record.data_page_size().value();`

### Transaction Abort
Reports the reason for an abort of a transactional memory transaction.

* Request by `sampler.values().transaction_abort(true);`
* Read from the results by `sample_record.transaction_abort().value();`, which returns an instance of the type `perf::TransactionAbort`. The abort can be queried by:
  * `sample_record.transaction_abort().value().is_elision()`, which returns `true`, if the abort comes from an elision type transaction (Intel-specific).
  * `sample_record.transaction_abort().value().is_transaction()`, which returns `true`, if the abort comes a generic transaction.
  * `sample_record.transaction_abort().value().is_synchronous()`, which returns `true`, if the abort is synchronous.
  * `sample_record.transaction_abort().value().is_asynchronous()`, which returns `true`, if the abort is asynchronous.
  * `sample_record.transaction_abort().value().is_retry()`, which returns `true`, if the abort is retryable.
  * `sample_record.transaction_abort().value().is_conflict()`, which returns `true`, if the abort is due to a conflict.
  * `sample_record.transaction_abort().value().is_capacity_write()`, which returns `true`, if the abort is due to write capacity.
  * `sample_record.transaction_abort().value().is_capacity_read()`, which returns `true`, if the abort is due to read capacity.
  * In addition, `sample_record.transaction_abort().value().code()` returns the abort-code for a transaction as specified by the user.

### Raw Values
Hardware will record different data, depending on the CPU generation and manufacture.
While the perf subsystem will parse the data and map it to a generalized interface, the raw data can also be accessed using raw sampling.

* Request by `sampler.values().raw(true);`
* Read the results by `sample_record.raw().value();`

&rarr; [See code example for AMD IBS](../examples/amd_ibs_raw_sampling.cpp) (more information about how to access the raw data is provided by the [AMD manual](https://www.amd.com/content/dam/amd/en/documents/processor-tech-docs/programmer-references/24593.pdf) from page 428)

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
* Unknown
* Kernel
* User
* Hypervisor
* GuestKernel
* GuestUser

You can check the mode via `Sample::mode()`, for example:
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
Sample records can be lost, e.g., if the buffer is out of capacity or the CPU is too busy.
Lost samples are recorded and are reported as such through `sample_record.count_loss()`, which holds an `std::nullopt` if the sample was a regular sample and an integer, if the perf subsystem reported losses.

In addition, the following data will be set in a sample:
* `sample_record.process_id()` and `sample_record.thread_id()`, if `sampler.thread_id(true)` was specified,
* `sample_record.timestamp()`, if `sampler.time(true)` was specified,
* `sample_record.stream_id()`, if `sampler.stream_id(true)` was specified,
* `sample_record.cpu_id()`, if `sampler.cpu_id(true)` was specified, and
* `sample_record.id()`, if `sampler.identifier(true)` was specified.

## Specific Notes for different CPU Vendors
### Intel (PEBS)
Sampling might work without problems since _Cascade Lake_, however, _Sapphire Rapids_  is much more exact (e.g., about the latency).

#### Sapphire Rapids
To use weight-sampling on Intel's Sapphire Rapids architecture, perf needs an auxiliary counter to be added to the group, before the first "real" counter is added (see [this commit](https://lore.kernel.org/lkml/1612296553-21962-3-git-send-email-kan.liang@linux.intel.com/)).
*perf-cpp*  defines this counter, you only need to add it accordingly.
For example, to record loads (counter `0x1CD`) and stores (counter `0x2CD`):

```cpp
sampler.trigger({
    { 
        perf::Sampler::Trigger{"mem-loads-aux", perf::Precision::MustHaveZeroSkid}, /// Helper
        perf::Sampler::Trigger{"loads", perf::Precision::RequestZeroSkid}           /// First "real" counter
    },
    { perf::Sampler::Trigger{"stores", perf::Precision::MustHaveZeroSkid} }         /// Other "real" counters.
  });
```

The sampler will detect that auxiliary counter automatically.

&rarr; [See code example](../examples/multi_event_sampling.cpp)

### AMD (Instruction Based Sampling)
AMD uses Instruction Based Sampling to tag instructions randomly for sampling and collect various information for each sample ([see the programmer reference](https://www.amd.com/content/dam/amd/en/documents/processor-tech-docs/programmer-references/24593.pdf)).
In contrast to Intel's mechanism, IBS cannot tag specific load and store instructions (and apply a filter on the latency).
In case the instruction was a load/store instruction, the sample will include data source, latency, and a memory address ([see kernel mailing list](https://lore.kernel.org/all/20220616113638.900-2-ravi.bangoria@amd.com/T/)).

*perf-cpp* –or the `perf::CounterDefinition` class to be precise– will detect IBS support on AMD devices and adds the following counters that can be used as **trigger** for sampling on AMD:
* `ibs_op` selects instructions during the execution pipeline. CPU cycles (on the specified period/frequency) will lead to tag an instruction.
* `ibs_op_uops` selects instructions during the execution pipeline, **but** the period/frequency refers to the number of executed micro-operations, **not** CPU cycles.
* `ibs_op_l3missonly` selects instructions during the execution pipeline that miss the L3 cache. CPU cycles are used as the trigger.
* `ibs_op_uops_l3missonly` selects instructions during the execution pipeline that miss the L3 cache, using micro-operations as the trigger.
* `ibs_fetch` selects instructions in the fetch-state (frontend) using cycles as the trigger.
* `ibs_fetch_l3missonly` selects instructions in the fetch-state (frontend) that miss the L3 cache, again, using cycles as a trigger.
---

## Debugging Counter Settings
In certain scenarios, configuring counters for sampling can be challenging, as settings (e.g., `precise_ip`) may need to be adjusted for different machines.
To facilitate this process, perf provides a debug output option:


    perf --debug perf-event-open [mem] record ...


This command helps visualize configurations for various counters, which is also beneficial for retrieving event codes (for more details, see the [counters documentation](counters.md)).

Similarly, *perf-cpp* includes a debug feature for sampled counters.
To examine the configuration settings—particularly useful if encountering errors during `sampler.start();`—enable debugging in your code as follows:

```cpp
sample_config.is_debug(true);
```

When `is_debug` is set to `true`, *perf-cpp* will display the configuration of all counters upon initiating sampling.
