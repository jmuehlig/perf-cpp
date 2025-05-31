# Perf Paranoid

This section explains the *perf paranoid* setting in Linux, which controls access to performance monitoring features and can restrict unprivileged users from using performance counters.

## Understanding Perf Paranoid
The *perf paranoid* level is defined in `/proc/sys/kernel/perf_event_paranoid`, with values ranging from *highly restrictive* to *fully permissive*:

| Value  | Access Level                                                         |
|--------|----------------------------------------------------------------------|
| `-1`   | No restrictions (full access).                                       |
| `0`    | Allow normal users access, but no raw tracepoint samples.            |
| `1`    | Allow user and kernel-level profiling (default before Linux `4.6`).    |
| `>= 2` | Only user-level measurements allowed  (**default since Linux** `4.6`). |


If the setting is too restrictive, you may encounter errors like:

    Cannot open perf counter: insufficient access rights to start the counter, 
    e.g., profiling a not user-owned process or perf_event_paranoid value too high.

This can be resolved either by [modifying the paranoid level](#setting-the-perf-paranoid-value) or [adjusting monitoring settings](#adjusting-monitoring-configuration). 

## Setting the Perf Paranoid Value
To enable full access (if permitted by system policy), you can lower the paranoid value **temporarily** with:

```bash
sudo sysctl -w kernel.perf_event_paranoid=-1
```

For a **persistent** change, add the following line to `/etc/sysctl.conf`:

``` 
kernel.perf_event_paranoid = -1
```

Then apply changes with:

```bash
sudo sysctl --system
```

## Adjusting Monitoring Configuration
If you **cannot modify the paranoid level**, you may still be able to record user-level events only.
Use the `perf::Config` class to **disable kernel/hypervisor-level** measurements, which allows profiling under restrictive `perf_event_paranoid` settings (`>= 2`).

```cpp
const auto counter_definitions = perf::CounterDefinition{};

auto config = perf::Config{};
config.include_kernel(false);       /// Disable kernel event sampling
config.include_hypervisor(false);   /// Disable hypervisor event sampling

auto event_counter = perf::EventCounter{ counter_definitions, config };
event_counter.add(...);

event_counter.start(); /// Will only record user-level events.
```

To further restrict monitoring exclude guest events:

```cpp
config.include_guest(false);
```