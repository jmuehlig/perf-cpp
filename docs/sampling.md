# Event Sampling

*perf-cpp* enables the recording of event samples, capturing details like instruction pointers, multiple counters, branches, memory addresses, data sources, latency, and more. 
Essentially, you define a sampling period or frequency at which data is captured. 
At its core, this functionality is akin to traditional profiling tools, like `perf record`, but uniquely tailored to record specific blocks of code rather than the entire application.

The following data can be recorded:
* Instruction pointers (current and callchain)
* ID of thread, CPU, and sample
* Timestamp
* Logical and physical address
* Data source (e.g., cache level, memory, etc.)
* Group of counter values
* Last branch stack (including jump addresses and prediction)
* User- and kernel-level registers
* Weight of the access (which is mostly the latency)
* Data and code page sizes (when sampling for data addresses or instruction pointers)

&rarr; [See details below](#what-can-be-recorded-and-how-to-access-the-data).

## Interface
### 1) Define what is recorded and when
```cpp
#include <perfcpp/sampler.h>
auto counter_definitions = perf::CounterDefinition{};
auto event_counter = perf::EventCounter{counter_definitions};

/// Define when the data is recorded.
/// This can be every Nth event (e.g., every 1000th cycle):
auto sample_config = perf::SampleConfig{};
sample_config.period(1000U);
/// or with a frequency of N (e.g., every ms with a clock-frequency of 2GHz):
sample_config.frequency(2000000U);
/// Note that you can use only frequency XOR period

/// Define the trigger (cycles in this example) 
//      and what to record (time and instruction pointer in this example)
auto sampler = perf::Sampler{
    counter_definitions,
    "cycles",
    perf::Sampler::Type::Time |  perf::Sampler::Type::InstructionPointer,
    sample_config
};
```

### 2) Wrap `start()` and `stop()` around your processing code
```cpp
sampler.start();

/// ... do some computational work here...

sampler.stop();
```

### 3) Access the recorded samples
The output consists of a list of `perf::Sample` instances, where each sample may contain comprehensive data. 
As you have the flexibility to specify which data elements to sample, each piece of data is encapsulated within an `std::optional` to handle its potential absence.
```cpp
const auto result = sampler.result();

for (const auto& sample : result)
{
    if (sample.time().has_value() && sample.instruction_pointer().has_value())
    {
        std::cout << "Time = " << sample.time().value() << " | IP = 0x" << std::hex << sample.instruction_pointer().value() << std::dec << std::endl;
    }
}
```

The output may be something like this:

    Time = 124853764466887 | IP = 0x5794c991990c
    Time = 124853764663977 | IP = 0xffffffff8d79d48b
    Time = 124853764861377 | IP = 0x5794c991990c
    Time = 124853765058918 | IP = 0x5794c991990c
    Time = 124853765256328 | IP = 0x5794c991990c

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

---

## Sample mode
Each sample is recorded in one of the following modes:
* Unknown
* Kernel
* User
* Hypervisor
* GuestKernel
* GuestUser

You can check the mode via `Sample::mode`, for example:
```cpp
for (const auto& sample : result)
{
  if (sample.mode() == perf::Sample::Mode::Kernel)
  {
    std::cout << "Sample in Kernel" << std::endl;      
  }
  else if (sample.mode() == perf::Sample::Mode::User)
  {
    std::cout << "Sample in User" << std::endl;      
  }
}
```

## What can be recorded and how to access the data?
The most of the following options can be combined using the or operator, e.g., `perf::Sampler::Type::Time |  perf::Sampler::Type::InstructionPointer`.

### `perf::Sampler::Type::InstructionPointer`
The instruction pointer of the current instruction. 
You may need to adjust the `sample_config.precise_ip(X)` setting on different hardware (ranging from `0` to `3`). 
The sampled instruction pointer can be accessed via `sample.instruction_pointer()`.

&rarr; [See example](../examples/instruction_pointer_sampling.cpp)

### `perf::Sampler::Type::ThreadId`
The thread id. Can be accessed via `sample.thread_id()`.

### `perf::Sampler::Type::Time`
A timestamp. Can be accessed via `sample.time()`.

&rarr; [See example](../examples/instruction_pointer_sampling.cpp)

### `perf::Sampler::Type::LogicalMemAddress`
The logical (or virtual) memory address.
Can be accessed via `sample.logical_memory_address()`.
From our experience, this only works on Intel hardware (ARM might work, too) and only with specific triggers.
You may need to adjust the `sample_config.precise_ip(X)` setting on different hardware (ranging from `0` to `3`).

&rarr; [See example](../examples/address_sampling.cpp)

### `perf::Sampler::Type::CounterValues`
Multiple counter values. Additional counter values to record must be specified along with the trigger (which is the first counter in the list):
```cpp
auto sampler = perf::Sampler{
    counter_definitions,
    {"instruction", "cycles", "cache-misses"},
    ...
};
```

The results are retrievable through the `sample.counter_result()` method, returning an instance of `perf::CounterResult`. 
This can be accessed in the same manner as when recording counters. 
For example, to access the "cycles" counter, you would use `sample.counter_result().value().get("cycles")`.

&rarr; [See example](../examples/counter_sampling.cpp)

### `perf::Sampler::Type::Callchain`
Callchain as a list of instruction pointers.
Can be accessed via `sample.callchain()`, which returns a vector of `std::uintptr_t` instruction pointers.

### `perf::Sampler::Type::CPU`
CPU id, accessible via `sample.cpu_id()`.

&rarr; [See example](../examples/instruction_pointer_sampling.cpp)

### `perf::Sampler::Type::Period`
Sample period, accessible via `sample.period()`.

### `perf::Sampler::Type::BranchStack`
Branch history with jump addresses and flag if predicted correctly.
The result can be accessed through `sample.branches()`, which returns a vector of `perf::Branch`.
Each `perf::Branch` instance has the following values:
* Instruction pointer from where the branch started (`branch.instruction_pointer_from()`).
* Instruction pointer from where the branch ended (`branch.instruction_pointer_to()`).
* A flag that indicates if the branch was correctly predicted (`branch.is_predicted()`) (`0` if not supported by the hardware).
* A flag that indicates if the branch was mispredicted (`branch.is_mispredicted()`) (`0` if not supported by the hardware).
* A flag that indicates if the branch was within a transaction (`branch.is_in_transaction()`).
* A flag that indicates if the branch was a transaction abort (`branch.is_transaction_abort()`).
* Cycles since the last branch (`branch.cycles()`) (`0` if not supported by the hardware).

In addition, the type of sampled branches can be restricted by passing the type of wanted branches into `SampleConfig::branch_type`, for example:

```cpp
/// Sample only conditional branches in user mode.
auto sample_config = perf::SampleConfig{};
sample_config.branch_type(perf::BranchType::User | perf::BranchType::Conditional);
```

The possible branch type values are:
* `perf::BranchType::User`: Sample branches in user mode.
* `perf::BranchType::Kernel`: Sample kernel branches.
* `perf::BranchType::HyperVisor`: Sample branches in HV mode.
* `perf::BranchType::Any` (the default): Sample all branches.
* `perf::BranchType::Call`: Sample any call (direct, indirect, far jumps).
* `perf::BranchType::DirectCall`: Sample direct call.
* `perf::BranchType::IndirectCall`: Sample indirect call.
* `perf::BranchType::Return`: Sample return branches.
* `perf::BranchType::IndirectJump`: Sample indirect jumps.
* `perf::BranchType::Conditional`: Sample conditional branches.
* `perf::BranchType::TransactionalMemoryAbort`: Sample branches that abort transactional memory.
* `perf::BranchType::InTransaction`: Sample branches in transactions of transactional memory.
* `perf::BranchType::NotInTransaction`: Sample branches not in transactions of transactional memory.

&rarr; [See example](../examples/branch_sampling.cpp)

### `perf::Sampler::Type::UserRegisters`
Values of user registers (the values in the process before the kernel was called).
The values that should be sampled must be set before sampling (see `SampleConfig::user_registers`, e.g., `perf_config.user_registers(perf::Registers{{ perf::Registers::x86::AX, perf::Registers::x86::DI, perf::Registers::x86::R10 }});`).
The result can be accessed through `sample.user_registers()`, which returns a vector of `std::uint64_t` values.
The ABI can be queried using `sample.user_registers_abi()`.

&rarr; [See example](../examples/register_sampling.cpp)

### `perf::Sampler::Type::KernelRegisters`
Values of user registers (the values in the process when the kernel was called).
The values that should be sampled must be set before sampling (see `SampleConfig::kernel_registers`, e.g., `perf_config.kernel_registers(perf::Registers{{ perf::Registers::x86::AX, perf::Registers::x86::DI, perf::Registers::x86::R10 }});`).
The result can be accessed through `sample.kernel_registers()`, which returns a vector of `std::uint64_t` values.
The ABI can be queried using `sample.kernel_registers_abi()`.

&rarr; [See example](../examples/register_sampling.cpp)

### `perf::Sampler::Type::Weight` (Linux Kernel `< 5.12`) / `perf::Sampler::Type::WeightStruct` (since Kernel `5.12`)
The weight indicates how costly the event was.
From our experience, the access latency and can only be accessed on Intel hardware (ARM may work, too).
Since Linux Kernel version `5.12`, the Kernel might generate more information than only the "weight".
Can be accessed via `sample.weight()`, which returns a `perf::Weight` class, which has the following attributes:
* `sample.weight().value().latency()` returns the latency (for both `perf::Sampler::Type::Weight` and `perf::Sampler::Type::WeightStruct`).
* `sample.weight().value().var2()` returns "other information" (not specified by perf) **but** only for `perf::Sampler::Type::WeightStruct`. On Intel's Sapphire Rapids architecture, it seems to record the instruction latency (which is higher than the load latency and includes the latter).
* `sample.weight().value().var3()` returns "other information" (not specified by perf) **but** only for `perf::Sampler::Type::WeightStruct`.

&rarr; [See example](../examples/address_sampling.cpp)

#### Specific Notice for Intel's Sapphire Rapids architecture
To use weight-sampling on Intel's Sapphire Rapids architecture, perf needs an auxiliary counter to be added to the group, before the "real" counter is added (see [this commit](https://lore.kernel.org/lkml/1612296553-21962-3-git-send-email-kan.liang@linux.intel.com/)).
*perf-cpp*  defines this counter, you only need to add it accordingly:

```cpp
 auto sampler = perf::Sampler{ counter_definitions,
                   std::vector<std::string>{/* helper: */"mem-loads-aux", 
                                            /* real counter: */ "your-memory-counter"},
                   perf::Sampler::Type::Time 
                    | perf::Sampler::Type::LogicalMemAddress 
                    | perf::Sampler::Type::DataSource 
                    | perf::Sampler::Type::WeightStruct,
                   perf_config };
```

The sampler will detect that auxiliary counter automatically.

### `perf::Sampler::Type::DataSource`
Data source where the data was sampled (e.g., local mem, remote mem, L1d, L2, ...).
From our experience, this only works on Intel hardware (ARM might work, too) and only with specific triggers.

The data source can be accessed via `sample.data_source()`, which provides a specific `perf::DataSource` object.
The `perf::DataSource` object can be queried if ...
* ... the data was loaded (`sample.data_source().value().is_load()`)
  * or stored (`sample.data_source().value().is_store()`)
  * or prefetched (`sample.data_source().value().is_prefetch()`)
* ... the data was a (memory subsystem related) hit (`sample.data_source().value().is_mem_hit()`)
  * or miss (`sample.data_source().value().is_mem_miss()`)
* ... the data was found (or missed) in the L1 data cache (`sample.data_source().value().is_mem_l1()`)
  * or Line Fill Buffer / Miss Address Buffer (`sample.data_source().value().is_mem_lfb()`)
  * or L2 cache (`sample.data_source().value().is_mem_l2()`)
  * or L3 cache (`sample.data_source().value().is_mem_l3()`)
  * or local memory (`sample.data_source().value().is_mem_local_ram()`)
  * or remote memory with one hop distance (`sample.data_source().value().is_mem_remote_ram1()`)
  * or remote memory with two hops distance (`sample.data_source().value().is_mem_remote_ram2()`)
  * or remote cache with one hop distance (`sample.data_source().value().is_mem_remote_cce1()`)
  * or remote cache with two hops distance (`sample.data_source().value().is_mem_remote_cce2()`)
* ... the data was a TLB hit (`sample.data_source().value().is_tlb_hit()`)
  * or miss (`sample.data_source().value().is_tlb_miss()`)
* ... the dTLB was accessed (`sample.data_source().value().is_tlb_l1()`)
  * or the STLB was accessed (`sample.data_source().value().is_tlb_l2()`)

&rarr; [See example](../examples/address_sampling.cpp)

### `perf::Sampler::Type::Identifier`
Sample id, accessible via `sample.id()`.

### `perf::Sampler::Type::PhysicalMemAddress`
The physical memory address.
Can be accessed via `sample.physical_memory_address()`.
From our experience, this only works on Intel hardware (ARM might work, too) and only with specific triggers.
You may need to adjust the `sample_config.precise_ip(X)` setting on different hardware (ranging from `0` to `3`).

### `perf::Sampler::Type::DataPageSize`
Size of pages of sampled data addresses (e.g., when sampling for `perf::Sample::Type::LogicalMemoryAddress`).
Can be accessed via `sample.data_page_size()`.

### `perf::Sampler::Type::CodePageSize`
Size of pages of sampled instruction pointers (e.g., when sampling for `perf::Sample::Type::InstructionPointer`).
Can be accessed via `sample.code_page_size()`.

