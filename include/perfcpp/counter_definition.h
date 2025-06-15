#pragma once

#include "counter.h"
#include "metric.h"
#include "time_event.h"
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <unordered_map>

namespace perf {
/**
 * The CounterDefinition holds names and configurations of events and metrics.
 */
class CounterDefinition
{
public:
  static CounterDefinition DEFAULT;

  CounterDefinition();
  explicit CounterDefinition(const std::string& config_file);
  explicit CounterDefinition(std::string&& config_file)
    : CounterDefinition(config_file)
  {
  }

  CounterDefinition(CounterDefinition&&) noexcept = default;
  CounterDefinition& operator=(CounterDefinition&&) noexcept = default;

  ~CounterDefinition() = default;

  /**
   * Adds an event with the given name and configuration.
   *
   * See https://github.com/jmuehlig/perf-cpp/blob/dev/docs/counters.md
   *
   * @param pmu_name Name of the PMU.
   * @param event_name Name of the event.
   * @param type Type of the event.
   * @param event_id Id of the event.
   */
  void add(std::string&& pmu_name, std::string&& event_name, const std::uint32_t type, const std::uint64_t event_id)
  {
    add(std::move(pmu_name), std::move(event_name), CounterConfig{ type, event_id });
  }

  /**
   * Adds a RAW event with the given name and configuration.
   *
   * See https://github.com/jmuehlig/perf-cpp/blob/dev/docs/counters.md
   *
   * @param pmu_name Name of the PMU.
   * @param event_name Name of the event.
   * @param event_id Id of the event.
   */
  void add(std::string&& pmu_name, std::string&& event_name, const std::uint64_t event_id)
  {
    add(std::move(pmu_name), std::move(event_name), CounterConfig{ PERF_TYPE_RAW, event_id });
  }

  /**
   * Adds an event with the given name and configuration.
   *
   * See https://github.com/jmuehlig/perf-cpp/blob/dev/docs/counters.md
   *
   * @param pmu_name Name of the PMU.
   * @param event_name Name of the event.
   * @param config Config of the event.
   */
  void add(std::string&& pmu_name, std::string&& event_name, CounterConfig config);

  /**
   * Adds an event with the given name and configuration.
   *
   * See https://github.com/jmuehlig/perf-cpp/blob/dev/docs/counters.md
   *
   * @param name Name of the event.
   * @param type Type of the event.
   * @param event_id Id of the event.
   */
  void add(std::string&& name, const std::uint32_t type, const std::uint64_t event_id)
  {
    add("cpu", std::move(name), type, event_id);
  }

  /**
   * Adds a RAW event with the given name and configuration.
   *
   * See https://github.com/jmuehlig/perf-cpp/blob/dev/docs/counters.md
   *
   * @param name Name of the event.
   * @param event_id Id of the event.
   */
  void add(std::string&& name, const std::uint64_t event_id) { add("cpu", std::move(name), event_id); }

  /**
   * Adds an event with the given name and configuration.
   *
   * See https://github.com/jmuehlig/perf-cpp/blob/dev/docs/counters.md
   *
   * @param config Config of the event.
   */
  void add(std::string&& name, CounterConfig config) { add("cpu", std::move(name), config); }

  /**
   * Adds a metric with the given name.
   *
   * See https://github.com/jmuehlig/perf-cpp/blob/dev/docs/metrics.md
   *
   * @param name Name of the metric.
   * @param metric Metric.
   */
  void add(std::string&& name, std::unique_ptr<Metric>&& metric)
  {
    _metrics.insert(std::make_pair(std::move(name), std::move(metric)));
  }

  /**
   * Adds a metric. The name is provided by the metric.
   *
   * See https://github.com/jmuehlig/perf-cpp/blob/dev/docs/metrics.md
   *
   * @param metric Metric.
   */
  void add(std::unique_ptr<Metric>&& metric) { _metrics.insert(std::make_pair(metric->name(), std::move(metric))); }

  /**
   * Adds a formula metric with the given name and formula.
   *
   * See https://github.com/jmuehlig/perf-cpp/blob/dev/docs/metrics.md#using-formulas
   *
   * @param name Name of the metric.
   * @param formula Expression of the metric.
   */
  void add(std::string&& name, std::string&& formula)
  {
    this->add(std::make_unique<FormulaMetric>(std::move(name), std::move(formula)));
  }

  /**
   * Adds a time event the given name.
   *
   * @param name Name of the time event.
   * @param time_event Time event.
   */
  void add(std::string&& name, std::unique_ptr<TimeEvent>&& time_event)
  {
    _time_events.insert(std::make_pair(std::move(name), std::move(time_event)));
  }

