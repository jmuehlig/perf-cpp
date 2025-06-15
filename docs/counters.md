# Built-in and Hardware-specific Performance Events

Modern CPUs introduce new performance events with each generation, often unique to their micro-architecture. 
To accurately measure performance across diverse hardware platforms, it’s important to use events tailored to the underlying processor.

The `perf::CounterDefinition` class allows you to define and integrate both standard and hardware-specific performance counters.

> [!TIP] 
> To simplify the process, *perf-cpp* includes tooling to automatically detect and configure these events (see [Retrieving Raw Event Codes](#retrieving-raw-event-codes) for details).

For an extensive catalog of *Intel-specific* events, refer to the official [Intel PerfMon website](https://perfmon-events.intel.com/).

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

In addition, *perf-cpp* supports *virtual time events*, which use `std::chrono` rather than hardware counters. 
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

## Adding Hardware-Specific Events
Event names and configurations are managed via the `perf::CounterDefinition` class, which is passed by reference to `perf::EventCounter` or `perf::Sampler`.
By default, instances share a built-in configuration, but you can provide a custom definition to support additional events or metrics:

```cpp
auto counter_definition = perf::CounterDefinition{};            /// Create own instance
auto event_coutner = perf::EventCounter {counter_definition };  /// Pass as a reference
```


> [!IMPORTANT] 
>  If you are using a custom `perf::CounterDefinition`, ensure it remains valid for the entire duration of monitoring or sampling.

### Directly in Code
To add hardware-specific events programmatically, use the `add()` method on your custom `perf::CounterDefinition` and pass it as a reference to the `perf::EventCounter` or `perf::Sampler`:

```cpp
const auto counter_definitions = perf::CounterDefinition{};
counter_definitions.add(
    /* event name = */ "cycle_activity.stalls_l3_miss", 
    /* event code = */ 0x65306a3
);

auto event_counter = perf::EventCounter{ counter_definiton };
event_counter.add({"cycles", "instructions", "cycle_activity.stalls_l3_miss"});
event_counter.start();
/// ...
event_counter.stop();

const auto result = event_counter.result(); /// Will contain results for 'cycle_activity.stalls_l3_miss'
```

> [!TIP]
> Hardware event codes are *platform-specific*.
> See [Retrieving Raw Event Codes](#retrieving-raw-event-codes) to get the correct values for your system.

### Through Configuration Files
For convenience, hardware-specific events can also be defined using a simple *CSV-style configuration file*, making it easy to manage large sets of counters:

```cpp
const auto counter_definition = perf::CounterDefinition{"perf_list.csv"};
auto event_counter = perf::EventCounter{ counter_definiton };

event_counter.add({"cycle_activity.stalls_l1d_miss", 
                   "cycle_activity.stalls_l2_miss", 
                   "cycle_activity.stalls_l3_miss"});
event_counter.start();
/// ...
event_counter.stop();

```

An example `perf_list.csv` might look like:

```csv
cycle_activity.stalls_l1d_miss,0xc530ca3
cycle_activity.stalls_l2_miss,0x55305a3
cycle_activity.stalls_l3_miss,0x65306a3
```

> [!TIP]
> *perf-cpp* can auto-generate this CSV file based on your system's capabilities. See the section below.


## Retrieving Raw Event Codes
### Auto-Generate a Configuration File
The library includes a [helper script](../script/create_perf_list.py) to automatically extract all available raw performance event codes for your current hardware. 
This is similar to running `perf list`:

```bash
cmake .
cmake --build . --target perf-list
```

This process generates a `perf_list.csv` file containing event names and their corresponding raw codes, which can be directly consumed by `perf::CounterDefinition`:

```cpp
const auto counter_definition = perf::CounterDefinition{"perf_list.csv"};
auto event_counter = perf::EventCounter{ counter_definiton };
```

### Manual Retrieval with libpfm4
Behind the scenes, the automatic script leverages the *[libpfm4](https://github.com/wcohen/libpfm4)* library. 
You can also manually retrieve raw event codes using *libpfm4* as follows:

1. Clone or download *libpfm4*: [https://github.com/wcohen/libpfm4](https://github.com/wcohen/libpfm4)
2. Run `make` to build all binaries.
3. Navigate to the `examples/` directory.
4. Use `perf list` to identify an event of interest.
5. Run the `check_events` tool to retrieve the raw code:

```bash
./check_events cycle_activity.stalls_l3_miss
```

The output will include the identifier that can be used in your configuration.


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