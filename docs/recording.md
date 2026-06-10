# Recording Hardware Events

Record hardware performance counters for specific code regions using `perf::EventCounter`.

> [!NOTE]
> `EventCounter` monitors a single thread. For multi-threaded or multi-core recording, use `MultiThreadEventCounter`, `MultiCoreEventCounter`, or `MultiProcessEventCounter` (see [parallel recording](recording-parallel.md)).

> [!TIP]
> See **[single_thread.cpp](https://github.com/jmuehlig/perf-cpp/tree/dev/examples/statistics/single_thread.cpp)** for a full working example.

---

## Basic Lifecycle

Set up an event counter, wrap your code with `start()` / `stop()`, and retrieve the results:

```cpp
#include <perfcpp/event_counter.hpp>

/// Create the counter and add events.
auto event_counter = perf::EventCounter{};
event_counter.add({"instructions", "cycles", "branches", "cache-misses"});

/// Optionally, open counters ahead of time to exclude setup from measurement.
event_counter.open();

/// Measure.
event_counter.start();
/// ... your code here ...
event_counter.stop();

/// Retrieve results.
const auto result = event_counter.result();
```

After `stop()`, you can call `start()` / `stop()` again without re-adding events.

```cpp
/// Release resources explicitly, or let the destructor handle it.
event_counter.close();
```

### Accessing Results

```cpp
/// Query a specific event.
const auto cycles = result.get("cycles");
std::cout << "Took " << cycles.value() << " cycles" << std::endl;

/// Iterate over all results.
for (const auto [name, value] : result)
{
    std::cout << name << " = " << value << std::endl;
}

/// Print as formatted table.
std::cout << result.to_string() << std::endl;

/// Export as CSV or JSON, to string or to file.
std::cout << result.to_csv() << std::endl;
std::cout << result.to_json() << std::endl;
result.to_csv("results.csv");
result.to_json("results.json");
```

## Scheduling Events to Hardware Counters

Physical hardware counters are limited (typically 4–8 per core).
When you request more events than counters, the kernel **multiplexes**: it time-shares counters and scales results.

By default, *perf-cpp* packs events into as few counters as possible.
You can control this via a scheduling hint in `add()`:

```cpp
event_counter.add({"instructions", "cycles", "branches"},
                  perf::EventCounter::Schedule::Separate);
```

| Schedule Mode | Description |
|---|---|
| `Schedule::Append` | Pack into any counter, using multiplexing. **Default.** |
| `Schedule::Separate` | One event per physical counter; avoids multiplexing. |
| `Schedule::Group` | Force all listed events onto the **same** counter (multiplexed together). |

`add()` throws if the requested scheduling doesn't fit (e.g., too many events to group).

### Fixed-Function Performance Counters (Intel)

On Intel processors, `instructions`, `cycles`, `cpu-cycles`, and `ref-cycles` are backed by dedicated fixed-function hardware counters rather than the general-purpose PMCs.
*perf-cpp* detects this automatically and schedules these events into their own pinned groups; they are never multiplexed and do not consume a generic PMC slot.

This means you can measure fixed events alongside a full set of generic events without any scheduling penalty:

```cpp
/// On Intel: instructions, cycles, and ref-cycles go to fixed-function PMCs;
/// cache-misses and branch-misses use the generic PMC budget as usual.
event_counter.add({"instructions", "cycles", "ref-cycles", "cache-misses", "branch-misses"});
```

## Binding to a CPU Core or Process

By default, events are counted for the calling thread only, on every core it runs on.

```cpp
auto config = perf::Config{};

/// Count only on CPU core 5.
config.cpu_core(5U);
config.cpu_core(perf::CpuCore::Any); /// Revert to counting on all cores.

/// Monitor a specific process or all processes.
config.process(perf::Process{1337});
config.process(perf::Process::Any);

auto event_counter = perf::EventCounter{ config };
```

> [!NOTE]
> Monitoring other or all processes may require elevated privileges.
> See the [perf paranoid setting](perf-paranoid.md).

> [!TIP]
> Some hardware events (e.g., Intel off-core events) require monitoring all processes on a specific CPU core, as the hardware does not attribute these events to individual processes.

## Monitoring a cgroup (Container)

Instead of targeting a process or thread by PID, you can monitor all tasks belonging to a **cgroup**, the Linux kernel primitive that container runtimes (Docker, Kubernetes, systemd) build on.
The kernel counts events only when a task from the designated cgroup is running on the monitored CPU.

Pass a `perf::CGroupMonitor` to `Config::cgroup()` instead of a process:

```cpp
auto config = perf::Config{};

/// Open by path.
config.cgroup(std::filesystem::path{ "/sys/fs/cgroup/my-container" });

/// Open by name (expands to /sys/fs/cgroup/{name}).
config.cgroup("my-container");

/// From a raw fd opened elsewhere in the application: wrap it in a UniqueFileDescriptor
/// to pass ownership. The descriptor is closed once the last copy of the monitor is destroyed.
config.cgroup(perf::CGroupMonitor{ perf::util::UniqueFileDescriptor{ raw_fd } });
```

The cgroup directory path is opened with `O_RDONLY` and the resulting file descriptor is passed to `perf_event_open` via `PERF_FLAG_PID_CGROUP`.
A `perf::CannotOpenCGroupError` is thrown if the path cannot be opened.

> [!IMPORTANT]
> Cgroup monitoring is **system-wide** and always requires a specific CPU core; `CpuCore::Any` is not permitted.
> Use `config.cpu_core(N)` to pin to a core.
> To monitor across all cores, use `MultiCoreEventCounter` with a `CGroupMonitor` config.

```cpp
config.cpu_core(0U); /// Required: cgroup monitoring is per CPU core.

auto event_counter = perf::EventCounter{ config };
event_counter.add({"instructions", "cycles", "cache-misses"});

event_counter.start();
/// ... workload running inside the cgroup ...
event_counter.stop();
```

To monitor the calling process's own cgroup (e.g., for testing), read its path from `/proc/self/cgroup`:

```cpp
/// cgroupv2: lines start with "0::", the relative path follows.
auto file = std::ifstream{ "/proc/self/cgroup" };
auto line  = std::string{};
while (std::getline(file, line)) {
    if (line.rfind("0::", 0) == 0) {
        config.cgroup(perf::CGroupMonitor{ std::filesystem::path{ "/sys/fs/cgroup" + line.substr(3) } });
        break;
    }
}
```

> [!NOTE]
> Cgroup monitoring requires `CAP_PERFMON` (kernel ≥ 5.8) or `perf_event_paranoid ≤ 0`.
> See [perf paranoid](perf-paranoid.md).
> Grant the capability to a binary without running as root:
> ```bash
> sudo setcap cap_perfmon+ep ./my-program
> ```

> [!TIP]
> See **[cgroup.cpp](https://github.com/jmuehlig/perf-cpp/tree/dev/examples/statistics/cgroup.cpp)** for a full working example.
> Run it with `taskset -c 0` to pin the workload to the monitored CPU:
> ```bash
> taskset -c 0 ./examples/bin/cgroup-statistics
> ```

## Detection of Physical Hardware Counters

*perf-cpp* automatically detects the number of physical counters and multiplexing capabilities on most systems.
On Intel processors, fixed-function PMCs (typically 3: `instructions`, `cycles`, `ref-cycles`) are detected separately via CPUID and do not reduce the available generic PMC slots.

> [!IMPORTANT]
> If the NMI watchdog is enabled (`cat /proc/sys/kernel/nmi_watchdog` returns `1`), it permanently consumes one hardware counter.
> *perf-cpp* detects this and adjusts automatically.
> To reclaim the counter, disable the watchdog via `echo 0 > /proc/sys/kernel/nmi_watchdog` (requires root).

For unusual hardware where auto-detection fails, specify limits manually:

```cpp
auto config = perf::Config{};
config.num_physical_counters(2U);
config.num_events_per_physical_counter(1U);
auto event_counter = perf::EventCounter{ config };
```

## Further Configuration

| Setting | Default | Description |
|---|---|---|
| `include_child_threads(bool)` | `false` | Also monitor child threads spawned by the recording thread. |
| `include_kernel(bool)` | `true` | Include events from kernel activity. Disable when only user-space matters or [perf paranoid](perf-paranoid.md) restricts access. |
| `include_user(bool)` | `true` | Include events from user-space activity. |
| `include_hypervisor(bool)` | `true` | Include events from hypervisor activity. |
| `include_idle(bool)` | `true` | Include events during CPU idle periods. |
| `include_guest(bool)` | `true` | Include events from guest (VM) activity. |
| `include_host(bool)` | `true` | Include events from host activity. |
| `pinned(bool)` | `false` | Pin events to the CPU, preventing them from being multiplexed off. |

---

## Troubleshooting Counter Configurations

Enable debug output to inspect the counter configuration passed to the kernel:

```cpp
auto config = perf::Config{};
config.debug(true);
auto event_counter = perf::EventCounter{ config };
```

The output resembles that of `perf --debug perf-event-open stat -- sleep 1` and shows the `perf_event_open` arguments for each counter.
It helps with retrieving event codes and diagnosing why a counter fails to open.

---

## Example: Random vs. Sequential Access

This example measures how unpredictable memory access patterns defeat the hardware prefetcher:

```cpp
#include <random>
#include <iostream>
#include <cstdint>
#include <vector>
#include <algorithm>
#include <numeric>
#include <perfcpp/event_counter.hpp>

/// One cache line per element.
struct alignas(64U) cache_line { std::int64_t value; };

int main()
{
    auto event_counter = perf::EventCounter{};
    event_counter.add({"instructions", "cycles", "cache-misses", "cycles-per-instruction"});

    /// 256 MB of cache lines.
    auto cache_lines = std::vector<cache_line>{};
    cache_lines.resize((1024U * 1024U * 256U) / sizeof(cache_line));
    for (auto i = 0U; i < cache_lines.size(); ++i)
    {
        cache_lines[i].value = i;
    }

    /// Shuffle indices for random access.
    auto indices = std::vector<std::uint64_t>(cache_lines.size());
    std::iota(indices.begin(), indices.end(), 0U);
    std::shuffle(indices.begin(), indices.end(), std::mt19937{std::random_device{}()});

    /// Measure random access.
    event_counter.start();
    auto value = 0ULL;
    for (const auto index : indices)
    {
        value += cache_lines[index].value;
    }
    asm volatile("" : "+r,m"(value) : : "memory");
    event_counter.stop();

    /// Print per-cache-line results.
    const auto result = event_counter.result(cache_lines.size());
    for (const auto [name, val] : result)
    {
        std::cout << val << " " << name << " per cache line" << std::endl;
    }

    event_counter.close();
}
```

With random access, every cache line costs more than one miss:
```
7.12 instructions per cache line
57.19 cycles per cache line
1.63 cache-misses per cache line
8.03 cycles-per-instruction per cache line
```

With sequential access (no shuffling), the prefetcher eliminates nearly all misses:
```
6.85 instructions per cache line
8.94 cycles per cache line
0.007 cache-misses per cache line
1.31 cycles-per-instruction per cache line
```
