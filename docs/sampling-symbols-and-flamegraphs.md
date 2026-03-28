# Symbols and Flamegraphs

A flamegraph ([example](https://www.brendangregg.com/flamegraphs.html)) collapses thousands of sampled call stacks into a single interactive SVG where the widest bars show the functions that consume the most CPU time.

*perf-cpp* provides two building blocks:

- **Symbol resolution**: translate instruction pointers into `<module>::<function>+<offset>` strings.
- **Collapsed-stack export**: emit samples in the `func1;func2;func3 <count>` format understood by [FlameGraph](https://github.com/brendangregg/FlameGraph), [Speedscope](https://www.speedscope.app/), or [flamegraph.com](https://flamegraph.com/).

---

## Translating Instruction Pointers into Symbols

`perf::util::SymbolResolver` translates logical instruction pointers into symbols (module name, function name, and offset):

```cpp
#include <perfcpp/sampler.hpp>
#include <perfcpp/util/symbol_resolver.hpp>

auto sampler = perf::Sampler{};
sampler.trigger("cycles", perf::Precision::RequestZeroSkid, perf::Period{ 50000U });
sampler.values().logical_instruction_pointer(true);

sampler.start();
/// ... computation here ...
sampler.stop();

auto symbol_resolver = perf::util::SymbolResolver{};

for (const auto& sample : sampler.result()) {
    const auto instruction_pointer = sample.instruction_execution().logical_instruction_pointer();
    if (instruction_pointer.has_value()) {
        const auto symbol = symbol_resolver.resolve(instruction_pointer.value());
        const auto symbol_name = symbol.has_value() ? symbol->to_string() : std::string{"??"};

        std::cout << "IP = 0x" << std::hex
                  << instruction_pointer.value() << std::dec
                  << " | Symbol = " << symbol_name << "\n";
    }
}

/// Release resources explicitly, or let the destructor handle it.
sampler.close();
```

The output could look like the following:

```
IP = 0x57459be95faf | Symbol = [instruction-pointer-sampling] _ZNK4perf7example15AccessBenchmarkixEm+47
IP = 0x57459be95faf | Symbol = [instruction-pointer-sampling] _ZNK4perf7example15AccessBenchmarkixEm+47
IP = 0x57459be987d0 | Symbol = [instruction-pointer-sampling] _ZNKSt6vectorIN4perf7example15AccessBenchmark10cache_lineESaIS3_EEixEm+0
```

> [!TIP]
> See the example: **[instruction_pointer.cpp](https://github.com/jmuehlig/perf-cpp/tree/dev/examples/sampling/instruction_pointer.cpp)**.

---

## Generating Flamegraphs

To generate flamegraphs, record the instruction pointer and callchain. Including the timestamp and sorting the results produces more condensed output.

```cpp
#include <perfcpp/sampler.hpp>

auto sampler = perf::Sampler{};
sampler.trigger("cycles");
sampler.values()
    .logical_instruction_pointer(true)
    .callchain(true)
    .timestamp(true);

sampler.start();
/// ... computation here ...
sampler.stop();

/// Get samples and sort for condensed output (sorting is optional).
const auto samples = sampler.result(/* sort = */ true);

/// Write collapsed stacks to a file.
samples.to_flamegraphs("flamegraphs.txt");

/// Release resources explicitly, or let the destructor handle it.
sampler.close();
```

The output file can be fed into common flamegraph generators:

- [Brendan Gregg's FlameGraph](https://github.com/brendangregg/FlameGraph): `./flamegraph.pl flamegraphs.txt > flamegraphs.svg`
- [flamegraph.com](https://flamegraph.com/): Upload `flamegraphs.txt`
- [Speedscope](https://www.speedscope.app/): Upload `flamegraphs.txt`

> [!TIP]
> See the example: **[flame_graph.cpp](https://github.com/jmuehlig/perf-cpp/tree/dev/examples/sampling/flame_graph.cpp)**.
