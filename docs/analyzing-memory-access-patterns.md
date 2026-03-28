# Analyzing Memory Access Patterns

The **Memory Access Analyzer** maps sampled memory accesses to individual data structure instances, producing per-attribute statistics (cache hits/misses, TLB performance, latency).

This is useful when multiple instances of the same data structure share identical code but exhibit different access patterns — e.g., different nodes within a tree.

> [!TIP]
> See the example: **[memory_access_analyzer.cpp](https://github.com/jmuehlig/perf-cpp/tree/dev/examples/sampling/memory_access_analyzer.cpp)**.

---

## Step 1: Describing Data Types

The analyzer needs a description of your data type's layout. For example, given a binary tree node:

```cpp
class BinaryTreeNode {
    std::uint64_t value;
    BinaryTreeNode* left_child;
    BinaryTreeNode* right_child;
};
```

Create a `perf::analyzer::DataType` definition:

```cpp
#include <perfcpp/analyzer/memory_access.hpp>

auto binary_tree_node = perf::analyzer::DataType{"BinaryTreeNode", sizeof(BinaryTreeNode)};
binary_tree_node.add("value", sizeof(std::uint64_t));         /// Describe the "value" attribute.
binary_tree_node.add("left_child", sizeof(BinaryTreeNode*));  /// Describe the "left_child" attribute.
binary_tree_node.add("right_child", sizeof(BinaryTreeNode*)); /// Describe the "right_child" attribute.
```

> [!TIP]
> For accurate size and offset information, use [**pahole**](https://linux.die.net/man/1/pahole). See [Pramod Kumbhar's guide](https://pramodkumbhar.com/2023/11/pahole-to-analyz-data-structure-memory-layouts-with-ease/) for details.

## Step 2: Registering Data Type Instances

Register individual instances so the analyzer can map sampled addresses to specific objects:

```cpp
auto memory_access_analyzer = perf::analyzer::MemoryAccess{};

/// Expose the data type to the analyzer.
memory_access_analyzer.add(std::move(binary_tree_node));

/// Register each instance by pointer.
for (auto* node : tree->nodes()) {
    memory_access_analyzer.annotate("BinaryTreeNode", node);
}
```

## Step 3: Mapping Samples to Data Type Instances

Sample memory accesses using a memory-capable trigger (`mem-loads` on Intel, `ibs_op` on AMD — see [CPU-specific notes](sampling.md#specific-notes-for-different-cpu-vendors)):

```cpp
#include <perfcpp/sampler.hpp>

auto sampler = perf::Sampler{};
sampler.trigger("mem-loads", perf::Precision::MustHaveZeroSkid, perf::Period{ 1000U });
sampler.values()
    .logical_memory_address(true)
    .data_source(true)
    .data_access_latency(true);

sampler.start();
/// ... computation here ...
sampler.stop();

/// Map samples to registered data types and instances.
const auto samples = sampler.result();
const auto result = memory_access_analyzer.map(samples);

/// Release resources explicitly, or let the destructor handle it.
sampler.close();
```

## Step 4: Processing the Result

```cpp
std::cout << result.to_string() << std::endl;
```

Example output:

```
DataType BinaryTreeNode (24B) {
                                      |     loads      |    cache hits    |   RAM hits    |           TLB            |     stores
                              samples | count  latency | L1d  LFB  L2  L3 | local  remote | L1 hits  L2 hits  misses | count  latency
      0:   value (8B)             373 |   373      439 | 154    0   0   7 |   212       0 |     190        5     178 |     0        0
      8:   left_child (8B)        146 |   146      720 |   1    0   0   5 |   140       0 |      12       18     116 |     0        0
     16:   right_child (8B)       528 |   528      173 | 393    0   1  14 |   120       0 |     415        4     109 |     0        0
}
```

For structured export:

```cpp
result.to_json();  /// JSON format.
result.to_csv();   /// CSV format.
```
