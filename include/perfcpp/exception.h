#pragma once

#include <cerrno>
#include <cstdint>
#include <perfcpp/feature.h>
#include <stdexcept>
#include <string>

namespace perf {

class InvalidConfigAnyCpuCoreAndAnyProcess final : public std::runtime_error
{
public:
  explicit InvalidConfigAnyCpuCoreAndAnyProcess()
    : std::runtime_error("Cannot monitor any process on any CPU core. This configuration is invalid.")
  {
  }
  ~InvalidConfigAnyCpuCoreAndAnyProcess() override = default;
};

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
    , _error_code(error_code)
  {
  }
  ~CannotOpenCounterError() override = default;

  /**
   * @return The error code provided by the perf subsystem.
   */
  [[nodiscard]] std::int64_t error_code() const noexcept { return _error_code; }

private:
  std::int64_t _error_code;
  /**
   * Creates an exception message based on the errno set when accessing the perf subsystem to open an event.
   *
   * @param error_code Error code raised when calling perf_event_open.
   * @return Error message that can be thrown to inform the user.
   */
  [[nodiscard]] static std::string create_error_message_from_code(std::int64_t error_code);
};

class CannotReadCounter final : public std::runtime_error
{
public:
  CannotReadCounter()
    : std::runtime_error(std::string{ "Cannot read from event counter." })
  {
  }
  ~CannotReadCounter() override = default;
};

class IoctlError : public std::runtime_error
{
public:
  explicit IoctlError(const std::int64_t error_code, std::string&& error_message)
    : std::runtime_error(error_message.append(" (error no ")
                           .append(std::to_string(error_code))
                           .append("): ")
                           .append(IoctlError::create_error_message_from_code(error_code))
                           .append("."))
  {
  }

  ~IoctlError() override = default;

protected:
  [[nodiscard]] static std::string create_error_message_from_code(std::int64_t error_code);
};

class CannotEnableCounter final : public IoctlError
{
public:
  explicit CannotEnableCounter(const std::int64_t error_code)
    : IoctlError(error_code, "Cannot enable counter")
  {
  }
  ~CannotEnableCounter() override = default;
};

class CannotDisableCounter final : public IoctlError
{
public:
  explicit CannotDisableCounter(const std::int64_t error_code)
    : IoctlError(error_code, "Cannot disable counter")
  {
  }
  ~CannotDisableCounter() override = default;
};

class CannotReadCounterId final : public IoctlError
{
public:
  explicit CannotReadCounterId(const std::int64_t error_code)
    : IoctlError(error_code, "Cannot open counter")
  {
  }
  ~CannotReadCounterId() override = default;
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
  explicit MaxGroupsReachedError(const std::uint64_t num_physical_counters)
    : std::runtime_error(
        std::string{ "Cannot add more events: reached maximum number of physical performance counters (" }.append(
          std::to_string(num_physical_counters)
            .append("). Try to increase via perf::Config::num_physical_counters(X).")))
  {
  }
  ~MaxGroupsReachedError() override = default;
};

class CannotAddEventToSingleGroupError final : public std::runtime_error
{
public:
  explicit CannotAddEventToSingleGroupError(const std::uint64_t num_events_per_physical_counter)
    : std::runtime_error(std::string{ "Cannot add more than " }
                           .append(std::to_string(num_events_per_physical_counter))
                           .append(" events to a single physical counter. Try to increase via "
                                   "perf::Config::num_events_per_physical_counter(X)."))
  {
  }
  ~CannotAddEventToSingleGroupError() override = default;
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

class CannotEvaluateMetricsBecauseOfCycleError final : public std::runtime_error
{
public:
  explicit CannotEvaluateMetricsBecauseOfCycleError()
    : std::runtime_error(std::string{ "Cannot evaluate metrics because they are mutually (cyclically) dependent. " })
  {
  }
  ~CannotEvaluateMetricsBecauseOfCycleError() override = default;
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

  CannotFindEventError(const std::string_view pmu_name, const std::string_view event_name)
    : std::runtime_error(std::string{ "Cannot find an event with name '" }
                           .append(event_name)
                           .append("' for the PMU '")
                           .append(pmu_name)
                           .append("'."))
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

class SamplingFeatureIsNotSupported final : public std::runtime_error
{
public:
  SamplingFeatureIsNotSupported(const std::string_view feature_name, const std::string_view linux_kernel_version)
    : std::runtime_error(std::string{ "Sampling " }
                           .append(feature_name)
                           .append(" is only supported from Linux ")
                           .append(linux_kernel_version)
                           .append("."))
  {
  }

