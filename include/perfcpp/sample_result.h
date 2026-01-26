#pragma once
#include "sample.h"
#include "sample_recording_values.h"
#include <fstream>
#include <functional>
#include <vector>

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
  explicit SampleResult(std::vector<value_type>&& samples) noexcept
    : _samples(std::move(samples))
  {
  }

  SampleResult(SampleRecordingValues values, std::vector<value_type>&& samples) noexcept
    : _sample_recording_values(std::move(values))
    , _samples(std::move(samples))
  {
  }

  SampleResult(const SampleResult&) = default;
  SampleResult(SampleResult&&) noexcept = default;

  SampleResult& operator=(const SampleResult&) = default;
  SampleResult& operator=(SampleResult&&) noexcept = default;

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
   * Returns true if the result is empty, i.e., does not contain any samples.
   *
   * @return True if the result does not contain samples.
   */
  [[nodiscard]] bool empty() const noexcept { return _samples.empty(); }

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
   * Accesses the first sample.
   *
   * @return Reference to the first sample.
   */
  [[nodiscard]] const Sample& front() const noexcept { return _samples.front(); }

  /**
   * Accesses the first sample.
   *
   * @return Reference to the first sample.
   */
  [[nodiscard]] Sample& front() noexcept { return _samples.front(); }

  /**
   * Accesses the last sample.
   *
   * @return Reference to the last sample.
   */
  [[nodiscard]] const Sample& back() const noexcept { return _samples.back(); }

  /**
   * Accesses the last sample.
   *
   * @return Reference to the last sample.
   */
  [[nodiscard]] Sample& back() noexcept { return _samples.back(); }

  /**
   * Filters the samples by unary function.
   *
   * @param filter Filter function mapping from sample to bool where "true" indicates to keep the value while "false"
   * discards the sample.
   */
  void filter(std::function<bool(const Sample&)> filter);

  /**
   * Writes the sample results as CSV to the given file.
   *
   * @param file_name File to write the sample results in CSV format.
   */
  void to_csv(std::string&& file_name) const;

private:
  /// List of values recorded by the sample. These values are represented in the samples.
  SampleRecordingValues _sample_recording_values;

  /// List of samples.
  std::vector<value_type> _samples;

  class CSVWriter
  {
  public:
    CSVWriter(std::ofstream& file_stream, const SampleRecordingValues& sample_recording_values) noexcept
      : _csv_stream(file_stream)
      , _values(sample_recording_values)
    {
    }

    CSVWriter(const CSVWriter&) = delete;
    CSVWriter(CSVWriter&&) = delete;

    ~CSVWriter() = default;

    CSVWriter& operator=(const CSVWriter&) = delete;
    CSVWriter& operator=(CSVWriter&&) = delete;

    void write_header(const SampleRecordingValues::Field field, std::string&& name)
    {
      if (this->_values.is_set(field)) {
        this->_csv_stream << "," << name;
      }
    }

    template<typename T>
    void write_value(const SampleRecordingValues::Field field, const std::optional<T> value, const bool is_hex = false)
    {
      if (this->_values.is_set(field)) {
        this->_csv_stream << ",";
        if (value.has_value()) {
          if constexpr (std::is_same_v<T, bool>) {
            this->_csv_stream << (value.value() ? "true" : "false");
          } else {
            if (is_hex) {
              this->_csv_stream << std::hex << "0x" << value.value() << std::dec;
            } else {
              this->_csv_stream << value.value();
            }
          }
        }
      }
    }

    void write_value(const SampleRecordingValues::Field field, const bool value)
    {
      if (this->_values.is_set(field)) {
        this->_csv_stream << "," << (value ? "true" : "false");
      }
    }

  private:
    std::ofstream& _csv_stream;
    const SampleRecordingValues& _values;
  };
};
}