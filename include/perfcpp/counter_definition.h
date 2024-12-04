#pragma once

#include "counter.h"
#include "metric.h"
#include "time_event.h"
#include <algorithm>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

namespace perf {
/**
 * The CounterDefinition holds names and configurations of events and metrics.
 */
class CounterDefinition
{
public:
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
   * @param name Name of the event.
   * @param type Type of the event.
   * @param event_id Id of the event.
   */
  void add(std::string&& name, const std::uint32_t type, const std::uint64_t event_id)
  {
    add(std::move(name), CounterConfig{ type, event_id });
  }

  /**
   * Adds a RAW event with the given name and configuration.
   *
   * @param name Name of the event.
   * @param event_id Id of the event.
   */
  void add(std::string&& name, const std::uint64_t event_id)
  {
    add(std::move(name), CounterConfig{ PERF_TYPE_RAW, event_id });
  }

  /**
   * Adds an event with the given name and configuration.
   *
   * @param config Config of the event.
   */
  void add(std::string&& name, CounterConfig config)
  {
    _hardware_counter_configurations.insert(std::make_pair(std::move(name), config));
  }

  /**
   * Adds a metric with the given name.
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
   * @param metric Metric.
   */
  void add(std::unique_ptr<Metric>&& metric) { _metrics.insert(std::make_pair(metric->name(), std::move(metric))); }

  /**
   * Adds a formula metric with the given name and formula.
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
   * Checks if a specific counter is registered and returns the name and the config.
   *
   * @param name Name of the queried counter.
   * @return Name and config of the counter, std::nullopt of the counter does not exist.
   */
  [[nodiscard]] std::optional<std::pair<std::string_view, CounterConfig>> counter(std::string&& name) const noexcept
  {
    return counter(name);
  }

  /**
   * Checks if a specific counter is registered and returns the name and the config.
   *
   * @param name Name of the queried counter.
   * @return Name and config of the counter, std::nullopt of the counter does not exist.
   */
  [[nodiscard]] std::optional<std::pair<std::string_view, CounterConfig>> counter(
    const std::string& name) const noexcept;

  /**
   * Checks if a specific counter is registered and returns the name and the config.
   *
   * @param name Name of the queried counter.
   * @return Name and config of the counter, std::nullopt of the counter does not exist.
   */
  [[nodiscard]] std::optional<std::pair<std::string_view, CounterConfig>> counter(
    const std::string_view name) const noexcept
  {
    return counter(std::string{ name });
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
    std::transform(_hardware_counter_configurations.begin(),
                   _hardware_counter_configurations.end(),
                   std::back_inserter(names),
                   [](const auto& config) { return config.first; });
    return names;
  }

  /**
   * Reads and adds counters from the provided CSV file with counter configurations.
   * @param csv_filename CSV file with counter configurations.
   */
  void read_counter_configuration(const std::string& csv_filename);

private:
  /// List of added counter configurations.
  std::unordered_map<std::string, CounterConfig> _hardware_counter_configurations;

  /// List of added metrics.
  std::unordered_map<std::string, std::unique_ptr<Metric>> _metrics;

  /// List of time events.
  std::unordered_map<std::string, std::unique_ptr<TimeEvent>> _time_events;

  /**
   * Add all generalized counters to the counter config.
   */
  void initialize_generalized_counters();

  /**
   * If the system is an AMD, read IBS counters, if supported.
   */
  void initialize_amd_ibs_counters();

  /**
   * If the system is an Intel, read some PEBS counters, if supported.
   */
  void initialize_intel_pebs_counters();

  /**
   * Initializes time events.
   */
  void initialize_time_events();
};
}