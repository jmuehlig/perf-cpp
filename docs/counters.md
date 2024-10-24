# Built-in and Hardware-specific Performance Counters

Each CPU generation comes equipped with its own set of performance events. 
If you're looking to measure your code's performance across various machines, you might need to incorporate different events. 
The `perf::CounterDefinition` class facilitates the addition and access of diverse events to accommodate this need.

&rarr; This library also ships a script to read all hardware-specific events, see [**How to get _raw_ counter codes?**](#how-to-get-raw-counter-codes) below.

To get an idea of existing events on Intel hardware, you can also see [the perfmon website](https://perfmon-events.intel.com/).

---
## Table of Contents
- [Built-in Counters](#built-in-counters)
- [Adding Hardware-specific Performance Counters](#adding-hardware-specific-performance-counters)
   - [1) In-code](#1-in-code)
   - [2) Using a file](#2-using-a-file)
- [Recording added Counters](#recording-added-counters)
- [How to get Raw Counter Codes?](#how-to-get-raw-counter-codes)
   - [Automatically](#automatically)
   - [Manual Configuration with libpfm4](#manual-configuration-with-libpfm4)
- [Querying the Hardware at Runtime](#querying-the-hardware-at-runtime)
---

## Built-in Counters
Several performance counters, common across most CPUs, are pre-defined by the library and ready for immediate use out-of-the-box (see `src/counter_definition.cpp` for details):

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
    bpf-output
    cgroup-switches     # only since Linux Kernel 5.13
    cpu-migrations
    migrations


## Adding Hardware-specific Performance Counters
All counters, their names and configurations, are stored within the `perf::CounterDefinition`.
This class is passed to all event counter instances as a reference – consequentially, the instance **must be alive throughout the entire monitoring phase**.

Basically, there are two options to add more counters: defining counters in-code and using an external file.

&rarr; See paragraph [**How to get raw counter codes?**](#how-to-get-raw-counter-codes) below for instructions to get all hardware-counters.

### 1) In-code
The `perf::CounterDefinition` interface offers an `add()` function that takes
* the name of the counter,
* the type of that counter (e.g., `PERF_TYPE_HW_CACHE`, `PERF_TYPE_RAW` by default),
* and the config id

Example:
```cpp
auto counter_definitions = perf::CounterDefinition{};
counter_definitions.add(
  /* name */ "cycle_activity.stalls_l3_miss",
  /* config id */ 0x65306a3
);
```

or

```cpp
auto counter_definitions = perf::CounterDefinition{};
counter_definitions.add(
  /* name */ "llc-load-misses", 
  /* type */ PERF_TYPE_HW_CACHE, 
  /* config id */ PERF_COUNT_HW_CACHE_LL | (PERF_COUNT_HW_CACHE_OP_READ << 8) | (PERF_COUNT_HW_CACHE_RESULT_MISS << 16)
);
```

You can also use the `perf::CounterConfig` class, which takes extended configs:

```cpp
auto counter_definitions = perf::CounterDefinition{};
counter_definitions.add(
  /* name */ "mem-loads-lat-3", 
  perf::CounterConfig{
      /* type */ PERF_TYPE_RAW,
      /* config id */ 0x1CD,
      /* config2 */ 0x3,
      /* config3 */ 0x0
  }
);
```

### 2) Using a file
Alternatively, you can use a file in csv-like format with `name,config id[,config2]` in every line. 
Example:

    cycle_activity.stalls_l1d_miss,0xc530ca3
    cycle_activity.stalls_l2_miss,0x55305a3
    cycle_activity.stalls_l3_miss,0x65306a3

These counters will be added as `PERF_TYPE_RAW` with the given `config id`.
The file needs to be provided to `perf::CounterDefinition` when creating it:
```cpp
auto counter_definition = perf::CounterDefinition{"my_file.csv"};

auto event_counter = perf::EventCounter{counter_definitions};
event_counter.add({"cycle_activity.stalls_l1d_miss", "cycle_activity.stalls_l2_miss"});
```

## Recording added Counters
In both scenarios, the counters can be added to the `perf::EventCounter` instance:
```cpp
auto counter_definitions = perf::CounterDefinition{"my_file.csv"};
counter_definitions.add("llc-load-misses", PERF_TYPE_HW_CACHE, ...);

/// Note, that the (now defined) counter "llc-load-misses" must be also added to the perf instance.
auto event_counter = perf::EventCounter{counter_definitions};
event_counter.add({"instructions", "cycles", "cycle_activity.stalls_l3_miss", "llc-load-misses"});

/// start, compute, and stop...

const auto result = event_counter.result();
const auto llc_misses = result.get("llc-load-misses");
```

## How to get Raw Counter Codes?
### Automatically
This library includes a Python script (`script/create_perf_list.py`) designed to streamline the extraction of hardware counter values, producing an output similar to the `perf list` command.

#### Executing
To execute the script, use the following Make commands:

    cmake .
    cmake --build . --target perf-list

This command generates a CSV file named `perf-list.csv`.
This file includes the names of all performance counters along with their raw values, which are extracted from the system's underlying hardware.

#### Using the counter list `perf-list.csv`
You can use the resulting CSV file with the `perf::CounterDefinition` class as follows:

```cpp
auto counter_definitions = perf::CounterDefinition{"perf-list.csv"};
```

This allows your application to access all the counters listed in the CSV file.

#### Script details
The script operates by downloading the [libpfm4 library](https://github.com/wcohen/libpfm4) and extracting all reported counters specific to the hardware it is run on.


### Manual Configuration with libpfm4
For manual setup using **libpfm4** (available on [GitHub](https://github.com/wcohen/libpfm4)), follow these steps:

1. Clone or download the libpfm4 repository from [GitHub](https://github.com/wcohen/libpfm4).
2. Call `make` to build all binaries.
3. Navigate to the `examples/` directory within the downloaded
4. Select and Check a Specific Counter:
    * Identify a performance counter of interest on your machine by using the perf list command.
    * Retrieve the specific code for this counter by running the check_events executable with the counter's name as an argument. For example: `./check_events cycle_activity.stalls_l3_miss`
    * The output from this command will provide the identifier (ID) that can be used as a raw value to reference the counter.

## Querying the Hardware at Runtime
When you are writing cross-platform code, you might decide on the underlying hardware, which counters to add.
To that end, you can query the `HardwareInfo` class as follows:

```cpp
#include <perfcpp/hardware_info.h>

if (HardwareInfo::is_intel()) {
  /// Add intel-specifics like counters, etc.

  if (HardwareInfo::is_intel_aux_counter_required()) {
    /// Add the "mem-loads-aux" counter in front of precise memory counters.
    /// See the sampling documentation for specifics.
  }
}

if (HardwareInfo::is_amd()) {
    /// Add amd-specifics like counters, etc.
    
    if (HardwareInfo::is_amd_ibs_supported()) {
        /// You can use ibs_op and further AMD IBS-related sampling mechanisms.
        /// See sampling documentation for specifics.
    }
}
```