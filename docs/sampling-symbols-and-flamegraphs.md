# Symbols and Flamegraphs
Performance bottlenecks often hide inside deep call stacks: the one slow function that is really stalling your frame rate sits four layers below the code you are looking at. 
A flamegraph ([example](https://www.brendangregg.com/flamegraphs.html)) collapses thousands of sampled call-stacks into a single, interactive SVG where the widest bars show the functions that burn the most CPU time.

*perf‑cpp* now provides two building blocks for flamegraph generation:
- **Symbol resolution**: translate raw instruction pointers into `<module>::<function>+<offset>` strings.
- **Collapsed‑stack export**: emit samples in the canonical `func1;func2;func3 <count>` format understood by tools such as [Brendan Gregg's FlameGraph](https://github.com/brendangregg/FlameGraph), [Speedscope](https://www.speedscope.app/), or [flamegraph.com](https://flamegraph.com/).

With just a few lines of code you can record samples, resolve symbols, and open a browser to an interactive heat‑map of your code.

---
## Table of Contents
- [Translating Instruction Pointers into Symbols](#translating-instruction-pointers-into-symbols)
- [Translating Sampler Results into Flame Graphs](#translating-sampler-results-into-flame-graphs)
  - [Setting up the Sampler](#setting-up-the-sampler)
  - [Generating Flamegraphs](#generating-flamegraphs)
---

## Translating Instruction Pointers into Symbols
The `perf::SymbolResolver` allows to translate logical instruction pointers into symbols (i.e., the name if the module, the name of the function, and the offset within that function).

```cpp
#include <perfcpp/sampler.h>
#include <perfcpp/symbol_resolver.h>

const auto counter_definitions = perf::CounterDefinition{};
auto sampler = perf::Sampler{ counter_definitions };
sampler.trigger("cycles", perf::Precision::RequestZeroSkid, perf::Period{ 4000U });
sampler.values().instruction_pointer(true);

sampler.start();
/// Run some code
sampler.stop();

auto symbol_resolver = perf::SymbolResolver{};

for (const auto& sample : sampler.results()) {
  const auto instruction_pointer =   sample.instruction_execution().logical_instruction_pointer();
  if (instruction_pointer.has_value()) {
      
    /// Resolve the symbol.
    const auto symbol = symbol_resolver.resolve(instruction_pointer.value());
    
    /// Translate the symbol into a string.
    const auto symbol_name = symbol.has_value() ? symbol->to_string() : std::string{"??"};

    std::cout " Instruction Pointer = 0x" << std::hex
              << instruction_pointer.value() << std::dec
              << " | Symbol = " << symbol_name
              << "\n";
  }
}
```

The output could look like the following:

```bash
Instruction Pointer = 0x57459be95faf | Symbol = [instruction-pointer-sampling] _ZNK4perf7example15AccessBenchmarkixEm+47
Instruction Pointer = 0x57459be95faf | Symbol = [instruction-pointer-sampling] _ZNK4perf7example15AccessBenchmarkixEm+47 
Instruction Pointer = 0x57459be987d0 | Symbol = [instruction-pointer-sampling] _ZNKSt6vectorIN4perf7example15AccessBenchmark10cache_lineESaIS3_EEixEm+0
```

**&rarr; [See a practical example](../examples/sampling/instruction_pointer.cpp)**

### Translating Sampler Results into Flame Graphs

#### Setting up the Sampler
To generate flamegraphs, we need include 
- the (logical) *instruction pointer* (to identify the leaf frame) 
- and the *callchain* (to reconstruct the stack)

into samples.
For more condensed outputs, it is also recommended to include the *timestamp* and sort the results afterward.

```cpp
#include <perfcpp/sampler.h>
const const auto counter_definitions = perf::CounterDefinition{};

auto sampler = perf::Sampler{ counter_definitions, sample_config };
sampler.trigger("cycles");
sampler.values()
    .instruction_pointer(true)
    .callchain(true)
    .timestamp(true);
```

#### Generating Flamegraphs
After sampling, the `perf::analyzer::FlameGraphGenerator` can map the samples into a format that can be read by common used flamegraph generators:

```cpp
#include <perfcpp/analyzer/flame_graph_generator.h>

sampler.start();
/// Code to sample will be called here...
sampler.stop();

/// Get all the recorded samples and sort for condensed outputs 
/// (sorting via `true` flag is optional).
const auto samples = sampler.result(/*sort = */ true);

/// Translate into a frame graph format and write the result to "flagraphs.txt".
auto flame_graph_generator = perf::analyzer::FlameGraphGenerator{};
flame_graph_generator.map(samples, "flamegraphs.txt");
```

After writing the output, we can use that file as an input to flamegraph generators, for example:
- [Brendan Gregg's FlameGraph](https://github.com/brendangregg/FlameGraph): Download the project and translate `flamegraphs.txt` into an SVG via `./flamegraph.pl flamegraphs.txt > flamegraphs.svg`
- [flamegraph.com](https://flamegraph.com/): Upload the `flamegraphs.txt`
- [Speedscope](https://www.speedscope.app/): Upload the `flamegraphs.txt`

**&rarr; [See full example](../examples/sampling/flame_graph.cpp)**