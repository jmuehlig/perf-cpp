# Event Sampling

*perfcpp* enables the recording of event samples, capturing details like instruction pointers, multiple counters, branches, memory addresses, data sources, latency, and more. 
Essentially, you define a sampling period or frequency at which data is captured. 
At its core, this functionality is akin to traditional profiling tools, like `perf record`, but uniquely tailored to record specific blocks of code rather than the entire application.

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

&rarr; [See example](../examples/branch_sampling.cpp)

### `perf::Sampler::Type::Weight`
The weight indicates how costly the event was.
From our experience, the access latency and can only be accessed on Intel hardware (ARM may work, too).
Can be accessed via `sample.weight()`.

&rarr; [See example](../examples/address_sampling.cpp)

### `perf::Sampler::Type::DataSource`
Data source where the data was sampled (e.g., local mem, remote mem, L1d, L2, ..).
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