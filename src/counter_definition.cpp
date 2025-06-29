#include <fstream>
#include <perfcpp/counter_definition.h>
#include <perfcpp/hardware_info.h>
#include <perfcpp/util/table.h>
#include <sstream>

perf::CounterDefinition::CounterDefinition(std::unique_ptr<EventProvider>&& event_provider)
{
  /// Reserve space for events.
  this->_performance_monitoring_unit_events.reserve(8U);
  this->_metrics.reserve(32U);
  this->_time_events.reserve(8U);

  /// Collect all generic event providers.
  auto event_providers = std::vector<std::unique_ptr<EventProvider>>{};
  event_providers.push_back(std::make_unique<PerfSubsystemEventProvider>());
  event_providers.push_back(std::make_unique<TimeEventProvider>());
  event_providers.push_back(std::make_unique<MetricEventProvider>());
  event_providers.push_back(std::make_unique<SystemSpecificEventProvider>());

#ifdef PERFCPP_HAS_PROCESSOR_SPECIFIC_EVENTS
  /// Hardware-specific events.
  event_providers.push_back(std::make_unique<ProcessorSpecificEventProvider>());
#endif

  /// AMD-specific event provider.
  if (HardwareInfo::is_amd()) {
    event_providers.push_back(std::make_unique<AMDIbsEventProvider>());
  }

  /// Additional event provider, if specified. For example, this could be a provider adding events from a file.
  if (event_provider != nullptr) {
    event_providers.push_back(std::move(event_provider));
  }

  /// Let the event providers add events.
  for (const auto& provider : event_providers) {
    provider->add_events(*this);
  }
}

perf::CounterDefinition::CounterDefinition(const std::string& config_file)
  : CounterDefinition(std::make_unique<CsvFileEventProvider>(config_file))
{
}

void
perf::CounterDefinition::add(std::string&& pmu_name, std::string&& event_name, const perf::CounterConfig config)
{
  /// When the PMU (identified by the name) already exists, add the event.
  if (auto pmu_iterator = this->_performance_monitoring_unit_events.find(pmu_name);
      pmu_iterator != this->_performance_monitoring_unit_events.end()) {
    auto& event_config_map = pmu_iterator->second;
    event_config_map.insert_or_assign(std::move(event_name), config);
  }

  /// Otherwise reserve some space for upcoming events for that PMU and add that map.
  else {
    auto event_config_map = std::unordered_map<std::string, perf::CounterConfig>{};
    event_config_map.reserve(64U);

    event_config_map.insert(std::make_pair(std::move(event_name), config));

    this->_performance_monitoring_unit_events.insert(std::make_pair(std::move(pmu_name), std::move(event_config_map)));
  }
}

std::vector<std::tuple<std::string_view, std::string_view, perf::CounterConfig>>
perf::CounterDefinition::counter(const std::string& name) const noexcept
{
  auto event_configurations = std::vector<std::tuple<std::string_view, std::string_view, perf::CounterConfig>>{};
  event_configurations.reserve(this->_performance_monitoring_unit_events.size());

  /// Scan all PMUs...
  for (const auto& [pmu_name, events] : this->_performance_monitoring_unit_events) {
    /// ... for an event with that requested name.
    if (auto iterator = events.find(name); iterator != events.end()) {
      event_configurations.emplace_back(
        std::string_view(pmu_name), std::string_view(iterator->first), iterator->second);
    }
  }

  return event_configurations;
}

std::optional<std::tuple<std::string_view, std::string_view, perf::CounterConfig>>
perf::CounterDefinition::counter(const std::string& pmu_name, const std::string& event_name) const noexcept
{
  /// Find all events of the PMU.
  if (auto pmu_iterator = this->_performance_monitoring_unit_events.find(pmu_name);
      pmu_iterator != this->_performance_monitoring_unit_events.end()) {
    const auto& pmu_events = pmu_iterator->second;

    /// Find the event in the PMU event list.
    if (auto event_iterator = pmu_events.find(event_name); event_iterator != pmu_events.end()) {
      return std::make_optional(std::make_tuple(
        std::string_view{ pmu_iterator->first }, std::string_view{ event_iterator->first }, event_iterator->second));
    }
  }

  return std::nullopt;
}

