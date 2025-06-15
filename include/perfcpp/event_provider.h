#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace perf {
class CounterDefinition;
class EventProvider
{
public:
  virtual ~EventProvider() noexcept = default;

  /**
   * Adds events to the counter definition.
   * @param counter_definition Counter definition to add events to.
   */
  virtual void add_events(CounterDefinition& counter_definition) = 0;
};

/**
 * Event provider to add events defined by the perf subsystem.
 */
class PerfSubsystemEventProvider final : public EventProvider
{
public:
  ~PerfSubsystemEventProvider() noexcept override = default;

  /**
   * Adds all general purpose events provided by the perf subsystem.
   * @param counter_definition Counter definition to add perf subsystem GP events to.
   */
  void add_events(CounterDefinition& counter_definition) override;
};

/**
 * Event provider to add virtual time events.
 */
class TimeEventProvider final : public EventProvider
{
public:
  ~TimeEventProvider() noexcept override = default;

  /**
   * Adds virtual time events.
   * @param counter_definition Counter definition to add events to.
   */
  void add_events(CounterDefinition& counter_definition) override;
};

/**
 * Event provider to add metrics.
 */
class MetricEventProvider final : public EventProvider
{
public:
  ~MetricEventProvider() noexcept override = default;

  /**
   * Adds metrics.
   * @param counter_definition Counter definition to add events to.
   */
  void add_events(CounterDefinition& counter_definition) override;
};

/**
 * Event provider to add events defined by the system using the file system.
 */
class SystemSpecificEventProvider final : public EventProvider
{
public:
  ~SystemSpecificEventProvider() noexcept override = default;

  /**
   * Adds events provided by the system.
   * @param counter_definition Counter definition to add events to.
   */
  void add_events(CounterDefinition& counter_definition) override;

  /**
   * Tries to read the type from the provided file.
   *
   * @param path Path of the type file.
   * @return Integer representation of type.
   */
  [[nodiscard]] static std::optional<std::uint32_t> parse_event_file_descriptor_type(std::filesystem::path&& path);

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
   * Tries to read a format file and returns the id of the config and the number of bits.
   * Some formats have multiple entries.
   *
   * @param path Path of the format file.
   * @return List of pairs (config id, bits).
   */
  [[nodiscard]] static std::vector<std::pair<std::uint8_t, std::pair<std::uint8_t, std::optional<std::uint8_t>>>>
  parse_event_file_descriptor_format(std::filesystem::path&& path);

private:
  static void add_events(CounterDefinition& counter_definition, std::string&& pmu_name, std::string&& path);
};

/**
 * Event provider to add events defined by AMD's Instruction Based Sampling PMU.
 */
class AMDIbsEventProvider final : public EventProvider
{
public:
  ~AMDIbsEventProvider() noexcept override = default;

  /**
   * Adds events provided by AMD's Instruction Based Sampling PMU.
   * @param counter_definition Counter definition to add events to.
   */
  void add_events(CounterDefinition& counter_definition) override;

private:
  /**
   * Adds events from the IBS Op PMU.
   * @param counter_definition Counter definition to add events to.
   */
  static void add_fetch_events(CounterDefinition& counter_definition);

  /**
   * Adds events from the IBS Op PMU.
   * @param counter_definition Counter definition to add events to.
   */
  static void add_op_events(CounterDefinition& counter_definition);
};

/**
 * Event provider to add events defined by an external (CSV) file.
 */
class CsvFileEventProvider final : public EventProvider
{
public:
  explicit CsvFileEventProvider(const std::string& file_name) noexcept
    : _file_name(file_name)
  {
  }
  ~CsvFileEventProvider() noexcept override = default;

  /**
   * Adds events provided by an external file.
   * @param counter_definition Counter definition to add events to.
   */
  void add_events(CounterDefinition& counter_definition) override;

private:
  const std::string& _file_name;
};

}