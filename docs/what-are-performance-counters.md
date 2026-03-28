# What are Hardware Performance Counters?

Modern CPUs contain dedicated hardware registers — **performance monitoring counters (PMCs)** — that track low-level events as your code executes.
These counters run in hardware with virtually zero overhead, giving you precise insights that software-only profiling cannot provide.

## What do they measure?

Every CPU core has a small number of programmable counters (typically 4–8).
Each counter can be configured to count one event type at a time:

- **Instructions retired**: how many instructions actually completed
- **CPU cycles**: wall-clock time at the core level
- **Cache misses**: L1, L2, L3, at different levels of the hierarchy
- **Branch mispredictions**: wrong guesses by the branch predictor
- **TLB misses**: virtual-to-physical address translation failures
- **Memory accesses**: loads, stores, prefetches, and where data came from (L1, L2, RAM, remote NUMA node)

Beyond these common events, each CPU generation adds vendor-specific counters.
Intel and AMD publish thousands of events per microarchitecture, from micro-op queue stalls to specific cache coherency transitions.

## Counting vs. Sampling

There are two fundamentally different ways to use these counters:

**Counting** reads the counter registers after a region of code has executed.
You get totals: *"this loop executed 4.7 billion cycles and had 13 million cache misses."*
Counts are exact (no sampling error) but tell you nothing about *which* instructions caused those events.

**Sampling** captures snapshots at regular intervals.
Every *N* events (e.g., every 50,000 cycles), the CPU interrupts and records context about that moment — the instruction pointer, memory address, timestamp, cache level, and more.
This gives you a statistical picture of *where* events are concentrated, but since only every *N*-th event triggers a sample, it's an approximation — not every instruction is observed.

## How many counters are available?

Each physical core has a fixed number of counter registers.
Typical values:

| Architecture | General-purpose counters | Fixed counters |
|---|---|---|
| Intel (recent) | 4–8 | 3–4 (cycles, instructions, ref-cycles) |
| AMD (Zen 3+) | 6 | 0 |

If you request more events than physical counters, the kernel **multiplexes**: it time-shares the counters and scales the results.
Multiplexed counts are estimates, not exact — so fewer simultaneous events means more accurate data.

*perf-cpp* [detects your hardware's counter layout automatically](recording.md#adjusting-hardware-settings-to-the-underlying-system) and manages multiplexing transparently.

## Further reading

Introductory articles on hardware performance counters and how to work with them:

- **[PMU Counters and Profiling Basics](https://easyperf.net/blog/2018/06/01/PMU-counters-and-profiling-basics)** (Denis Bakhvalov): beginner-friendly walkthrough of what PMU counters are and how CPUs expose them
- **[Developing Intuition when Working with Performance Counters](https://easyperf.net/blog/2019/07/26/Developing-intuition-when-working-with-performance-counters)** (Denis Bakhvalov): how to interpret counter values and spot common patterns
- **[Performance Analysis and Tuning on Modern CPUs](https://book.easyperf.net/perf_book)** (Denis Bakhvalov): free book covering PMU fundamentals through practical optimization
- **[Hardware Performance Counters the Easy Way](https://johnnysswlab.com/hardware-performance-counters-the-easy-way-quickstart-likwid-perfctr/)** (Johnny's Software Lab): practical intro to reading counters, including multiplexing and pitfalls
- **[Linux perf Examples](https://www.brendangregg.com/perf.html)** (Brendan Gregg): comprehensive guide to Linux `perf` with visual diagrams of the hardware model
- **[perf Wiki Tutorial](https://perfwiki.github.io/main/tutorial/)**: step-by-step introduction to profiling with `perf`
- **[Hardware Performance Counter](https://en.wikipedia.org/wiki/Hardware_performance_counter)** (Wikipedia): concise conceptual overview of PMUs and counter registers
