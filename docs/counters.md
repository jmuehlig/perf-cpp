# Built-in and Hardware-specific Performance Events

Modern CPUs introduce unique performance events with each new generation. 
To effectively measure performance across different systems, it's essential to utilize the appropriate events for each processor type.
The `perf::CounterDefinition` class plays a crucial role in this, allowing you to add performance counters for your specific hardware.

The library includes a script to retrieve hardware-specific events **automatically**.
See the [Retrieving Raw Event Codes](#retrieving-raw-event-codes) section for details.

For a comprehensive list of **Intel-specific** events, refer to the [perfmon website](https://perfmon-events.intel.com/).

---
## Table of Contents
- [Built-in Events](#built-in-events)
- [Incorporating Hardware-Specific Events](#incorporating-hardware-specific-events)
   - [Directly in Code](#directly-in-code)
   - [Through Configuration Files](#through-configuration-files)
- [Using Newly Added Events](#using-newly-added-events)
- [Retrieving Raw Event Codes](#retrieving-raw-event-codes)
   - [Automated Retrieval](#automated-retrieval)
   - [Manual Retrieval with libpfm4](#manual-retrieval-with-libpfm4)
- [Runtime Hardware Querying](#runtime-hardware-querying)
---

## Built-in Events
*perf-cpp* includes a variety of built-in performance events that are universally applicable across most CPU architectures.
These events are readily available for immediate use:

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

Additionally, *perf-cpp* supports *virtual* **time events** (i.e., they do not use hardware counter but `std::chrono`), that can be used as normal counters to measure time or use time in [metrics](metrics.md).

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

## Incorporating Hardware-Specific Events
All event names and configurations are managed within the `perf::CounterDefinition` class.
This class is passed by reference to `perf::EventCounter` and `perf::Sampler` instances.
**Important**: The `perf::CounterDefinition` instance must remain alive for the entire duration of the monitoring phase to ensure correct functionality.

### Directly in Code
You can define additional events directly in your code using the `add()` method provided by the `perf::CounterDefinition` interface. 
This method allows for specific configurations. 
Here's how you can add events:

```cpp
auto counter_definitions = perf::CounterDefinition{};
counter_definitions.add(
    /* event name = */ "cycle_activity.stalls_l3_miss", 
    /* event code = */ 0x65306a3
);
```

Typically, event codes are specific to the underlying hardware. 
Refer to the [Retrieving Raw Event Codes](#retrieving-raw-event-codes) section below to learn how to obtain the event codes for your system.

### Through Configuration Files
Alternatively, hardware-specific events can be added via a CSV-like configuration file, specifying each event's name and configuration details. 
This method facilitates the bulk addition of events. 
For example:

```cpp
auto counter_definition = perf::CounterDefinition{"perf_list.csv"};
```

The CSV file `events.csv` could look like the following:

```csv
cycle_activity.stalls_l1d_miss,0xc530ca3
cycle_activity.stalls_l2_miss,0x55305a3
cycle_activity.stalls_l3_miss,0x65306a3
```

## Using Newly Added Events
After defining the new events–whether directly in code or via configuration files–you can incorporate them into your performance measurements as follows:

```cpp
auto counter_definitions = perf::CounterDefinition{"perf_list.csv"};
auto event_counter = perf::EventCounter{counter_definitions};
event_counter.add({"cycles", "cycle_activity.stalls_l1d_miss"});
```

## Retrieving Raw Event Codes
### Automated Retrieval
The library provides a Python script (`script/create_perf_list.py`) to automate the retrieval of hardware event codes, similar to the `perf list` command. 
To generate a comprehensive list of events, execute the following commands:

```bash
cmake .
cmake --build . --target perf-list
```

This will produce a CSV file named `perf_list.csv`, containing the names and raw codes of all performance events available on your system. 
You can then pass this file to `perf::CounterDefinition` as shown in the [Through Configuration Files](#through-configuration-files) section.


### Manual Retrieval with libpfm4
The script utilizes the **[libpfm4](https://github.com/wcohen/libpfm4)** library.
However, for manual setup, you can utilize libpfm4 to fetch and configure events specific to your hardware:

1. Clone or download the libpfm4 repository from [GitHub](https://github.com/wcohen/libpfm4).
2. Call `make` to build all binaries.
3. Navigate to the `examples/` directory within the downloaded
4. Select and check a specific event:
    * Identify a performance event of interest on your machine by using the perf list command.
    * Retrieve the specific code for this event by running the check_events executable with the event's name as an argument. For example: `./check_events cycle_activity.stalls_l3_miss`
    * The output from this command will provide the identifier (ID) that can be used as a raw value to reference the event.

## Runtime Hardware Querying
To ensure compatibility and optimal performance measurement, you can probe the specific hardware capabilities at runtime using the `perf::HardwareInfo` class.
This allows you to determine the appropriate counters and features based on the underlying CPU architecture.

&rarr; [See code example](../examples/address_sampling.cpp)

```cpp
#include <perfcpp/hardware_info.h>

if (perf::HardwareInfo::is_intel()) {
  /// Add intel-specifics like events, etc.

  if (perf::HardwareInfo::is_intel_aux_counter_required()) {
    /// Add the "mem-loads-aux" event in front of precise memory events.
    /// See the sampling documentation for specifics.
  }
}

if (perf::HardwareInfo::is_amd()) {
    /// Add amd-specifics like events, etc.
    
    if (perf::HardwareInfo::is_amd_ibs_supported()) {
        /// You can use ibs_op and further AMD IBS-related sampling mechanisms.
        /// See sampling documentation for specifics.
    }
}
```