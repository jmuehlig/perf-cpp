#include "access_benchmark.h"
#include <algorithm>
#include <numeric>
#include <random>

using namespace perf::example;

AccessBenchmark::AccessBenchmark(const bool is_random, const std::uint64_t access_data_size_in_mb)
{
  const auto count_cache_lines = (access_data_size_in_mb * 1024U * 1024U) / sizeof(cache_line);

  /// Fill the data array with some data.
  this->_data.resize(count_cache_lines);
  for (auto i = 0U; i < this->_data.size(); ++i) {
    this->_data[i].value = i + 1U;
  }

  /// Create the access pattern by filling the indices and shuffle, if we want a
  /// random access pattern.
  this->_indices.resize(count_cache_lines);
  std::iota(this->_indices.begin(), this->_indices.end(), 0U);

  if (is_random) {
    std::shuffle(this->_indices.begin(), this->_indices.end(), std::mt19937{ std::random_device{}() });
  }
}