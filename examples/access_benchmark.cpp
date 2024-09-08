#include "access_benchmark.h"
#include <algorithm>
#include <numeric>
#include <random>

using namespace perf::example;

AccessBenchmark::AccessBenchmark(const bool is_random, const std::uint64_t access_data_size_in_mb, const bool is_write)
{
  const auto count_cache_lines = (access_data_size_in_mb * 1024U * 1024U) / sizeof(cache_line);

  /// Fill the data array with some data.
  this->_data_to_read.resize(count_cache_lines);
  for (auto i = 0U; i < this->_data_to_read.size(); ++i) {
    this->_data_to_read[i].value = i + 1U;
  }

  if (is_write) {
    this->_data_to_write.resize(count_cache_lines);
  }

  /// Create the access pattern by filling the indices and shuffle, if we want a
  /// random access pattern.
  this->_indices.resize(count_cache_lines);
  std::iota(this->_indices.begin(), this->_indices.end(), 0U);

  if (is_random) {
    std::shuffle(this->_indices.begin(), this->_indices.end(), std::mt19937{ std::random_device{}() });
  }
}