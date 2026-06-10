# Perf Paranoid

The `perf_event_paranoid` setting in `/proc/sys/kernel/perf_event_paranoid` controls which performance monitoring features are available to users without `CAP_PERFMON` (or `CAP_SYS_ADMIN` before Linux `5.8`):

| Value  | Access Level                                                                                      |
|--------|----------------------------------------------------------------------------------------------------|
| `-1`   | No restrictions (full access).                                                                    |
| `0`    | Allow per-process and system-wide profiling, but no raw tracepoints.                              |
| `1`    | Allow per-process user- and kernel-level profiling, but no system-wide (per-CPU) events (default before Linux `4.6`). |
| `>= 2` | Only per-process user-level measurements allowed (default since Linux `4.6`).                     |

Some distribution kernels (e.g., Ubuntu and Android) add levels `3` and above that disallow `perf_event_open` for unprivileged users entirely.

If the setting is too restrictive, opening counters fails with an error like:

    Cannot open perf counter (error no 13): insufficient access rights to start the counter,
    e.g., profiling a not user-owned process or perf_event_paranoid value too high

To resolve this, either [lower the paranoid level](#setting-the-perf-paranoid-value) or [adjust your monitoring configuration](#adjusting-monitoring-configuration).

---

## Setting the Perf Paranoid Value

**Temporarily** (until reboot):

```bash
sudo sysctl -w kernel.perf_event_paranoid=-1
```

**Persistently**, add to `/etc/sysctl.conf`:

```
kernel.perf_event_paranoid = -1
```

Then apply:

```bash
sudo sysctl --system
```

---

## Adjusting Monitoring Configuration

By default, *perf-cpp* records kernel and hypervisor activity alongside user-level activity. At paranoid level `2` or higher, the kernel rejects this for unprivileged users, so opening counters fails even for plain events like `instructions`. If you cannot lower the paranoid level, disable kernel- and hypervisor-level measurements instead:

```cpp
auto config = perf::Config{};
config.include_kernel(false);       /// Disable kernel event recording.
config.include_hypervisor(false);   /// Disable hypervisor event recording.

auto event_counter = perf::EventCounter{ config };
event_counter.add({"instructions", "cycles"});

event_counter.start(); /// Will only record user-level events.
```

To also exclude guest VM events:

```cpp
config.include_guest(false);
```
