# Analyzing Memory Access Patterns of Data Structures

Modern applications often contain multiple instances of complex data structures, making it challenging to analyze their memory access patterns. 
While tools like Linux Perf and Intel VTune excel at identifying resource-intensive instructions, they cannot differentiate between different instances of the same data structure sharing identical code - for example, different nodes within a tree structure experiencing varying access patterns.

*perf-cpp* addresses this limitation through its **Memory Access Analyzer** component, which works in conjunction with memory-based sampling ([detailed in the sampling documentation](sampling.md)). 
The Memory Access Analyzer helps identify which specific memory addresses experience high access latency by:

* Mapping samples to individual data object instances
* Generating detailed access statistics including cache hits/misses, TLB performance, and average latency metrics

&rarr; [For a practical implementation, check out our random-access-benchmark example.](../examples/sampling/memory_access_analyzer.cpp)

---
## Table of Contents
- [Describing Data Types](#describing-data-types)
- [Registering Data Type Instances](#registering-data-type-instances)
- [Mapping Samples to Data Type Instances](#mapping-samples-to-data-type-instances)
- [Processing the Result](#processing-the-result)
---

## Step 1: Describing Data Types
The **Memory Access Analyzer** requires information about the structure of your data types. 
Let's walk through an example using a binary tree node:
```cpp
class BinaryTreeNode {
    std::uint64_t value;
    BinaryTreeNode* left_child;
    BinaryTreeNode* right_child;
};
```

To analyze this structure, create a `perf::analyzer::DataType` definition:

```cpp
#include <perfcpp/analyzer/memory_access.h>

auto binary_tree_node = perf::analyzer::DataType{"BinaryTreeNode", sizeof(BinaryTreeNode)};
binary_tree_node.add("value", sizeof(std::uint64_t));         /// Describe the "value" attribute.
binary_tree_node.add("left_child", sizeof(BinaryTreeNode*));  /// Describe the "left_child" attribute.
binary_tree_node.add("right_child", sizeof(BinaryTreeNode*)); /// Describe the "right_child" attribute.
```

> [!TIP]
> For accurate size and offset information, you can use [**pahole**](https://linux.die.net/man/1/pahole). See [Paramoud Kumbhar's detailed guide](https://pramodkumbhar.com/2023/11/pahole-to-analyz-data-structure-memory-layouts-with-ease/) for usage instructions.

## Step 2: Registering Data Type Instances
Since each instance of a data structure may exhibit different access patterns, the Memory Access Analyzer needs to track individual instances. 
Here's how to register them:

```cpp
#include <perfcpp/analyzer/memory_access.h>
auto memory_access_analyzer = perf::analyzer::MemoryAccess{};

/// Expose the data type to the Analyzer.
memory_access_analyzer.add(std::move(binary_tree_node));

/// Expose memory addresses to the Analyzer.
for (auto* node : tree->nodes()) {
    /// The first argument is the name describing the data type.
    /// The second argument is a pointer to the instance.
    memory_access_analyzer.annotate("BinaryTreeNode", node);
}
```

## Step 3: Mapping Samples to Data Type Instances
To collect memory access data, use *perf-cpp*'s [sampling mechanism](sampling.md) with the following key requirements:
* Include logical memory addresses
* Capture data source information
* Record latency data ("weight")
* Use a memory-address-capable sample trigger (e.g., `mem-loads` on Intel, `ibs_op` on AMD – see the [documentation](sampling.md#specific-notes-for-different-cpu-vendors))

```cpp
#include <perfcpp/sampler.h>
#include <perfcpp/analyzer/memory_access.h>

auto sampler = perf::Sampler{};

/// Set trigger that enables memory sampling.
sampler.trigger("mem-loads", perf::Precision::MustHaveZeroSkid, perf::Period{ 1000U });

/// Include addresses, data source, and latency.
sampler.values()
    .logical_memory_address(true)
    .data_src(true)
    .weight_struct(true);

/// Run the workload while recording samples.
sampler.start();
///... execute ....
sampler.stop();

/// Get the samples and map to described and registered data types and instances.
const auto samples = sampler.result();
const auto result = memory_access_analyzer.map(samples);
```

## Step 4: Processing the Result
The analyzer generates detailed statistics for each data type attribute. 
To view the results:
```cpp
std::cout << result.to_string() << std::endl;
```

Example output:

```bash
DataType BinaryTreeNode (24B) {
                                      |     loads      |    cache hits    |   RAM hits    |           TLB            |     stores    
                              samples | count  latency | L1d  LFB  L2  L3 | local  remote | L1 hits  L2 hits  misses | count  latency
      0:   value (8B)             373 |   373      439 | 154    0   0   7 |   212       0 |     190        5     178 |     0        0
      8:   left_child (8B)        146 |   146      720 |   1    0   0   5 |   140       0 |      12       18     116 |     0        0
     16:   right_child (8B)       528 |   528      173 | 393    0   1  14 |   120       0 |     415        4     109 |     0        0
}
```

The output shows:
* Attribute details (offset, name, size)
* Sample counts
* Detailed performance metrics per attribute

For further analysis, export the results in structured formats:

```cpp
result.to_json();  /// JSON format
result.to_csv();   /// CSV format
```