  /**
   * Returns a list of counter configurations with the requested name.
   *
   * @param name Name of the queried counter.
   * @return A list of 3-tuples (name of the PMU, name of the event, event configuration). The list may be empty, when
   * that event does not exist.
   */
  [[nodiscard]] std::vector<std::tuple<std::string_view, std::string_view, CounterConfig>> counter(
    std::string&& name) const noexcept
  {
    return counter(name);
  }

  /**
   * Returns a list of counter configurations with the requested name.
   *
   * @param name Name of the queried counter.
   * @return A list of 3-tuples (name of the PMU, name of the event, event configuration). The list may be empty, when
   * that event does not exist.
   */
  [[nodiscard]] std::vector<std::tuple<std::string_view, std::string_view, CounterConfig>> counter(
    const std::string& name) const noexcept;

  /**
   * Returns a list of counter configurations with the requested name.
   *
   * @param name Name of the queried counter.
   * @return A list of 3-tuples (name of the PMU, name of the event, event configuration). The list may be empty, when
   * that event does not exist.
   */
  [[nodiscard]] std::vector<std::tuple<std::string_view, std::string_view, CounterConfig>> counter(
    const std::string_view name) const noexcept
  {
    return counter(std::string{ name });
  }

  /**
   * Returns the counter configurations with the requested event name for a specified PMU.
   *
   * @param pmu_name Name of the PMU.
   * @param event_name Name of the event.
   * @return Tuple of PMU name, event name, and event configuration.
   */
  [[nodiscard]] std::optional<std::tuple<std::string_view, std::string_view, CounterConfig>> counter(
    const std::string& pmu_name,
    const std::string& event_name) const noexcept;

  /**
   * Returns the counter configurations with the requested event name for a specified PMU.
   *
   * @param pmu_name Name of the PMU.
   * @param event_name Name of the event.
   * @return Tuple of PMU name, event name, and event configuration.
   */
  [[nodiscard]] std::optional<std::tuple<std::string_view, std::string_view, CounterConfig>> counter(
    const std::string_view pmu_name,
    const std::string_view event_name) const noexcept
  {
    return counter(std::string{ pmu_name }, std::string{ event_name });
  }

  /**
   * Checks if a metric with the given name is registered.
   *
   * @param name Name of the requested query.
   * @return True, if the metric exists.
   */
  [[nodiscard]] bool is_metric(const std::string& name) const noexcept { return _metrics.find(name) != _metrics.end(); }

  /**
   * Checks if a metric with the given name is registered.
   *
   * @param name Name of the requested metric.
   * @return True, if the metric exists.
   */
  [[nodiscard]] bool is_metric(std::string_view name) const noexcept
  {
    return is_metric(std::string{ name.data(), name.size() });
  }

  /**
   * Checks if a specific metric is registered and returns the name and the metric.
   *
   * @param name Name of the queried metric.
   * @return Metric and config, std::nullopt of the metric does not exist.
   */
  [[nodiscard]] std::optional<std::pair<std::string_view, Metric&>> metric(const std::string& name) const noexcept;

  /**
   * Checks if a specific metric is registered and returns the name and the metric.
   *
   * @param name Name of the queried metric.
   * @return Metric and config, std::nullopt of the metric does not exist.
   */
  [[nodiscard]] std::optional<std::pair<std::string_view, Metric&>> metric(std::string&& name) const noexcept
  {
    return metric(name);
  }

  /**
   * Checks if a specific metric is registered and returns the name and the metric.
   *
   * @param name Name of the queried metric.
   * @return Metric and config, std::nullopt of the metric does not exist.
   */
  [[nodiscard]] std::optional<std::pair<std::string_view, Metric&>> metric(const std::string_view name) const noexcept
  {
    return metric(std::string{ name.data(), name.size() });
  }

  /**
   * Checks if a time event with the given name is registered.
   *
   * @param name Name of the requested time event.
   * @return True, if the time event exists.
   */
  [[nodiscard]] bool is_time_event(const std::string& name) const noexcept
  {
    return _time_events.find(name) != _time_events.end();
  }

  /**
   * Checks if a time event with the given name is registered.
   *
   * @param name Name of the requested time event.
   * @return True, if the time event exists.
   */
  [[nodiscard]] bool is_time_event(std::string&& name) const noexcept { return is_time_event(name); }

  /**
   * Checks if a time event with the given name is registered.
   *
   * @param name Name of the requested time event.
   * @return True, if the time event exists.
   */
  [[nodiscard]] bool is_time_event(const std::string_view name) const noexcept
  {
    return is_time_event(std::string{ name.data(), name.size() });
  }