std::optional<std::pair<std::string_view, perf::Metric&>>
perf::CounterDefinition::metric(const std::string& name) const noexcept
{
  if (auto iterator = this->_metrics.find(name); iterator != this->_metrics.end()) {
    return std::make_optional(std::make_pair(std::string_view(iterator->first), std::ref(*iterator->second)));
  }

  return std::nullopt;
}

std::optional<std::pair<std::string_view, perf::TimeEvent&>>
perf::CounterDefinition::time_event(const std::string& name) const noexcept
{
  if (auto iterator = this->_time_events.find(name); iterator != this->_time_events.end()) {
    return std::make_optional(std::make_pair(std::string_view(iterator->first), std::ref(*iterator->second)));
  }

  return std::nullopt;
}

std::vector<std::pair<std::string_view, perf::CounterConfig>>
perf::CounterDefinition::pmu(const std::string& pmu_name) const
{
  auto events = std::vector<std::pair<std::string_view, CounterConfig>>{};

  if (const auto iterator = this->_performance_monitoring_unit_events.find(pmu_name);
      iterator != this->_performance_monitoring_unit_events.end()) {
    std::transform(iterator->second.begin(), iterator->second.end(), std::back_inserter(events), [](const auto& pair) {
      return std::make_pair(std::string_view{ std::get<0>(pair) }, std::get<1>(pair));
    });
  }

  return events;
}

void
perf::CounterDefinition::read_counter_configuration(const std::string& csv_filename)
{
  CsvFileEventProvider{ csv_filename }.add_events(*this);
}

std::string
perf::CounterDefinition::to_string() const
{
  /// Lambda to turn decimal value into a hexadecimal string. Used to print event ids.
  const auto decimal_to_hex_string = [](const auto decimal) -> std::string {
    auto stream = std::stringstream{};
    stream << "0x" << std::hex << decimal << std::dec;
    return stream.str();
  };

  const auto double_to_scientific = [](const auto decimal) -> std::string {
    if (decimal == 1.) {
      return "1";
    }
    auto stream = std::stringstream{};
    stream << std::scientific << decimal << std::dec;
    return stream.str();
  };

  auto table = util::Table{};

  // Header.
  table.add({ util::Table::Header{ "PMU", util::Table::Alignment::Left },
              util::Table::Header{ "name", util::Table::Alignment::Left },
              util::Table::Header{ "type", util::Table::Alignment::Left },
              util::Table::Header{ "config", util::Table::Alignment::Left },
              util::Table::Header{ "config1", util::Table::Alignment::Left },
              util::Table::Header{ "config2", util::Table::Alignment::Left },
              util::Table::Header{ "scale", util::Table::Alignment::Left } });

  /// Add all events to the table.
  for (const auto& [pmu, events] : this->_performance_monitoring_unit_events) {
    for (const auto& [name, config] : events) {
      auto row = util::Table::Row{};

      row << pmu << name << config.type() << decimal_to_hex_string(config.configs()[0U])
          << decimal_to_hex_string(config.configs()[1U]) << decimal_to_hex_string(config.configs()[2U])
          << double_to_scientific(config.scale());
      table.add(std::move(row));
    }
  }

  /// Add all metrics to the table.
  for (const auto& [name, _] : this->_metrics) {
    auto row = util::Table::Row{};
    row << "metric" << name << "" << "" << "" << "" << "";
    table.add(std::move(row));
  }

  /// Add all virtual time events to the table.
  for (const auto& [name, _] : this->_time_events) {
    auto row = util::Table::Row{};
    row << "time" << name << "" << "" << "" << "" << "";
    table.add(std::move(row));
  }

  return table.to_string();
}