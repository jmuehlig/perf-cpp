# Perf Paranoid

The `perf_event_paranoid` setting in `/proc/sys/kernel/perf_event_paranoid` controls access to performance monitoring features:

| Value  | Access Level                                                         |
|--------|----------------------------------------------------------------------|
| `-1`   | No restrictions (full access).                                       |
| `0`    | Allow normal users access, but no raw tracepoint samples.            |
| `1`    | Allow user and kernel-level profiling (default before Linux `4.6`).  |
| `>= 2` | Only user-level measurements allowed (**default since Linux** `4.6`). |

If the setting is too restrictive, you may encounter errors like:

    Cannot open perf counter: insufficient access rights to start the counter,
    e.g., profiling a not user-owned process or perf_event_paranoid value too high.

This can be resolved by [lowering the paranoid level](#setting-the-perf-paranoid-value) or [adjusting your monitoring configuration](#adjusting-monitoring-configuration).

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

If you cannot modify the paranoid level, disable kernel/hypervisor-level measurements to allow profiling under restrictive settings (`>= 2`):

```cpp
auto config = perf::Config{};
config.include_kernel(false);       /// Disable kernel event recording.
config.include_hypervisor(false);   /// Disable hypervisor event recording.

auto event_counter = perf::EventCounter{ config };
event_counter.add({"instructions", "cycles"});

event_counter.start(); /// Will only record user-level events.
```

To further restrict monitoring, exclude guest events:

```cpp
config.include_guest(false);
```
