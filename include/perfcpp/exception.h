#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>

namespace perf {

class CannotOpenFileError final : std::runtime_error
{
public:
  explicit CannotOpenFileError(const std::string_view file_name)
    : std::runtime_error(std::string{ "Cannot open file '" }.append(file_name).append("'."))
  {
  }
  ~CannotOpenFileError() override = default;
};

class CannotOpenCounterError final : std::runtime_error
{
public:
  explicit CannotOpenCounterError(std::string&& message, const std::int64_t error_code)
    : std::runtime_error(std::string{ "Cannot open perf counter: " }
                           .append(message)
                           .append(" (error no: ")
                           .append(std::to_string(error_code))
                           .append(")."))
  {
  }
  ~CannotOpenCounterError() override = default;
};

class MmapError final : std::runtime_error
{
public:
  explicit MmapError(const std::int64_t error_code)
    : std::runtime_error(
        std::string{ "Creating buffer via mmap() failed (error no: " }.append(std::to_string(error_code)).append(")."))
  {
  }
  ~MmapError() override = default;
};

class MmapNullError final : std::runtime_error
{
public:
  MmapNullError()
    : std::runtime_error("Created buffer via mmap() is null.")
  {
  }
  ~MmapNullError() override = default;
};

class MaxCountersReachedError final : std::runtime_error
{
public:
  MaxCountersReachedError(const std::uint64_t max_counters, const std::uint64_t max_events_per_counter)
    : std::runtime_error(
        std::string{ "Cannot add more events: reached maximum number of counters and events (" }.append(
          std::to_string(max_counters)
            .append(" counters, ")
            .append(std::to_string(max_events_per_counter))
            .append(" events per counter). Try to increase via perf::Config::max_groups(X) and "
                    "perf::Config::max_counters_per_group(Y).")))
  {
  }

  explicit MaxCountersReachedError(const std::uint64_t max_counters)
    : std::runtime_error(std::string{ "Cannot add more events: reached maximum number of counters (" }.append(
        std::to_string(max_counters).append("). Try to increase via perf::Config::max_groups(X).")))
  {
  }
  ~MaxCountersReachedError() override = default;
};

class CannotFindEventForMetricError final : std::runtime_error
{
public:
  CannotFindEventForMetricError(const std::string& event_name, const std::string& metric_name)
    : std::runtime_error(std::string{ "Cannot find an event with name '" }
                           .append(event_name)
                           .append("' for metric '")
                           .append(metric_name)
                           .append("'."))
  {
  }
  ~CannotFindEventForMetricError() override = default;
};

class CannotFindEventOrMetricError final : std::runtime_error
{
public:
  explicit CannotFindEventOrMetricError(const std::string& event_name)
    : std::runtime_error(std::string{ "Cannot find an event or metric with name '" }.append(event_name).append("'."))
  {
  }
  ~CannotFindEventOrMetricError() override = default;
};

class CannotFindEventError final : std::runtime_error
{
public:
  explicit CannotFindEventError(const std::string& event_name)
    : CannotFindEventError(std::string_view{ event_name })
  {
  }

  explicit CannotFindEventError(const std::string_view event_name)
    : std::runtime_error(std::string{ "Cannot find an event with name '" }.append(event_name).append("'."))
  {
  }
  ~CannotFindEventError() override = default;
};

class MetricNotSupportedError final : std::runtime_error
{
public:
  MetricNotSupportedError(const std::string& event_name, std::string&& feature)
    : std::runtime_error(std::string{ "The event '" }
                           .append(event_name)
                           .append("' appears to be a metric. Metrics are not supported for ")
                           .append(feature)
                           .append("."))
  {
  }
  ~MetricNotSupportedError() override = default;
};

class CannotStartEmptyGroupError final : std::runtime_error
{
public:
  CannotStartEmptyGroupError()
    : std::runtime_error("Cannot start an empty group. Please add at least one counter.")
  {
  }
  ~CannotStartEmptyGroupError() override = default;
};

class CannotStartEmptySamplerError final : std::runtime_error
{
public:
  CannotStartEmptySamplerError()
    : std::runtime_error(
        "Cannot start sampling without any trigger event. Please specify at least one trigger via Sampler::trigger().")
  {
  }
  ~CannotStartEmptySamplerError() override = default;
};

class DataTypeAlreadyRegisteredError final : std::runtime_error
{
public:
  explicit DataTypeAlreadyRegisteredError(const std::string& data_type_name)
    : std::runtime_error(std::string{ "The DataType '" }
                           .append(data_type_name)
                           .append("' is already registered and cannot be registered twice."))
  {
  }
  ~DataTypeAlreadyRegisteredError() override = default;
};

}