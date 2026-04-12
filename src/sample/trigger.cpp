#include <perfcpp/exception.hpp>
#include <perfcpp/hardware_info.h>
#include <perfcpp/sample/trigger.hpp>

std::vector<std::tuple<std::string_view, std::string_view, perf::CounterConfig>>
perf::Cycles::resolve(const CounterDefinition& counter_definition) const
{
  const auto counter_configs = counter_definition.counter(std::string_view{ "cycles" });
  if (counter_configs.empty()) {
    throw CannotFindEventError{ std::string_view{ "cycles" } };
  }
  return counter_configs;
}

std::optional<std::tuple<std::string_view, std::string_view, perf::CounterConfig>>
perf::Cycles::resolve(const CounterDefinition& counter_definition, const std::string_view pmu_name) const
{
  return counter_definition.counter(pmu_name, std::string_view{ "cycles" });
}

std::vector<std::tuple<std::string_view, std::string_view, perf::CounterConfig>>
perf::MemoryLoads::resolve(const CounterDefinition& counter_definition) const
{
  if (!HardwareInfo::is_intel()) {
    throw EventRequiresSpecificVendorError{ "Intel", "mem-loads" };
  }

  const auto counter_configs = counter_definition.counter(std::string_view{ "mem-loads" });
  if (counter_configs.empty()) {
    throw CannotFindEventError{ std::string_view{ "mem-loads" } };
  }

  /// Build one entry per PMU variant, patching config1 with the user-specified minimum latency
  /// (perf_event_attr.config1 is where the kernel reads the ldlat filter).
  auto result = std::vector<std::tuple<std::string_view, std::string_view, CounterConfig>>{};
  result.reserve(counter_configs.size());
  for (const auto& [pmu_name, event_name, base] : counter_configs) {
    result.emplace_back(
      pmu_name,
      event_name,
      CounterConfig{ base.type(), base.configs()[0U], this->_min_latency, base.configs()[2U], base.is_fixed() });
  }
  return result;
}

std::optional<std::tuple<std::string_view, std::string_view, perf::CounterConfig>>
perf::MemoryLoads::resolve(const CounterDefinition& counter_definition, const std::string_view pmu_name) const
{
  if (!HardwareInfo::is_intel()) {
    throw EventRequiresSpecificVendorError{ "Intel", "mem-loads" };
  }

  auto event = counter_definition.counter(pmu_name, std::string_view{ "mem-loads" });
  if (!event.has_value()) {
    return std::nullopt;
  }

  /// Patch config1 with the user-specified minimum latency (perf_event_attr.config1 = ldlat).
  const auto& base = std::get<2>(event.value());
  return std::tuple{ std::get<0>(event.value()),
                     std::get<1>(event.value()),
                     CounterConfig{
                       base.type(), base.configs()[0U], this->_min_latency, base.configs()[2U], base.is_fixed() } };
}

std::vector<std::tuple<std::string_view, std::string_view, perf::CounterConfig>>
perf::MemoryStores::resolve(const CounterDefinition& counter_definition) const
{
  if (!HardwareInfo::is_intel()) {
    throw EventRequiresSpecificVendorError{ "Intel", "mem-stores" };
  }

  const auto counter_configs = counter_definition.counter(std::string_view{ "mem-stores" });
  if (counter_configs.empty()) {
    throw CannotFindEventError{ std::string_view{ "mem-stores" } };
  }

  /// Return all PMU variants unchanged; mem-stores needs no config patching.
  return counter_configs;
}

std::optional<std::tuple<std::string_view, std::string_view, perf::CounterConfig>>
perf::MemoryStores::resolve(const CounterDefinition& counter_definition, const std::string_view pmu_name) const
{
  if (!HardwareInfo::is_intel()) {
    throw EventRequiresSpecificVendorError{ "Intel", "mem-stores" };
  }

  return counter_definition.counter(pmu_name, std::string_view{ "mem-stores" });
}

