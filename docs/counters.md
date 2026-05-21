# Customizing Events

Performance events map human-readable names to hardware-specific event codes. *perf-cpp* ships with common events that work across most systems, but each CPU generation brings specialized events for its microarchitecture.

1. **[The CounterDefinition System](#the-counterdefinition-system)**: How event name-to-code mapping works and how to extend it.
2. **[Built-in Events](#built-in-events)**: Hardware, software, and virtual time events available out of the box.
3. **[Processor-Specific Events](#processor-specific-events)**: Load from the event library, auto-generate at compile time, or add manually.
4. **[Translating Event Names to Event Codes](#translating-event-names-to-event-codes)**: Using libpfm4 or perf debug output.
5. **[Detecting Hardware Capabilities](#detecting-hardware-capabilities-at-runtime)**: Runtime checks for Intel/AMD features.

---

## The CounterDefinition System

`perf::CounterDefinition` manages the mapping from event names to event codes. Both `perf::EventCounter` and `perf::Sampler` use it to translate names into codes the perf subsystem understands.

By default, a global instance is created automatically with common events:

```cpp
/// Using the default configuration — no setup needed.
auto event_counter = perf::EventCounter{};
auto sampler = perf::Sampler{};
```

For custom events, create your own instance. This extends (not replaces) the defaults:

```cpp
auto counter_definitions = perf::CounterDefinition{};

/// Add a processor-specific event by name and hardware code.
counter_definitions.add("cycle_activity.stalls_l3_miss", 0x65306a3);

/// Use the extended configuration.
auto event_counter = perf::EventCounter{ counter_definitions };
auto sampler = perf::Sampler{ counter_definitions };
```

> [!IMPORTANT]
> Keep your `CounterDefinition` instance alive throughout your measurement session.
> Event names are stored only in this instance — destroying it prematurely will cause issues when retrieving results.

---

## Built-in Events

### Hardware Events

Supported by most modern processors:

```bash
branches                 # Total branch instructions
branch-instructions      # Synonym for branches
branch-misses            # Mispredicted branches
bus-cycles               # Bus cycles
cache-misses             # Cache access that missed
cache-references         # Cache accesses
cycles                   # CPU cycles (at current core frequency)
cpu-cycles               # Synonym for cycles
instructions             # Retired instructions
ref-cycles               # Reference cycles (at fixed frequency, unaffected by turbo/power saving)
stalled-cycles-backend   # Cycles stalled in backend
idle-cycles-backend      # Synonym for stalled-cycles-backend
stalled-cycles-frontend  # Cycles stalled in frontend
idle-cycles-frontend     # Synonym for stalled-cycles-frontend
L1-dcache-loads          # L1 data cache loads
L1-dcache-load-misses    # L1 data cache load misses
L1-icache-loads          # L1 instruction cache loads
L1-icache-load-misses    # L1 instruction cache load misses
dTLB-loads               # Data TLB loads
dTLB-load-misses         # Data TLB load misses
iTLB-loads               # Instruction TLB loads
iTLB-load-misses         # Instruction TLB load misses
```

### Software Events

From the kernel, not hardware counters:

```bash
cpu-clock             # High-resolution CPU timer
task-clock            # Clock count specific to task
page-faults           # Page fault count
faults                # Synonym for page-faults
major-faults          # Page faults requiring disk I/O
minor-faults          # Page faults handled without disk I/O
alignment-faults      # Alignment fault count
emulation-faults      # Instruction emulation count
context-switches      # Context switch count
bpf-output            # BPF program output (Linux 4.4+)
cgroup-switches       # Cgroup switch count (Linux 5.13+)
cpu-migrations        # Times process moved between CPUs
migrations            # Synonym for cpu-migrations
```

### Virtual Time Events

Virtual events using `std::chrono` for wall-clock time, useful for [metrics](metrics.md):

```bash
seconds         # Wall-clock seconds
s               # Short form
milliseconds    # Wall-clock milliseconds
ms              # Short form
microseconds    # Wall-clock microseconds
us              # Short form
nanoseconds     # Wall-clock nanoseconds
ns              # Short form
```

> [!TIP]
> Time events are measured *after opening* and *before stopping* performance counters — the overhead for accessing performance counters is **not** included.

---

## Processor-Specific Events

> [!TIP]
> Use `perf list` to discover available events on your system. Intel users can explore the [Intel PerfMon website](https://perfmon-events.intel.com/) for detailed event descriptions.

### Loading from the Event Library

*perf-cpp* ships with curated event definitions for various processors in [events/x86](https://github.com/jmuehlig/perf-cpp/tree/dev/events/x86):

```cpp
/// Load AMD Zen 4 specific events.
const auto counter_definition = perf::CounterDefinition{ "events/x86/amd/zen-4.csv" };

auto event_counter = perf::EventCounter{ counter_definition };
event_counter.add("ex_ret_instr");
```

To list all events in a loaded configuration:

```cpp
const auto counter_definition = perf::CounterDefinition{ "events/x86/amd/zen-4.csv" };
std::cout << counter_definition.to_string() << std::endl;
```

### Auto-Generating Events at Compile Time

*perf-cpp* can detect your processor and generate event definitions automatically during compilation:

```bash
cmake . -B build -DGEN_PROCESSOR_EVENTS=1
cmake --build build
```

Once built, processor-specific events are available automatically — no manual loading required.

> [!IMPORTANT]
> Auto-generation is experimental. Validate your measurements, as event configurations may vary between processors or require specific kernel support.

### Adding Custom Events Programmatically

Add any event if you know its code:

```cpp
auto counter_definitions = perf::CounterDefinition{};

/// Add a single event with its raw code.
counter_definitions.add("cycle_activity.stalls_l3_miss", 0x65306a3);

/// For events requiring a specific PMU type.
counter_definitions.add("complex_event_name", /* type = */ 4, /* config = */ 0x1234);

/// For events requiring extended configuration fields (config1 through config4).
auto event_config = perf::CounterConfig{ /* type = */ 4, /* config = */ 0x1234 };
event_config.config_extension(/* config1 = */ 0x5678, /* config2 = */ 0x0, /* config3 = */ 0x9abc);
counter_definitions.add("complex_event_name", std::move(event_config));
```

Custom CSV files following the format `name,config[,config1,type]` can be loaded the same way as built-in event library files.

---

## Translating Event Names to Event Codes

### Using libpfm4

The [libpfm4](https://github.com/wcohen/libpfm4) library translates event names to codes:

```bash
git clone https://github.com/wcohen/libpfm4.git
cd libpfm4
make
cd examples
./check_events cycle_activity.stalls_l3_miss
```

### Using perf with Debug Output

```bash
perf --debug perf-event-open stat -e ex_ret_instr ls
```

Look for the `config` field in the output:

```
perf_event_attr:
  type                             4 (cpu)
  size                             136
  config                           0xc0 (ex_ret_instr)
  ...
```

The `config` value (`0xc0`) and `type` (`4`) are your event code and PMU type:

```cpp
counter_definitions.add("ex_ret_instr", /* type = */ 4, /* config = */ 0xc0);
```

---

## Detecting Hardware Capabilities at Runtime

`perf::HardwareInfo` lets you adapt measurements to the running system:

```cpp
#include <perfcpp/hardware_info.hpp>

if (perf::HardwareInfo::is_intel()) {
    /// Configure Intel-specific events.
}

if (perf::HardwareInfo::is_amd()) {
    /// Configure AMD-specific events.

    if (perf::HardwareInfo::is_amd_ibs_supported()) {
        /// IBS available — can use ibs_op and related sampling features.
    }
}
```

> [!TIP]
> See the example: **[memory_address.cpp](https://github.com/jmuehlig/perf-cpp/tree/dev/examples/sampling/memory_address.cpp)**.
