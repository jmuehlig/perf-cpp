# Hardware Performance Counters and Events

Performance events are hardware and software counters that help you understand how your application behaves at the CPU level—tracking everything from cache misses to branch predictions. 
Each CPU generation brings new events specific to its microarchitecture, making it essential to use the right events for your hardware.

This guide explains how *perf-cpp* handles performance events and how you can use both generic and processor-specific events in your measurements.

---

## Understanding Event Storage and Management
### The CounterDefinition System
At its core, *perf-cpp* uses a simple but powerful concept: events are stored in a key-value store that maps human-readable event names to hardware-specific event codes. 
This abstraction shields you from dealing with cryptic hexadecimal codes directly.

The `perf::CounterDefinition` class manages this mapping. It acts as a dictionary that both `perf::EventCounter` and `perf::Sampler` use to translate the event names you provide into the codes that the perf subsystem understands.

### Default vs. Custom Configurations
**The default approach** is straightforward–*perf-cpp* creates a global `CounterDefinition` instance automatically, preloaded with common events that work across most systems:

```cpp
/// Using the default configuration – no setup needed
auto event_counter = perf::EventCounter{};
auto sampler = perf::Sampler{};
```

**For custom needs**, you can create your own `CounterDefinition` instance. This doesn't replace the defaults—it extends them:

```cpp
/// Create a custom configuration that inherits all defaults
auto counter_definitions = perf::CounterDefinition{};

/// Add your specific event
counter_definitions.add(
    "cycle_activity.stalls_l3_miss",  // Human-readable name
    0x65306a3                          // Hardware event code
);

/// Use your extended configuration
auto event_counter = perf::EventCounter{ counter_definitions };
auto sampler = perf::Sampler{ counter_definitions };
```

> [!IMPORTANT]
> Keep your `CounterDefinition` instance alive throughout your measurement session. 
> Event names are stored only in this instance, so destroying it prematurely will cause issues when retrieving results.

## Working with Built-in Events
*perf-cpp* includes a comprehensive set of events that work reliably across different CPU architectures. 
You can use these immediately without any configuration.

### Hardware Events
These fundamental events are supported by most modern processors:

```
branches                 # Total branch instructions
branch-instructions      # Synonym for branches
branch-misses            # Mispredicted branches
cache-misses             # Cache access that missed
cache-references         # Cache accesses
cycles                   # CPU cycles
cpu-cycles               # Synonym for cycles
instructions             # Retired instructions
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
These events come from the kernel rather than hardware counters:

```
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
*perf-cpp* provides virtual events that use `std::chrono` for wall-clock time measurements. 
These are particularly useful when creating custom [metrics](metrics.md):

```
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
> Time events are always measured *after opening* and *before stopping* performance counters, i.e., the overhead for accessing performance counters is **not** included.

## Using Processor-Specific Events
While built-in events provide good coverage, modern processors offer hundreds of specialized events that reveal deeper performance characteristics. 
*perf-cpp* provides several ways to access these.

> [!TIP]
> To discover available events on your system, use `perf list` for a quick overview. Intel users can explore the [Intel PerfMon website](https://perfmon-events.intel.com/) for detailed event descriptions and recommendations.

### Loading Events from the Event Library
*perf-cpp* ships with curated event definitions for various processors in the [events/x86](../events/x86) directory. 
Load them directly:

```cpp
/// Load AMD Zen 4 specific events
const auto counter_definition = perf::CounterDefinition{ "events/x86/amd/zen-4.csv" };

auto event_counter = perf::EventCounter{ counter_definition };
event_counter.add("ex_ret_instr");  // Use a Zen 4 specific event
```

To see what events are available in your loaded configuration:

```cpp
#include <perfcpp/counter_definition.hpp>
#include <iostream>

const auto counter_definition = perf::CounterDefinition{ "events/x86/amd/zen-4.csv" };
std::cout << counter_definition.to_string() << std::endl;
```

### Auto-Generating Events at Compile Time
For the smoothest experience, *perf-cpp* can detect your processor and generate the appropriate event definitions automatically during compilation. 
Enable this feature when building:

```bash
cmake . -B build -DGEN_PROCESSOR_EVENTS=1
cmake --build build
```

During build, you'll see confirmation that your processor was detected:

```
[GEN_PROCESSOR_EVENTS] Detected micro-architecture: amdzen4
[GEN_PROCESSOR_EVENTS] Generated source file with 502 events.
[GEN_PROCESSOR_EVENTS] Wrote source file with processor-specific events: src/processor_specific_event_provider.cpp
```

Once built this way, processor-specific events become available automatically–no manual loading required.

> [!IMPORTANT]
> Auto-generation is experimental. 
> Always validate your measurements, as event configurations may vary between processors or require specific kernel support.

### Adding Custom Events Programmatically
You're not limited to predefined events. 
Add any event if you know its code:

```cpp
auto counter_definitions = perf::CounterDefinition{};

/// Add a single event with its raw code
counter_definitions.add(
    "cycle_activity.stalls_l3_miss",
    0x65306a3
);

/// For complex events requiring additional configuration
counter_definitions.add(
    "complex_event_name",
    0x1234,     /// config
    0x5678,     /// config1 (optional)
    4           /// type (optional)
);
```

You can also create custom CSV files following the format: `name,config[,config1,type]` and load them the same way as the built-in event library files.

## Translating Event Names to Event Codes
When you find an interesting event in `perf list` or documentation, you need its raw code to use it in *perf-cpp*. 
Here are two reliable methods.

### Using libpfm4

The *libpfm4* library excels at translating event names to codes:

1. Clone or download *libpfm4*: [https://github.com/wcohen/libpfm4](https://github.com/wcohen/libpfm4)
2. Build with `make`
3. Navigate to the `examples/` directory
4. Use the `check_events` tool:

```bash
./check_events cycle_activity.stalls_l3_miss
```

The output provides the raw code you need for your configuration.

### Using perf with Debug Output
The Linux perf tool itself can reveal event codes using debug mode:

```bash
perf --debug perf-event-open stat -e ex_ret_instr ls
```

Look for the `config` field in the output:

```
perf_event_attr:
  type                             4 (cpu)
  size                             136
  config                           0xc0 (ex_ret_instr)
  sample_type                      IDENTIFIER
  read_format                      TOTAL_TIME_ENABLED|TOTAL_TIME_RUNNING
  disabled                         1
  inherit                          1
  enable_on_exec                   1
  exclude_guest                    1
```

The `config` value (0xc0 in this example) is your event code:

```cpp
counter_definitions.add("ex_ret_instr", 0xc0);
```

## Detecting Hardware Capabilities at Runtime
Different processors support different features. 
The `perf::HardwareInfo` class lets you adapt your measurements to the running system:

```cpp
#include <perfcpp/hardware_info.hpp>

if (perf::HardwareInfo::is_intel()) {
    /// Configure Intel-specific events
    /// Use Intel-optimized sampling configurations
}

if (perf::HardwareInfo::is_amd()) {
    /// Configure AMD-specific events
    
    if (perf::HardwareInfo::is_amd_ibs_supported()) {
        /// IBS (Instruction-Based Sampling) is available
        /// Can use ibs_op and related AMD sampling features
        /// See sampling documentation for details
    }
}
```

This runtime detection enables you to write portable code that automatically uses the best available events for each system.

&rarr; [See complete example](../examples/sampling/memory_address.cpp)