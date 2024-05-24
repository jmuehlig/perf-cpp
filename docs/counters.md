# Built-in and hardware-specific performance counters

Each CPU generation comes equipped with its own set of performance counters. 
If you're looking to measure your code's performance across various machines, you might need to incorporate different counters. 
The `perf::CounterDefinition` class facilitates the addition and access of diverse counters to accommodate this need.

## Built-in counters
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
    cgroup-switches
    cpu-migrations
    migrations


## Adding hardware-specific performance counters
Basically, there are two options to add more counters:

### 1) In-code
The `perf::CounterDefinition` interface offers an `add()` function that takes
* the name of the counter,
* the type of that counter (e.g., `PERF_TYPE_RAW` or `PERF_TYPE_HW_CACHE`),
* and the config id

Example:
```cpp
auto counter_definitions = perf::CounterDefinition{};
counter_definitions.add("cycle_activity.stalls_l3_miss", PERF_TYPE_RAW, 0x65306a3);
```

or

```cpp
auto counter_definitions = perf::CounterDefinition{};
counter_definitions.add("llc-load-misses", PERF_TYPE_HW_CACHE, PERF_COUNT_HW_CACHE_LL | (PERF_COUNT_HW_CACHE_OP_READ << 8) | (PERF_COUNT_HW_CACHE_RESULT_MISS << 16));
```

### 2) Using a file
Alternatively, you can use a file in csv-like format with `name,config id` in every line. 
Example:

    cycle_activity.stalls_l1d_miss,0xc530ca3
    cycle_activity.stalls_l2_miss,0x55305a3
    cycle_activity.stalls_l3_miss,0x65306a3

These counters will be added as `PERF_TYPE_RAW` with the given `config id`.
The file needs to be provided to `perf::CounterDefinition` when creating it:
```cpp
auto counter_definition = perf::CounterDefinition{"my_file.csv"};
```

## Recording the new counters
In both scenarios, the counters can be added to the `perf::EventCounter` instance:
```cpp
auto counter_definitions = perf::CounterDefinition{"my_file.csv"};
counter_definitions.add("llc-load-misses", PERF_TYPE_HW_CACHE, ...);

/// Note, that the (now defined) counter "llc-load-misses" must be also added to the perf instance.
auto event_counter = perf::EventCounter{counter_definitions};
event_counter.add({"instructions", "cycles", "llc-load-misses"});

/// start, compute, and stop...

const auto result = event_counter.result();
const auto llc_misses = result.get("llc-load-misses");
```

## How to get _raw_ counter codes?
The easiest way is to use the [libpfm4 (see on GitHub)](https://github.com/wcohen/libpfm4):
* Download library
* Build executables in `examples/` folder
* Choose a specific counter (from `perf list` on your machine) and get the code using the compiled `check_events` executable:
```
./check_events cycle_activity.stalls_l3_miss
```

The output will guide you the id that can be used as a _raw_ value for the counter.