  ~SamplingFeatureIsNotSupported() override = default;
};

class AuxiliaryEventForSamplingNotFoundError final : public std::runtime_error
{
public:
  AuxiliaryEventForSamplingNotFoundError()
    : std::runtime_error("The underlying hardware requires an auxiliary counter for sampling memory loads but the "
                         "auxiliary event cannot be found.")
  {
  }
  ~AuxiliaryEventForSamplingNotFoundError() override = default;
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

class CannotParseMetricExpressionError final : public std::runtime_error
{
public:
  explicit CannotParseMetricExpressionError(const std::string_view input)
    : std::runtime_error(std::string{ "Cannot parse expression from '" }.append(input).append("'."))
  {
  }

  CannotParseMetricExpressionError(const std::string_view input, const std::string_view reason)
    : std::runtime_error(
        std::string{ "Cannot parse expression from '" }.append(input).append("': ").append(reason).append("."))
  {
  }

  ~CannotParseMetricExpressionError() override = default;
};

class CannotParseMetricExpressionUnknownFunctionError final : public std::runtime_error
{
public:
  explicit CannotParseMetricExpressionUnknownFunctionError(const std::string_view input,
                                                           const std::string_view function_name)
    : std::runtime_error(std::string{ "Cannot parse expression. Unknown function '" }
                           .append(function_name)
                           .append("' in expression '")
                           .append(input)
                           .append("'."))
  {
  }

  ~CannotParseMetricExpressionUnknownFunctionError() override = default;
};

class CannotParseMetricExpressionUnexpectedFunctionArgumentsError final : public std::runtime_error
{
public:
  explicit CannotParseMetricExpressionUnexpectedFunctionArgumentsError(const std::string_view input,
                                                                       const std::string_view function_name,
                                                                       const std::size_t expected_arguments,
                                                                       const std::size_t arguments)
    : std::runtime_error(std::string{ "Cannot parse expression. Function '" }
                           .append(function_name)
                           .append("' takes ")
                           .append(std::to_string(expected_arguments))
                           .append(" arguments, got ")
                           .append(std::to_string(arguments))
                           .append(" in expression '")
                           .append(input)
                           .append("'."))
  {
  }

  ~CannotParseMetricExpressionUnexpectedFunctionArgumentsError() override = default;
};

class CannotCreateEventFileDescriptor final : public std::runtime_error
{
public:
  CannotCreateEventFileDescriptor()
    : std::runtime_error(std::string{ "Cannot create eventfd for file descriptor." })
  {
  }

  ~CannotCreateEventFileDescriptor() override = default;
};

class CannotAddHeaderToTable final : public std::runtime_error
{
public:
  CannotAddHeaderToTable(const std::uint64_t columns, const std::uint64_t expected_columns)
    : std::runtime_error{ std::string{ "Header does not match the columns. Provided columns is " }
                            .append(std::to_string(columns))
                            .append(", expected is ")
                            .append(std::to_string(expected_columns))
                            .append(".") }
  {
  }
  ~CannotAddHeaderToTable() override = default;
};

class CannotAddRowToTable final : public std::runtime_error
{
public:
  CannotAddRowToTable(const std::uint64_t columns, const std::uint64_t expected_columns)
    : std::runtime_error{ std::string{ "Row does not match the columns. Provided columns is " }
                            .append(std::to_string(columns))
                            .append(", expected is ")
                            .append(std::to_string(expected_columns))
                            .append(".") }
  {
  }
  ~CannotAddRowToTable() override = default;
};

class CannotReadSymbolsForModule final : public std::runtime_error
{
public:
  CannotReadSymbolsForModule(const std::string_view name, const std::string_view path)
    : std::runtime_error{
      std::string{ "Cannot read symbols for module " }.append(name).append(" from path").append(path).append(".")
    }
  {
  }
  ~CannotReadSymbolsForModule() override = default;
};

class CannotReadFstatForModule final : public std::runtime_error
{
public:
  CannotReadFstatForModule(const std::string_view name, const std::string_view path)
    : std::runtime_error{
      std::string{ "Cannot read fstat for module " }.append(name).append(" from path").append(path).append(".")
    }
  {
  }
  ~CannotReadFstatForModule() override = default;
};

class CannotReadElfForModule final : public std::runtime_error
{
public:
  CannotReadElfForModule(const std::string_view name, const std::string_view path)
    : std::runtime_error{
      std::string{ "Cannot read ELF data for module " }.append(name).append(" from path").append(path).append(".")
    }
  {
  }
  ~CannotReadElfForModule() override = default;
};

class CannotVerifyElfMagicForModule final : public std::runtime_error
{
public:
  CannotVerifyElfMagicForModule(const std::string_view name, const std::string_view path)
    : std::runtime_error{
      std::string{ "Cannot verify ELF magic for module " }.append(name).append(" from path").append(path).append(".")
    }
  {
  }
  ~CannotVerifyElfMagicForModule() override = default;
};

}