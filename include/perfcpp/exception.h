#pragma once

#include <cerrno>
#include <cstdint>
#include <perfcpp/feature.h>
#include <stdexcept>
#include <string>

namespace perf {

class CannotOpenFileError final : public std::runtime_error
{
public:
  explicit CannotOpenFileError(const std::string_view file_name)
    : std::runtime_error(std::string{ "Cannot open file '" }.append(file_name).append("'."))
  {
  }
  ~CannotOpenFileError() override = default;
};

class CannotOpenCounterError final : public std::runtime_error
{
public:
  explicit CannotOpenCounterError(const std::int64_t error_code)
    : std::runtime_error(std::string{ "Cannot open perf counter (error no " }
                           .append(std::to_string(error_code))
                           .append("): ")
                           .append(CannotOpenCounterError::create_error_message_from_code(error_code))
                           .append("."))
  {
  }
  ~CannotOpenCounterError() override = default;

private:
  /**
   * Creates an exception message based on the errno set when accessing the perf subsystem to open an event.
   *
   * @param error_code Error code raised when calling perf_event_open.
   * @return Error message that can be thrown to inform the user.
   */
  [[nodiscard]] static std::string create_error_message_from_code(const std::int64_t error_code)
  {
    switch (error_code) {
      case ENOENT:
        return "configuration might not be valid (e.g., wrong type or too many counters scheduled to the same hardware "
               "counter)";
      case E2BIG:
        return "perf_event_attr.size was not configured properly – this could be a bug in the perf-cpp library";
      case EACCES:
        return "insufficient access rights to start the counter, e.g., profiling a not user-owned process or "
               "perf_event_paranoid value too high (see "
               "https://github.com/jmuehlig/perf-cpp/blob/dev/docs/perf-paranoid.md)";
#ifndef PERFCPP_NO_ERROR_EBUSY /// Busy error is reported since Linux 4.1
      case EBUSY:
        return "another event has exclusive access to the PMU";
#endif
      case EINVAL:
        return "counter is configured with an invalid argument (e.g., too high sample frequency, unknown CPU, invalid "
               "sample type)";
      case EMFILE:
        return "too many open file descriptors (e.g., too many opened counters?)";
      case ENODEV:
        return "configured with feature that does not exist on this CPU";
      case EOVERFLOW:
        return "maximal callchain stack size is higher than the maximum (see /proc/sys/kernel/perf_event_max_stack)";
      case EPERM:
        return "one of the following features is set but not supported: excluding hypervisor, excluding idle, "
               "excluding "
               "user, or excluding kernel";
      case ESRCH:
        return "specified process does not exist";
      default:
        return "perf_event_open failed with unknown error";
    }
  }
};

class MmapError final : public std::runtime_error
{
public:
  explicit MmapError(const std::int64_t error_code)
    : std::runtime_error(
        std::string{ "Creating buffer via mmap() failed (error no: " }.append(std::to_string(error_code)).append(")."))
  {
  }
  ~MmapError() override = default;
};

class MmapNullError final : public std::runtime_error
{
public:
  MmapNullError()
    : std::runtime_error("Created buffer via mmap() is null.")
  {
  }
  ~MmapNullError() override = default;
};

class MaxCountersReachedError final : public std::runtime_error
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

class MaxGroupsReachedError final : public std::runtime_error
{
public:
  explicit MaxGroupsReachedError(const std::uint64_t max_groups)
    : std::runtime_error(std::string{ "Cannot add more events: reached maximum number of hardware counters (" }.append(
        std::to_string(max_groups).append("). Try to increase via perf::Config::max_groups(X).")))
  {
  }
  ~MaxGroupsReachedError() override = default;
};

class CannotAddCountersToSingleGroupError final : public std::runtime_error
{
public:
  explicit CannotAddCountersToSingleGroupError(const std::uint64_t counters, const std::uint64_t max_counters_per_group)
    : std::runtime_error(
        std::string{ "Cannot add " }
          .append(std::to_string(counters))
          .append(" counters to a single hardware counter, the maximum counters per hardware counter is ")
          .append(std::to_string(max_counters_per_group))
          .append(". Try to increase via perf::Config::max_counters_per_group(X)."))
  {
  }
  ~CannotAddCountersToSingleGroupError() override = default;
};

class CannotFindEventForMetricError final : public std::runtime_error
{
public:
  CannotFindEventForMetricError(const std::string_view event_name, const std::string_view metric_name)
    : std::runtime_error(std::string{ "Cannot find an event with name '" }
                           .append(event_name)
                           .append("' for metric '")
                           .append(metric_name)
                           .append("'."))
  {
  }
  ~CannotFindEventForMetricError() override = default;
};

class CannotFindEventOrMetricError final : public std::runtime_error
{
public:
  explicit CannotFindEventOrMetricError(const std::string& event_name)
    : std::runtime_error(std::string{ "Cannot find an event or metric with name '" }.append(event_name).append("'."))
  {
  }
  ~CannotFindEventOrMetricError() override = default;
};

class CannotFindEventError final : public std::runtime_error
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

class CannotChangeTriggerWhenSamplerOpenedError final : public std::runtime_error
{
public:
  CannotChangeTriggerWhenSamplerOpenedError()
    : std::runtime_error(
        "The Sampler was already opened. Cannot modify triggers after opening. Please create a new Sampler.")
  {
  }
  ~CannotChangeTriggerWhenSamplerOpenedError() override = default;
};

class MetricNotSupportedAsSamplingTriggerError final : public std::runtime_error
{
public:
  explicit MetricNotSupportedAsSamplingTriggerError(const std::string& metric_name)
    : std::runtime_error(std::string{ "The event '" }
                           .append(metric_name)
                           .append("' appears to be a metric. Metrics are not supported as sampling triggers."))
  {
  }
  ~MetricNotSupportedAsSamplingTriggerError() override = default;
};

class MetricNotSupportedAsLiveEventError final : public std::runtime_error
{
public:
  explicit MetricNotSupportedAsLiveEventError(const std::string& metric_name)
    : std::runtime_error(std::string{ "The event '" }
                           .append(metric_name)
                           .append("' appears to be a metric. Metrics are not supported as live events."))
  {
  }
  ~MetricNotSupportedAsLiveEventError() override = default;
};

class TimeEventNotSupportedAsLiveEventError final : public std::runtime_error
{
public:
  explicit TimeEventNotSupportedAsLiveEventError(const std::string& time_event)
    : std::runtime_error(std::string{ "The event '" }
                           .append(time_event)
                           .append("' appears to be a time event. Time events are not supported as live events."))
  {
  }
  ~TimeEventNotSupportedAsLiveEventError() override = default;
};

class TimeEventNotSupportedForSamplingError final : public std::runtime_error
{
public:
  explicit TimeEventNotSupportedForSamplingError(const std::string_view event_name)
    : std::runtime_error(std::string{ "The event '" }
                           .append(event_name)
                           .append("' appears to be a time event. Time events are not supported for sampling."))
  {
  }
  ~TimeEventNotSupportedForSamplingError() override = default;
};

class CannotStartEmptyGroupError final : public std::runtime_error
{
public:
  CannotStartEmptyGroupError()
    : std::runtime_error("Cannot start an empty group. Please add at least one counter.")
  {
  }
  ~CannotStartEmptyGroupError() override = default;
};

class CannotStartEmptySamplerError final : public std::runtime_error
{
public:
  CannotStartEmptySamplerError()
    : std::runtime_error(
        "Cannot start sampling without any trigger event. Please specify at least one trigger via Sampler::trigger().")
  {
  }
  ~CannotStartEmptySamplerError() override = default;
};

class DataTypeAlreadyRegisteredError final : public std::runtime_error
{
public:
  explicit DataTypeAlreadyRegisteredError(const std::string_view data_type_name)
    : std::runtime_error(std::string{ "The DataType '" }
                           .append(data_type_name)
                           .append("' is already registered and cannot be registered twice."))
  {
  }
  ~DataTypeAlreadyRegisteredError() override = default;
};

class DataTypeNotRegisteredError final : public std::runtime_error
{
public:
  explicit DataTypeNotRegisteredError(const std::string_view data_type_name)
    : std::runtime_error(std::string{ "The DataType '" }.append(data_type_name).append("' is was not found."))
  {
  }
  ~DataTypeNotRegisteredError() override = default;
};

class CannotParseExpressionError final : public std::runtime_error
{
public:
  explicit CannotParseExpressionError(const std::string_view input)
    : std::runtime_error(std::string{ "Cannot parse expression from '" }.append(input).append("'."))
  {
  }

  ~CannotParseExpressionError() override = default;
};

class CannotEvaluateExpressionError final : public std::runtime_error
{
public:
  explicit CannotEvaluateExpressionError(const std::string_view input)
    : std::runtime_error(std::string{ "Cannot evaluate expression '" }.append(input).append("'."))
  {
  }

  ~CannotEvaluateExpressionError() override = default;
};

class CannotCreateEventFileDescriptor final : public std::runtime_error
{
public:
  explicit CannotCreateEventFileDescriptor(const std::int32_t original_file_descriptor)
    : std::runtime_error(std::string{ "Cannot create eventfd for file descriptor " }
                           .append(std::to_string(original_file_descriptor))
                           .append("."))
  {
  }

  ~CannotCreateEventFileDescriptor() override = default;
};

}