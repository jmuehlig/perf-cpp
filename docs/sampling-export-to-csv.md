# Exporting Samples to CSV

Export recorded samples to CSV for analysis with tools like Python (pandas), R, Excel, or custom scripts.

> [!TIP]
> See the example: **[instruction_pointer.cpp](https://github.com/jmuehlig/perf-cpp/tree/dev/examples/sampling/instruction_pointer.cpp)**.

---

## Exporting Samples

```cpp
#include <perfcpp/sampler.hpp>

auto sampler = perf::Sampler{};
sampler.trigger("cycles", perf::Period{ 50000U });
sampler.values()
    .timestamp(true)
    .logical_instruction_pointer(true)
    .cpu_id(true)
    .data_source(true);

sampler.start();
/// ... computation here ...
sampler.stop();

/// Export samples to a file or retrieve as string.
const auto samples = sampler.result();
samples.to_csv("samples.csv");
const auto csv_string = samples.to_csv();

/// Release resources explicitly, or let the destructor handle it.
sampler.close();
```

Each row is one sample, each column a recorded field. Fields not configured via `sampler.values()` appear as empty cells.

## Customizing Delimiters

Both overloads accept optional delimiter parameters:

- **Column delimiter** (default `,`): separates fields in each row
- **List delimiter** (default `;`): separates elements in list-type fields like callchains or registers

```cpp
/// Semicolon as column separator (useful for European Excel).
samples.to_csv("samples.csv", ';', '|');
```

## CSV File Structure

The CSV file contains a header row followed by one row per sample:

```
mode,timestamp,cpu_id,logical_instruction_pointer,data_access_is_l1d_hit,data_access_is_l2_hit,...
User,365449130714033,8,0x5a6e84b2075c,1,0,...
User,365449130913157,8,0x64af7417c75c,0,1,...
User,365449131112591,8,0x5a6e84b2075c,1,0,...
```

The `mode` field is always present and indicates execution context (`User`, `Kernel`, `Hypervisor`, `GuestKernel`, or `GuestUser`).
Some fields are platform-specific (Intel vs AMD) — see [field reference](#field-reference) below.

## Field Reference

All CSV columns and their corresponding `sampler.values()` configurations.
For detailed descriptions, see the [sampling documentation](sampling.md).

### Metadata Fields

| CSV Column | How to Record | Description | Always Present |
|------------|---------------|-------------|----------------|
| `mode` | Always recorded | Execution mode (`User`, `Kernel`, `Hypervisor`, `GuestKernel`, `GuestUser`) | Yes |
| `id` | `sampler.values().sample_id(true)` | Unique sample group leader ID | No |
| `stream_id` | `sampler.values().stream_id(true)` | Event stream identifier | No |
| `timestamp` | `sampler.values().timestamp(true)` | Sample timestamp (nanoseconds) | No |
| `period` | `sampler.values().period(true)` | Event count threshold that triggered the sample | No |
| `cpu_id` | `sampler.values().cpu_id(true)` | CPU core where sample was recorded | No |
| `process_id` | `sampler.values().thread_id(true)` | Process ID | No |
| `thread_id` | `sampler.values().thread_id(true)` | Thread ID | No |

### Instruction Execution Fields

Fields related to the sampled instruction and its execution characteristics.

| CSV Column | How to Record | Description | Platform |
|------------|---------------|-------------|----------|
| `instruction_type` | `sampler.values().instruction_type(true)` | Instruction type (`Return`, `Branch`, `DataAccess`) | AMD only |
| `is_microcode` | `sampler.values().instruction_type(true)` | Op was dispatched from the microcode ROM sequencer (`0` or `1`) | AMD Op PMU |
| `logical_instruction_pointer` | `sampler.values().logical_instruction_pointer(true)` | Logical address of sampled instruction (hexadecimal) | All |
| `is_instruction_pointer_exact` | `sampler.values().logical_instruction_pointer(true)` | Whether instruction pointer is exact (`0` or `1`) | All |
| `physical_instruction_pointer` | `sampler.values().physical_instruction_pointer(true)` | Physical address of sampled instruction (hexadecimal) | AMD Fetch PMU |
| `instruction_l1i_miss` | `sampler.values().instruction_cache(true)` | L1 instruction cache miss (`0` or `1`) | AMD Fetch PMU |
| `instruction_l2_miss` | `sampler.values().instruction_cache(true)` | L2 cache miss (`0` or `1`) | AMD Fetch PMU |
| `instruction_l3_miss` | `sampler.values().instruction_cache(true)` | L3 cache miss (`0` or `1`) | AMD Fetch PMU |
| `instruction_op_cache_miss` | `sampler.values().instruction_cache(true)` | Op cache (decoded instruction cache) miss (`0` or `1`) | AMD Fetch PMU |
| `instruction_itlb_miss` | `sampler.values().instruction_tlb(true)` | Instruction TLB miss (`0` or `1`) | AMD Fetch PMU |
| `instruction_itlb_size` | `sampler.values().instruction_tlb(true)` | Instruction TLB page size (bytes) | AMD Fetch PMU |
| `instruction_stlb_miss` | `sampler.values().instruction_tlb(true)` | Second-level TLB miss (`0` or `1`) | AMD Fetch PMU |
| `instruction_retirement_latency` | `sampler.values().instruction_latency(true)` | Instruction retirement latency (cycles) | Intel only |
| `uop_tag_to_retirement_latency` | `sampler.values().instruction_latency(true)` | Micro-op tag-to-retirement latency (cycles) | AMD only |
| `uop_tag_to_completion` | `sampler.values().instruction_latency(true)` | Micro-op tag-to-completion latency (cycles) | AMD only |
| `uop_completion_to_retirement` | `sampler.values().instruction_latency(true)` | Micro-op completion-to-retirement latency (cycles) | AMD only |
| `instruction_fetch_latency` | `sampler.values().instruction_latency(true)` | Instruction fetch latency (cycles) | AMD Fetch PMU |
| `instruction_itlb_refill_latency` | `sampler.values().instruction_latency(true)` | iTLB refill latency after a miss (cycles) | AMD Fetch PMU |
| `branch_type` | `sampler.values().branch_type(true)` | Branch type (`Taken`, `Retired`, `Mispredicted`, `Fuse`) | AMD Op PMU |
| `tx_is_elision` | `sampler.values().hardware_transaction_abort(true)` | Hardware transaction is elision type (`0` or `1`) | Intel only |
| `tx_is_generic` | `sampler.values().hardware_transaction_abort(true)` | Hardware transaction is generic type (`0` or `1`) | Intel only |
| `tx_is_synchronous_abort` | `sampler.values().hardware_transaction_abort(true)` | Synchronous transaction abort (`0` or `1`) | Intel only |
| `tx_is_retryable` | `sampler.values().hardware_transaction_abort(true)` | Transaction abort is retryable (`0` or `1`) | Intel only |
| `tx_is_memory_conflict` | `sampler.values().hardware_transaction_abort(true)` | Abort due to memory conflict (`0` or `1`) | Intel only |
| `tx_is_write_capacity_conflict` | `sampler.values().hardware_transaction_abort(true)` | Abort due to write capacity (`0` or `1`) | Intel only |
| `tx_is_read_capacity_conflict` | `sampler.values().hardware_transaction_abort(true)` | Abort due to read capacity (`0` or `1`) | Intel only |
| `tx_user_code` | `sampler.values().hardware_transaction_abort(true)` | User-specified transaction abort code | Intel only |
| `code_page_size` | `sampler.values().code_page_size(true)` | Page size of instruction pointer (bytes) | Linux ≥ 5.11 |
| `callchain` | `sampler.values().callchain(true)` | Call stack (list of addresses separated by `;`) | All |

### Data Access Fields

Fields describing memory access behavior, cache hierarchy, and data sources.

| CSV Column | How to Record | Description | Platform |
|------------|---------------|-------------|----------|
| `data_access_type` | `sampler.values().data_source(true)` | Access type (`Load`, `Store`, `Prefetch`) | All |
| `is_locked` | `sampler.values().data_source(true)` | Locked memory operation (`0` or `1`) | All |
| `logical_memory_address` | `sampler.values().logical_memory_address(true)` | Logical memory address (hexadecimal) | All |
| `physical_memory_address` | `sampler.values().physical_memory_address(true)` | Physical memory address (hexadecimal) | Linux ≥ 4.13 |
| `data_access_is_l1d_hit` | `sampler.values().data_source(true)` | L1 data cache hit (`0` or `1`) | All |
| `data_access_is_mhb_hit` | `sampler.values().data_source(true)` | LFB/MAB hit (`0` or `1`) | All |
| `mhb_slots_allocated` | `sampler.values().mhb_allocations(true)` | Number of MAB slots allocated | AMD Op PMU |
| `data_access_is_l2_hit` | `sampler.values().data_source(true)` | L2 cache hit (`0` or `1`) | All |
| `data_access_is_l3_hit` | `sampler.values().data_source(true)` | L3 cache hit (`0` or `1`) | All |
| `data_access_is_memory_hit` | `sampler.values().data_source(true)` | Main memory access (`0` or `1`) | All |
| `data_access_is_remote` | `sampler.values().data_source(true)` | Remote core/node access (`0` or `1`) | All |
| `data_access_is_same_node_remote_core` | `sampler.values().data_source(true)` | Same node, different core (`0` or `1`) | All |
| `data_access_is_same_socket_remote_node` | `sampler.values().data_source(true)` | Same socket, different node (`0` or `1`) | All |
| `data_access_is_same_board_remote_socket` | `sampler.values().data_source(true)` | Same board, different socket (`0` or `1`) | All |
| `data_access_is_remote_board` | `sampler.values().data_source(true)` | Different board (`0` or `1`) | All |
| `dtlb_hit` | `sampler.values().data_source(true)` | Data TLB hit (`0` or `1`) | All |
| `stlb_hit` | `sampler.values().data_source(true)` | Second-level TLB hit (`0` or `1`) | All |
| `dtlb_page_size` | `sampler.values().data_tlb_page_size(true)` | Data TLB page size (bytes) | AMD Op PMU |
| `stlb_page_size` | `sampler.values().data_tlb_page_size(true)` | Second-level TLB page size (bytes) | AMD Op PMU |
| `cache_access_latency` | `sampler.values().data_access_latency(true)` | Cache access latency (cycles) | Intel only |
| `cache_miss_latency` | `sampler.values().data_access_latency(true)` | Cache miss latency (cycles) | AMD only |
| `dtlb_refill_latency` | `sampler.values().data_tlb_latency(true)` | Data TLB refill latency (cycles) | AMD Op PMU |
| `snoop_is_hit` | `sampler.values().data_source(true)` | Snoop hit (`0` or `1`) | All |
| `snoop_is_hit_modified` | `sampler.values().data_source(true)` | Snoop hit on modified line (`0` or `1`) | All |
| `snoop_is_forward` | `sampler.values().data_source(true)` | Cache line forwarded (`0` or `1`) | All |
| `snoop_is_transfer_from_peer` | `sampler.values().data_source(true)` | Transfer from peer node (`0` or `1`) | All |
| `is_misalign_penalty` | `sampler.values().data_access_misalign_penalty(true)` | Misalignment penalty (`0` or `1`) | AMD Op PMU |
| `data_access_width` | `sampler.values().data_access_width(true)` | Access width (bytes) | AMD Op PMU |
| `data_page_size` | `sampler.values().data_page_size(true)` | Data page size (bytes) | Linux ≥ 5.11 |

> [!IMPORTANT]
> Memory access fields require specific triggers:
> - **Intel**: Use `mem-loads` or `mem-stores` triggers with `perf::Precision::MustHaveZeroSkid`
> - **AMD**: Use `ibs_op` family triggers
>
> See the [sampling documentation](sampling.md#specific-notes-for-different-cpu-vendors) for details.

### Performance Counter Fields

Counter values recorded at the time of sampling.

| CSV Column | How to Record | Description |
|------------|---------------|-------------|
| `counter_<name>` | `sampler.values().counter({"cycles", "instructions", ...})` | One column per configured counter (e.g., `counter_cycles`, `counter_instructions`) |

**Example:**
```cpp
sampler.values().counter({"cycles", "instructions", "cache-misses"});
```
Creates columns: `counter_cycles`, `counter_instructions`, `counter_cache-misses`.

### Register Fields

Register values captured at sampling time.

| CSV Column | How to Record | Description |
|------------|---------------|-------------|
| `user_register_<name>` | `sampler.values().user_registers({...})` | User-space register (e.g., `user_register_AX`, `user_register_R10`) |
| `kernel_register_<name>` | `sampler.values().kernel_registers({...})` | Kernel-space register (e.g., `kernel_register_AX`) |

**Example:**
```cpp
sampler.values().user_registers({perf::Registers::x86::AX, perf::Registers::x86::R10});
```
Creates columns: `user_register_AX`, `user_register_R10`.

### Control Group Fields

Information about control groups (cgroups).

| CSV Column | How to Record | Description | Kernel Version |
|------------|---------------|-------------|----------------|
| `cgroup_id` | `sampler.values().cgroup(true)` | CGroup ID of sample | Linux ≥ 5.7 |
| `cgroup_cgroup_id` | `sampler.values().cgroup(true)` | New cgroup ID (if changed) | Linux ≥ 5.7 |
| `cgroup_path` | `sampler.values().cgroup(true)` | New cgroup path (if changed) | Linux ≥ 5.7 |

### Context Switch Fields

Context switch event information.

| CSV Column | How to Record | Description | Kernel Version |
|------------|---------------|-------------|----------------|
| `context_switch_is_out` | `sampler.values().context_switch(true)` | Process switched out (`0` or `1`) | Linux ≥ 4.3 |
| `context_switch_is_in` | `sampler.values().context_switch(true)` | Process switched in (`0` or `1`) | Linux ≥ 4.3 |
| `context_switch_is_preempt` | `sampler.values().context_switch(true)` | Process preempted (`0` or `1`) | Linux ≥ 4.3 |
| `context_switch_process_id` | `sampler.values().context_switch(true)` | Process ID involved in switch | Linux ≥ 4.3 |
| `context_switch_thread_id` | `sampler.values().context_switch(true)` | Thread ID involved in switch | Linux ≥ 4.3 |

### Throttle Fields

Throttling event information (when kernel reduces sampling rate).

| CSV Column | How to Record | Description |
|------------|---------------|-------------|
| `is_throttle` | `sampler.values().throttle(true)` | Sampling was throttled (`0` or `1`) |
| `is_unthrottle` | `sampler.values().throttle(true)` | Sampling was unthrottled (`0` or `1`) |

### Loss Event Fields

Information about lost samples due to buffer overflow.

| CSV Column | Description | When Present |
|------------|-------------|--------------|
| `loss_count` | Number of samples lost | Only if any sample contains a loss event |

