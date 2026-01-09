#pragma once
#include "sample.h"
#include "sample_recording_values.h"
#include <vector>
#include <functional>

namespace perf {
/**
 * The SampleResult holds a collection of samples recorded by the perf subsystem and provides
 * utilities to access, iterate, and filter the samples.
 */
class SampleResult
{
public:
  using value_type = Sample;

  SampleResult() = default;
  explicit SampleResult(std::vector<value_type>&& samples) noexcept : _samples(std::move(samples))
  {
  }

  ~SampleResult() = default;

  /**
   * Converts the SampleResult into a vector of samples by moving the underlying data.
   *
   * @return Vector of samples.
   */
  [[nodiscard]] operator std::vector<value_type>() noexcept { return std::move(_samples); }

  /**
   * Converts the SampleResult into a vector of samples by copying the underlying data.
   *
   * @return Vector of samples.
   */
  [[nodiscard]] operator std::vector<value_type>() const noexcept { return _samples; }

  /**
   * Returns the number of samples in the result.
   *
   * @return Number of samples.
   */
  [[nodiscard]] std::size_t size() const noexcept { return _samples.size(); }

  /**
   * Returns an iterator to the beginning of the samples.
   *
   * @return Iterator to the first sample.
   */
  [[nodiscard]] std::vector<value_type>::iterator begin() noexcept { return _samples.begin(); }

  /**
   * Returns an iterator to the end of the samples.
   *
   * @return Iterator to the element past the last sample.
   */
  [[nodiscard]] std::vector<value_type>::iterator end() noexcept { return _samples.end(); }

  /**
   * Returns a const iterator to the beginning of the samples.
   *
   * @return Const iterator to the first sample.
   */
  [[nodiscard]] std::vector<value_type>::const_iterator begin() const noexcept { return _samples.begin(); }

  /**
   * Returns a const iterator to the end of the samples.
   *
   * @return Const iterator to the element past the last sample.
   */
  [[nodiscard]] std::vector<value_type>::const_iterator end() const noexcept { return _samples.end(); }

  /**
   * Adds a sample to the end of the collection.
   *
   * @param sample Sample to add.
   */
  void push_back(value_type&& sample) noexcept { _samples.push_back(std::move(sample)); }

  /**
   * Accesses a sample at the specified index.
   *
   * @param index Index of the sample to access.
   * @return Reference to the sample at the specified index.
   */
  [[nodiscard]] const Sample& operator[](const std::size_t index) const noexcept { return _samples[index]; }

  /**
   * Filters the samples by unary function.
   *
   * @param filter Filter function mapping from sample to bool where "true" indicates to keep the value while "false" discards the sample.
   */
  void filter(std::function<bool(const Sample&)> filter);
private:
  /// List of values recorded by the sample. These values are represented in the samples.
  SampleRecordingValues _sample_recording_values;

  /// List of samples.
  std::vector<value_type> _samples;
};
}