#pragma once

#include <cstdint>
#include <vector>

namespace perf::example {
/**
 * Benchmark accessing benchmarks in random or sequential order.
 * This is an example to demonstrate the perfcpp library.
 */
class AccessBenchmark
{
public:
  /**
   * Object sized of one cache line.
   */
  struct alignas(64U) cache_line
  {
    std::int64_t value;
  };

  AccessBenchmark(bool is_random, std::uint64_t access_data_size_in_mb);
  ~AccessBenchmark() = default;

  /**
   * @return Number of cache lines.
   */
  [[nodiscard]] std::size_t size() const noexcept { return _indices.size(); }

  /**
   * Grant access to the i-th cache line, considering the defined access order.
   *
   * @param index Index of the cache line to access.
   * @return Cache line.
   */
  [[nodiscard]] const cache_line& operator[](const std::size_t index) const noexcept { return _data[_indices[index]]; }

private:
  /// Indices, defining the order in which the memory chunk is accessed.
  std::vector<std::uint64_t> _indices;

  /// Memory chunk that is accessed during the benchmark.
  std::vector<cache_line> _data;
};
}