  /**
   * Checks if a specific time event is registered and returns the name and the time event.
   *
   * @param name Name of the queried time event.
   * @return Time event and config, std::nullopt of the time event does not exist.
   */
  [[nodiscard]] std::optional<std::pair<std::string_view, TimeEvent&>> time_event(
    const std::string& name) const noexcept;

  /**
   * Checks if a specific time event is registered and returns the name and the time event.
   *
   * @param name Name of the queried time event.
   * @return Time event and config, std::nullopt of the time event does not exist.
   */
  [[nodiscard]] std::optional<std::pair<std::string_view, TimeEvent&>> time_event(std::string&& name) const noexcept
  {
    return time_event(name);
  }

  /**
   * Checks if a specific time event is registered and returns the name and the time event.
   *
   * @param name Name of the queried time event.
   * @return Time event and config, std::nullopt of the time event does not exist.
   */
  [[nodiscard]] std::optional<std::pair<std::string_view, TimeEvent&>> time_event(
    const std::string_view name) const noexcept
  {
    return time_event(std::string{ name.data(), name.size() });
  }

  /**
   * @return List names of all available counters.
   */
  [[nodiscard]] std::vector<std::string> names() const
  {
    auto names = std::vector<std::string>{};
    std::transform(_performance_monitoring_unit_events.begin(),
                   _performance_monitoring_unit_events.end(),
                   std::back_inserter(names),
                   [](const auto& config) { return config.first; });
    return names;
  }

  /**
   * Reads and adds counters from the provided CSV file with counter configurations.
   * @param csv_filename CSV file with counter configurations.
   */
  void read_counter_configuration(const std::string& csv_filename);

  /**
   * Translates a config string (hexadecimal or decimal) to a number.
   * @param config Config string.
   * @return Number.
   */
  [[nodiscard]] static std::uint64_t config_string_to_unsigned_long(const std::string& config);

  /**
   * @return A table containing all events, metrics, and virtual time events.
   */
  [[nodiscard]] std::string to_string() const;

private:
  /// List of added counter configurations for different PMUs. Each PMU can have multiple counters; but different PMUs
  /// can have the same counter name with different configurations.
  std::unordered_map<std::string, std::unordered_map<std::string, CounterConfig>> _performance_monitoring_unit_events;

  /// List of added metrics.
  std::unordered_map<std::string, std::unique_ptr<Metric>> _metrics;

  /// List of time events.
  std::unordered_map<std::string, std::unique_ptr<TimeEvent>> _time_events;

  /**
   * Adds all counters specified as constants by the perf subsystem in the linux perf header.
   */
  void add_general_events_from_perf_subsystem();

  /**
   * Scans the given path for events and adds the found ones.
   * All events that are already specified (e.g., by the perf subsystem constant) will NOT be replaced.
   *
   * @param pmu_name Name of the PMU the events belong to.
   * @param path Path of the event descriptors.
   */
  void add_events_from_descriptor_files(std::string&& pmu_name, std::string&& path);

  /**
   * If the system is an AMD, read IBS Fetch PMU, if supported.
   */
  void add_amd_ibs_fetch_events();

  /**
   * If the system is an AMD, read IBS Op PMU, if supported.
   */
  void add_amd_ibs_op_events();

  /**
   * Initializes time events.
   */
  void add_virtual_time_events();

  /**
   * Add pre-defined metrics.
   */
  void add_metrics();

  /**
   * Parses an event file descriptor (typically located somewhere in the /sys/bus/event_source/.. directory).
   * Typically, event file descriptors contain the event code, umask, and some additional data (e.g., ldlat for load
   * latency).
   *
   * @param path Path of the file descriptor.
   * @return A pair of configuration code and (optional) additional information, like load latency. When the descriptor
   * could not be parsed, nullopt will be returned.
   */
  [[nodiscard]] static std::optional<std::pair<std::uint64_t, std::optional<std::uint64_t>>>
  parse_event_file_descriptor_config(const std::filesystem::path& path);

  /**
   * Tries to read the type from the provided file.
   *
   * @param path Path of the type file.
   * @return Integer representation of type.
   */
  [[nodiscard]] static std::optional<std::uint32_t> parse_event_file_descriptor_type(std::filesystem::path&& path);

  /**
   * Tries to read a format file and returns the id of the config and the number of bits.
   * Some formats have multiple entries.
   *
   * @param path Path of the format file.
   * @return List of pairs (config id, bits).
   */
  [[nodiscard]] static std::vector<std::pair<std::uint8_t, std::pair<std::uint8_t, std::optional<std::uint8_t>>>>
  parse_event_file_descriptor_format(std::filesystem::path&& path);
};
}