std::vector<std::tuple<std::string_view, std::string_view, perf::CounterConfig>>
perf::MemoryLoadsAux::resolve(const CounterDefinition& counter_definition) const
{
  if (!HardwareInfo::is_intel()) {
    throw EventRequiresSpecificVendorError{ "Intel", "mem-loads-aux" };
  }

  const auto counter_configs = counter_definition.counter(std::string_view{ "mem-loads-aux" });
  if (counter_configs.empty()) {
    throw CannotFindEventError{ std::string_view{ "mem-loads-aux" } };
  }

  return counter_configs;
}

std::optional<std::tuple<std::string_view, std::string_view, perf::CounterConfig>>
perf::MemoryLoadsAux::resolve(const CounterDefinition& counter_definition, const std::string_view pmu_name) const
{
  if (!HardwareInfo::is_intel()) {
    throw EventRequiresSpecificVendorError{ "Intel", "mem-loads-aux" };
  }

  return counter_definition.counter(pmu_name, std::string_view{ "mem-loads-aux" });
}

std::vector<std::tuple<std::string_view, std::string_view, perf::CounterConfig>>
perf::IbsFetch::resolve(const CounterDefinition& counter_definition) const
{
  if (!HardwareInfo::is_amd()) {
    throw EventRequiresSpecificVendorError{ "AMD", "ibs_fetch" };
  }

  /// Select the event name based on the L3 miss filter.
  /// Note: the registered ibs_fetch event always has rand_en set; is_rand=false cannot be
  /// honoured via event-name variants and requires direct config-bit patching (not yet implemented).
  const auto event_name = this->_is_l3_miss_only ? std::string{ "ibs_fetch_l3missonly" } : std::string{ "ibs_fetch" };

  const auto counter_configs = counter_definition.counter(event_name);
  if (counter_configs.empty()) {
    throw CannotFindEventError{ event_name };
  }

  /// AMD IBS has a single dedicated PMU; always a single entry.
  return { counter_configs.front() };
}

std::optional<std::tuple<std::string_view, std::string_view, perf::CounterConfig>>
perf::IbsFetch::resolve(const CounterDefinition& counter_definition, const std::string_view pmu_name) const
{
  if (!HardwareInfo::is_amd()) {
    throw EventRequiresSpecificVendorError{ "AMD", "ibs_fetch" };
  }

  /// Select the event name based on the L3 miss filter.
  const auto event_name =
    this->_is_l3_miss_only ? std::string_view{ "ibs_fetch_l3missonly" } : std::string_view{ "ibs_fetch" };
  return counter_definition.counter(pmu_name, event_name);
}

std::vector<std::tuple<std::string_view, std::string_view, perf::CounterConfig>>
perf::IbsOp::resolve(const CounterDefinition& counter_definition) const
{
  if (!HardwareInfo::is_amd()) {
    throw EventRequiresSpecificVendorError{ "AMD", "ibs_op" };
  }

  /// Select the event name from the four variants covering all combinations of cnt_ctl and l3missonly.
  auto event_name = std::string{ "ibs_op" };
  if (this->_is_uop) {
    event_name.append("_uops");
  }
  if (this->_is_l3_miss_only) {
    event_name.append("_l3missonly");
  }

  const auto counter_configs = counter_definition.counter(event_name);
  if (counter_configs.empty()) {
    throw CannotFindEventError{ event_name };
  }

  /// AMD IBS has a single dedicated PMU; always a single entry.
  return { counter_configs.front() };
}

std::optional<std::tuple<std::string_view, std::string_view, perf::CounterConfig>>
perf::IbsOp::resolve(const CounterDefinition& counter_definition, const std::string_view pmu_name) const
{
  if (!HardwareInfo::is_amd()) {
    throw EventRequiresSpecificVendorError{ "AMD", "ibs_op" };
  }

  /// Select the event name from the four variants covering all combinations of cnt_ctl and l3missonly.
  auto event_name = std::string{ "ibs_op" };
  if (this->_is_uop) {
    event_name.append("_uops");
  }
  if (this->_is_l3_miss_only) {
    event_name.append("_l3missonly");
  }

  return counter_definition.counter(pmu_name, event_name);
}
