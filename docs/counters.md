# Built-in and Processor-specific Performance Events

Modern CPUs introduce new performance events with each generation, often unique to their micro-architecture. 
To accurately measure performance across diverse hardware platforms, it’s important to use events tailored to the underlying processor.

The `perf::CounterDefinition` class allows you to define and integrate both standard and hardware-specific performance events.

> [!TIP] 
> *perf-cpp* includes an event code library for *x86* processors (see the [events/ directory](../events/x86)).
> These files can be added via the `perf::CounterDefinition` class (see below).

For an extensive catalog including event descriptions of *Intel-specific* events, refer to the official [Intel PerfMon website](https://perfmon-events.intel.com/).

---
## Table of Contents
- [Built-in Events](#built-in-events)
- [Adding Hardware-Specific Events](#adding-hardware-specific-events)
   - [Directly in Code](#directly-in-code)
   - [Through Configuration Files](#through-configuration-files)
- [Retrieving Raw Event Codes](#retrieving-raw-event-codes)
   - [Auto-Generate a Configuration File](#auto-generate-a-configuration-file)
   - [Manual Retrieval with libpfm4](#manual-retrieval-with-libpfm4)
- [Runtime Hardware Querying](#runtime-hardware-querying)
---

## Built-in Events
*perf-cpp* comes with a set of built-in events that are broadly supported across modern CPU architectures. 
These events are ready to use out of the box:

```
branches 
branch-instructions
branch-misses
cache-misses
cache-references
cycles 
cpu-cycles
instructions
stalled-cycles-backend 
idle-cycles-backend
stalled-cycles-frontend 
idle-cycles-frontend

L1-dcache-loads
L1-dcache-load-misses
L1-icache-loads
L1-icache-load-misses
dTLB-loads
dTLB-load-misses
iTLB-loads
iTLB-load-misses

cpu-clock
task-clock
page-faults
faults
major-faults
minor-faults
alignment-faults
emulation-faults
context-switches
bpf-output          # only since Linux Kernel 4.4
cgroup-switches     # only since Linux Kernel 5.13
cpu-migrations
migrations
```

In addition, *perf-cpp* supports *virtual time events*, which use `std::chrono` rather than hardware events. 
These are useful for measuring wall-clock time or for integrating time into custom [metrics](metrics.md):

```
seconds
s               # short for seconds
milliseconds 
ms              # short for milliseconds
microseconds
us              # short for microseconds
nanoseconds 
ns              # short for nanoseconds
```

## Processor-Specific Events
Many "interesting" events depend on the actual hardware.
Although the generic events listed above can provide a good glimpse, many processors support sophisticated events.

> [!TIP]
> To find "interesting" events, check also `perf list`, which shows additional descriptions and the [Intel PerfMon website](https://perfmon-events.intel.com/) (if you are using an Intel system).


### Event Library
*perf-cpp* includes a library of different (for the time being only) *x86* events in [events/x86](../events/x86). 
The event specifications allow a good impact on different events and can also be loaded via the `perf::CounterDefinition` class:

```cpp
const auto counter_definition = perf::CounterDefinition{ "events/x86/amd-zen-4.csv" };

auto event_counter = perf::EventCounter{ counter_definition };
event_counter.add("ex_ret_instr"); /// Add ex_ret_instr event which is specific to 
                                   /// AMD Zen4 and defined in events/x86/amdzen4.csv
/// Start, measure, stop, ...
```

The list of available events registered can be printed via:

```cpp
#include <perfcpp/counter_definition.h>
#include <iostream>

const auto counter_definition = perf::CounterDefinition{ "events/x86/amd-zen-4.csv" };
std::cout << counter_definition.to_string() << std::endl;
```

### Generating Processor-Specific Events at Compile Time
However, copying and choosing the matching CSV file might be cumbersome.
For an easier use, *perf-cpp* can auto-generate a C++ source file containing the processor-specific events at compile time.
The new class will be compiled and linked automatically; processor-specific events are available out-of-the box.

For the time begin, the option `GEN_PROCESSOR_EVENTS` needs to be activated when building *perf-cpp*:

```bash
cmake . -B build  -DGEN_PROCESSOR_EVENTS=1
cmake --build build
```

*perf-cpp* will let you know if the processor was successfully identified:

```
[GEN_PROCESSOR_EVENTS] Detected micro-architecture: amdzen4
[GEN_PROCESSOR_EVENTS] Generated source file with 502 events.
[GEN_PROCESSOR_EVENTS] Wrote source file with processor-specific events: src/processor_specific_event_provider.cpp
```

> [!IMPORTANT]
> Please be careful when using this option and double-check your results as events might be configured wrong.
> We have not tested all available processors. 

### Adding Events Manually
In general, the `perf::CounterDefinition` class can read any `.csv` file of the format `name,config[,config1,type]`, not only files from [events/x86](../events/x86).
Note that *config1* and *type* are optional and may be required for complex events

However, events can also be added directly within the code:

```cpp
auto counter_definitions = perf::CounterDefinition{};
counter_definitions.add(
    /* event name = */ "cycle_activity.stalls_l3_miss", 
    /* event code = */ 0x65306a3
);
```

### Translating Event Names into Codes
#### Via Libpfm4
As one can see in the example above, events need to be configured using event codes.
The `perf list` command, on the other hand, only provides event names.

Luckily, the *[libpfm4](https://github.com/wcohen/libpfm4)* library can help in translating event names into codes:

1. Clone or download *libpfm4*: [https://github.com/wcohen/libpfm4](https://github.com/wcohen/libpfm4)
2. Run `make` to build all binaries.
3. Navigate to the `examples/` directory.
4. Use `perf list` to identify an event of interest.
5. Run the `check_events` tool to retrieve the raw code:

```bash
./check_events cycle_activity.stalls_l3_miss
```

The output will include the identifier that can be used in your configuration.

#### Via Perf
Also, the Linux perf tool can help to translate event names into codes by using the `--debug perf-event-open` flag:

```bash
perf --debug perf-event-open stat -e ex_ret_instr ls
```

The output will include the `config`, which relates to the event code, for example:

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

Accordingly, the following snippet would define the `ex_ret_instr` event for *perf-cpp*:

```cpp
counter_definitions.add("ex_ret_instr", 0xc0);
```

## Runtime Hardware Querying
To tailor event configurations based on the executing system, *perf-cpp* provides the `perf::HardwareInfo` class.
This utility lets you dynamically detect supported features and capabilities at runtime:

&rarr; [See code example](../examples/sampling/memory_address.cpp)

```cpp
#include <perfcpp/hardware_info.h>

if (perf::HardwareInfo::is_intel()) {
  /// Add intel-specifics like events, etc.
}

if (perf::HardwareInfo::is_amd()) {
    /// Add amd-specifics like events, etc.
    
    if (perf::HardwareInfo::is_amd_ibs_supported()) {
        /// You can use ibs_op and further AMD IBS-related sampling mechanisms.
        /// See sampling documentation for specifics.
    }
}
```