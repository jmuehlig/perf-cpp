# Built-in and Hardware-specific Performance Events

Each generation of CPUs introduces its own unique performance events. 
To tailor performance measurement across diverse systems, it's essential to utilize the right events for each processor. 
The `perf::CounterDefinition` class plays a crucial role in this, facilitating the management of diverse events to cater to different hardware needs.

Additionally, the library provides a script for retrieving all hardware-specific events.
For more details, see [Retrieving Raw Event Codes](#retrieving-raw-event-codes) section below.

For a primer on existing events specific to Intel hardware, visit the [perfmon website](https://perfmon-events.intel.com/).

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
*perf-cpp* comes with a variety of performance events that are universally applicable across most CPU architectures. 
These are accessible immediately for use:

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

## Incorporating Hardware-Specific Events
All events, their names and configurations, are stored within the `perf::CounterDefinition`.
This class is passed to `EventCounter` and `Sampler` instances as a reference–consequentially, the instance **must be alive throughout the entire monitoring phase**.

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

For the most time, the event codes are specific to the underlying hardware. 
See below, how you can retrieve the event codes for your system: [Retrieving Raw Event Codes](#retrieving-raw-event-codes).

### Through Configuration Files
Alternatively, hardware-specific events can be added via a CSV-like configuration file, specifying each event's name and configuration details. 
This method facilitates the bulk addition of events. 
For example:

```cpp
auto counter_definition = perf::CounterDefinition{"events.csv"};
```

The CSV file `events.csv` could look like the following:

```csv
cycle_activity.stalls_l1d_miss,0xc530ca3
cycle_activity.stalls_l2_miss,0x55305a3
cycle_activity.stalls_l3_miss,0x65306a3
```

## Using Newly Added Events
Once defined, either through code or configuration files, these events can be incorporated into your performance measurements:

```cpp
auto counter_definitions = perf::CounterDefinition{"events.csv"};
auto event_counter = perf::EventCounter{counter_definitions};
event_counter.add("cycles", "cycle_activity.stalls_l1d_miss");
```

## Retrieving Raw Event Codes
### Automated Retrieval
A Python script (`script/create_perf_list.py`) included in the library helps automate the process of fetching hardware event values, similar to the `perf list` command. 
Execute the script as follows to obtain a comprehensive list of events:

```bash
cmake .
cmake --build . --target perf-list
```

This command generates a CSV file named `perf-list.csv`.
This file includes the names of all performance events along with their raw values, which are extracted from the system's underlying hardware.
The file can be passed to the `perf::CounterDefinition` as demonstrated [above](#through-configuration-files).


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
To ensure compatibility and optimal performance measurement, you can probe the specific hardware capabilities at runtime using the `perf::HardwareInfo` class, which helps determine the necessary counters and features to use based on the underlying CPU architecture.


&rarr; [See code example](../examples/address_sampling.cpp)
```cpp
#include <perfcpp/hardware_info.h>

if (HardwareInfo::is_intel()) {
  /// Add intel-specifics like events, etc.

  if (HardwareInfo::is_intel_aux_counter_required()) {
    /// Add the "mem-loads-aux" event in front of precise memory events.
    /// See the sampling documentation for specifics.
  }
}

if (HardwareInfo::is_amd()) {
    /// Add amd-specifics like events, etc.
    
    if (HardwareInfo::is_amd_ibs_supported()) {
        /// You can use ibs_op and further AMD IBS-related sampling mechanisms.
        /// See sampling documentation for specifics.
    }
}
```