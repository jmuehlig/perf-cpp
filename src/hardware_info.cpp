#include <algorithm>
#include <cerrno>
#include <filesystem>
#include <fstream>
#include <perfcpp/counter_definition.h>
#include <perfcpp/group.h>
#include <perfcpp/hardware_info.h>
#include <unistd.h>
#if defined(__x86_64__) || defined(__i386__)
#include <cpuid.h>
#endif

/// Cache variable to remember if Intel's auxiliary event is required for sampling.
std::optional<bool> perf::HardwareInfo::_is_intel_aux_event_required{ std::nullopt };

/// Cache variable to remember if the underlying Intel hardware is from the 12th generation or even newer; these
/// machines have heterogeneous CPUs and PMUs.
std::optional<bool> perf::HardwareInfo::_is_intel_12th_generation_or_newer{ std::nullopt };

/// Cache variable to remember AMD IBS is supported.
std::optional<bool> perf::HardwareInfo::_is_amd_ibs_supported{ std::nullopt };

/// Cache variable to remember if AMD's IBS supports filtering for L3 cache misses.
std::optional<bool> perf::HardwareInfo::_is_ibs_l3_filter_supported{ std::nullopt };

/// Cache variable to remember the memory page size.
std::optional<std::uint64_t> perf::HardwareInfo::_memory_page_size{ std::nullopt };

/// Number of performance counters per logical CPU core.
std::optional<std::uint8_t> perf::HardwareInfo::_physical_performance_counters_per_logical_core{ std::nullopt };

/// Number of events that can be scheduled to the same physical performance counter.
std::optional<std::uint8_t> perf::HardwareInfo::_events_per_physical_performance_counter{ std::nullopt };

/// Maximal clock frequency across all cores in Hz.
std::optional<std::uint64_t> perf::HardwareInfo::_max_cpu_clock_frequency{ std::nullopt };

bool
perf::HardwareInfo::is_intel_aux_counter_required()
{
#if defined(__x86_64__) || defined(__i386__)
  if (HardwareInfo::_is_intel_aux_event_required.has_value()) {
    return HardwareInfo::_is_intel_aux_event_required.value();
  }

  if (!HardwareInfo::is_intel()) {
    return HardwareInfo::cache_value(HardwareInfo::_is_intel_aux_event_required, false);
  }

  const auto is_aux_event_required =
    std::filesystem::exists(std::filesystem::path("/sys/bus/event_source/devices/cpu/events/mem-loads-aux")) ||
    std::filesystem::exists(std::filesystem::path("/sys/bus/event_source/devices/cpu_core/events/mem-loads-aux"));
  return HardwareInfo::cache_value(HardwareInfo::_is_intel_aux_event_required, is_aux_event_required);
#else
  return false;
#endif
}

bool
perf::HardwareInfo::is_intel_12th_generation_or_newer()
{
#if defined(__x86_64__) || defined(__i386__)
  if (HardwareInfo::_is_intel_12th_generation_or_newer.has_value()) {
    return HardwareInfo::_is_intel_12th_generation_or_newer.value();
  }

  if (!HardwareInfo::is_intel()) {
    return HardwareInfo::cache_value(HardwareInfo::_is_intel_12th_generation_or_newer, false);
  }

  /// Get processor family/model information
  if (const auto model_info = HardwareInfo::cpuid(0x1); model_info.has_value()) {
    /// Check the family.
    const auto family_id = (model_info->eax >> 8) & 0xF;
    const auto extended_family_id = (model_info->eax >> 20) & 0xFF;

    /// Families < 6 are older than Alder Lake (12th generation); families > 6 are newer (and do not exist up to now).
    if (const auto display_family = family_id + extended_family_id;
        display_family != /* 6U is the line between 12th and earlier generations */ 6U) {
      return HardwareInfo::cache_value(HardwareInfo::_is_intel_12th_generation_or_newer, display_family > 6U);
    }

    /// For family 6, check the model.
    const auto model = (model_info->eax >> 4) & 0xF;
    const auto extended_model = (model_info->eax >> 16) & 0xF;

    const auto display_model = (extended_model << 4) + model;
    return HardwareInfo::cache_value(HardwareInfo::_is_intel_12th_generation_or_newer,
                                     display_model >= /* 12th generation */ 143U);
  }

  return HardwareInfo::cache_value(HardwareInfo::_is_intel_12th_generation_or_newer, false);

#else
  return false;
#endif
}

bool
perf::HardwareInfo::is_amd_ibs_supported()
{
#if defined(__x86_64__) || defined(__i386__)
  if (HardwareInfo::_is_amd_ibs_supported.has_value()) {
    return HardwareInfo::_is_amd_ibs_supported.value();
  }

  if (!HardwareInfo::is_amd()) {
    return HardwareInfo::cache_value(HardwareInfo::_is_amd_ibs_supported, false);
  }

  /// See https://github.com/jlgreathouse/AMD_IBS_Toolkit/blob/master/ibs_with_perf_events.txt
  if (const auto extended_processor_info = HardwareInfo::cpuid(0x80000001); extended_processor_info.has_value()) {
    return HardwareInfo::cache_value(
      HardwareInfo::_is_amd_ibs_supported,
      static_cast<bool>(extended_processor_info->ecx & (static_cast<std::uint32_t>(1U) << 10)));
  }

  return HardwareInfo::cache_value(HardwareInfo::_is_amd_ibs_supported, false);
#else
  return false;
#endif
}

bool
perf::HardwareInfo::is_ibs_l3_filter_supported()
{
#if defined(__x86_64__) || defined(__i386__)
  if (HardwareInfo::_is_ibs_l3_filter_supported.has_value()) {
    return HardwareInfo::_is_ibs_l3_filter_supported.value();
  }

  if (!HardwareInfo::is_amd_ibs_supported()) {
    return HardwareInfo::cache_value(HardwareInfo::_is_ibs_l3_filter_supported, false);
  }

  if (const auto ibs_info = HardwareInfo::cpuid(0x8000001b); ibs_info.has_value()) {
    const auto is_ibs_l3_filter_supported = static_cast<bool>(ibs_info->eax & (static_cast<std::uint32_t>(1U) << 11));
    return HardwareInfo::cache_value(HardwareInfo::_is_ibs_l3_filter_supported, is_ibs_l3_filter_supported);
  }

  return HardwareInfo::cache_value(HardwareInfo::_is_ibs_l3_filter_supported, false);
#else
  return false;
#endif
}

std::uint64_t
perf::HardwareInfo::memory_page_size()
{
  if (HardwareInfo::_memory_page_size.has_value()) {
    return HardwareInfo::_memory_page_size.value();
  }

  /// Read memory page size from sysconf (see https://man7.org/linux/man-pages/man3/sysconf.3.html).
  const auto memory_page_size = static_cast<std::uint64_t>(std::max(0L, ::sysconf(_SC_PAGESIZE)));
  return HardwareInfo::cache_value(HardwareInfo::_memory_page_size, memory_page_size);
}

std::uint8_t
perf::HardwareInfo::physical_performance_counters_per_logical_core()
{
  if (HardwareInfo::_physical_performance_counters_per_logical_core.has_value()) {
    return HardwareInfo::_physical_performance_counters_per_logical_core.value();
  }

#if defined(__x86_64__) || defined(__i386__)
  if (HardwareInfo::is_intel()) {
    /// Read CPUID information with 0x0A (see https://www.felixcloutier.com/x86/cpuid).
    if (const auto pmu_info = HardwareInfo::cpuid(0x0A); pmu_info.has_value()) {
      /// Number of general-purpose performance monitoring counter per logical processor is in bits 15-08.
      const auto performance_counters_per_logical_core = (pmu_info->eax >> 8) & 0xFF;

      return HardwareInfo::cache_value(HardwareInfo::_physical_performance_counters_per_logical_core,
                                       static_cast<std::uint8_t>(performance_counters_per_logical_core));
    }
  }

  if (HardwareInfo::is_amd()) {
    /// Check the Extended Processor Information (0x80000001), see
    /// http://www.flounder.com/cpuid_explorer2.htm#CPUID(0x80000001):ECX.
    if (const auto extended_processor_info = HardwareInfo::cpuid(0x80000001);
        extended_processor_info.has_value() &&
        static_cast<bool>(extended_processor_info->ecx & (static_cast<std::uint32_t>(1U) << 23))) {

      /// Check the Extended Information (0x80000000), see
      /// http://www.flounder.com/cpuid_explorer2.htm#CPUID(0x80000000).
      if (const auto extended_info = HardwareInfo::cpuid(0x80000000);
          extended_info.has_value() && extended_info->eax >= 0x80000022) {

        /// Check the Performance Monitoring Unit Information (0x80000022).
        if (const auto pmu_info = HardwareInfo::cpuid(0x80000022); pmu_info.has_value()) {
          const auto performance_counters_per_logical_core = pmu_info->eax & 0xFF;

          return HardwareInfo::cache_value(HardwareInfo::_physical_performance_counters_per_logical_core,
                                           static_cast<std::uint8_t>(performance_counters_per_logical_core));
        }
      }
    }
  }
#endif

  /// Try to find the number of hardware counters per logical core.
  const auto hardware_counters = HardwareInfo::explore_hardware_counters_experimentally(true);

  /// Fallback: Set to one, if the experiment failed.
  if (!hardware_counters.has_value()) {
    return HardwareInfo::cache_value(HardwareInfo::_physical_performance_counters_per_logical_core,
                                     static_cast<std::uint8_t>(0U));
  }

  return HardwareInfo::cache_value(HardwareInfo::_physical_performance_counters_per_logical_core,
                                   hardware_counters.value());
}

#if defined(__x86_64__) || defined(__i386__)
std::optional<perf::HardwareInfo::CPUIDResult>
perf::HardwareInfo::cpuid(const std::uint32_t leaf, const std::uint32_t sub_leaf) noexcept
{

  auto result = CPUIDResult{};
  if (__get_cpuid_count(leaf, sub_leaf, &result.eax, &result.ebx, &result.ecx, &result.edx) > 0) {
    return result;
  }

  return std::nullopt;
}
#endif

std::uint8_t
perf::HardwareInfo::events_per_physical_performance_counter()
{
  if (HardwareInfo::_events_per_physical_performance_counter.has_value()) {
    return HardwareInfo::_events_per_physical_performance_counter.value();
  }

  /// Try to find the number of events per physical performance counter.
  if (const auto events_per_hardware_counter = HardwareInfo::explore_hardware_counters_experimentally(false);
      events_per_hardware_counter.has_value()) {
    return HardwareInfo::cache_value(HardwareInfo::_events_per_physical_performance_counter,
                                     events_per_hardware_counter.value());
  }

  /// Fallback: Set to one, if the experiment failed.
  return HardwareInfo::cache_value(HardwareInfo::_events_per_physical_performance_counter,
                                   static_cast<std::uint8_t>(1U));
}

std::uint64_t
perf::HardwareInfo::max_cpu_clock_frequency()
{
  if (HardwareInfo::_max_cpu_clock_frequency.has_value()) {
    return HardwareInfo::_max_cpu_clock_frequency.value();
  }

  auto max_frequency_in_hz = 0UL;

  for (const auto& entry : std::filesystem::directory_iterator("/sys/devices/system/cpu")) {
    if (entry.is_directory()) {
      if (auto cpu_directory_name = entry.path().filename().string();
          cpu_directory_name.rfind("cpu", 0U) == 0U && std::isdigit(cpu_directory_name[3U]) != 0) {
        auto freq_file = std::ifstream{ entry.path() / "cpufreq/cpuinfo_max_freq" };
        if (auto frequency_in_khz = 0UL; freq_file >> frequency_in_khz) {
          max_frequency_in_hz = std::max(max_frequency_in_hz, frequency_in_khz * 1000UL);
        }
      }
    }
  }

  if (max_frequency_in_hz == 0ULL) {
    throw CannotReadMaxClockFrequency{};
  }

  return HardwareInfo::cache_value(HardwareInfo::_max_cpu_clock_frequency, max_frequency_in_hz);
}

std::optional<std::uint8_t>
perf::HardwareInfo::explore_hardware_counters_experimentally(const bool is_identify_hardware_counters)
{
  /// Translate event names into codes.
  auto events = HardwareInfo::generate_events_for_counter_identification();

  for (auto number_events = static_cast<std::uint8_t>(1U); number_events <= static_cast<std::uint8_t>(events.size());
       ++number_events) {
    auto group = Group{};

    /// Depending on what we want to find, we use either (a) only one event per hardware counter or (b) only one
    /// hardware counter with multiple events.
    auto config = is_identify_hardware_counters ? Config{ /* max groups */ number_events, /* max events */ 1U }
                                                : Config{ /* max groups */ 1U, /* max events */ number_events };

    /// Add the number of events to the group.
    for (auto i = 0U; i < number_events; ++i) {
      group.add(events[i]);
    }

    try {
      /// Try to open the performance counter via the perf subsystem.
      group.open(config);
    } catch (const CannotOpenCounterError& error) {

      /// If the perf subsystem fails with error code EINVAL, it is likely that we hit the number. However, if we only
      /// added a single counter, we another issue seems to cause the error.
      if (error.error_code() == EINVAL && number_events > 1U) {
        return number_events > 2U ? number_events - 2U : number_events;
      }

      return std::nullopt;
    } catch (const std::runtime_error& /*error*/) {
      /// For every other error, we cannot tell the reason.
      return std::nullopt;
    }
  }

  return events.size();
}

std::vector<perf::CounterConfig>
perf::HardwareInfo::generate_events_for_counter_identification()
{
  /// List of event codes.
  auto event_codes = std::vector<CounterConfig>{};
  event_codes.reserve(Group::MAX_MEMBERS);

  /// Fetch all PMU names that are registered.
  const auto pmu_names = CounterDefinition::global().pmu_names();

  /// Check if ARM events are registered. Some ARM CPUs do not support all events provided by the perf subsystem. Hence,
  /// we rely on the events coming from the ARM pmu.
  if (const auto arm_pmu_name =
        std::find_if(pmu_names.begin(),
                     pmu_names.end(),
                     [](const std::string& name) { return name.length() >= 3 && name.substr(0, 3) == "arm"; });
      arm_pmu_name != pmu_names.end()) {
    const auto events = CounterDefinition::global().pmu(*arm_pmu_name);

    /// Translate events into codes.
    for (auto i = 0U; i < std::min<std::size_t>(events.size(), Group::MAX_MEMBERS); ++i) {
      event_codes.push_back(std::get<1>(events[i]));
    }

    return event_codes;
  }

  /// If we could not detect an ARM PMU, we use events provided "cpu" PMU.
  for (const auto& [name, config] : CounterDefinition::global().pmu("cpu")) {

    /// Try to open the event on a physical performance counter.
    try {
      auto counter = Counter{ config };

      /// Open as a single (non-live) event on a performance counter.
      counter.open(Config{ 1U, 1U }, false);

      /// If the open() call did not throw an exception, we can use the event.
      event_codes.push_back(config);

      /// Check if we reached the limit.
      if (event_codes.size() == Group::MAX_MEMBERS) {
        return event_codes;
      }
    } catch (CannotOpenCounterError&) {
      /// We do not handle the counter as some events will definitely lead to an exception, as not all events provided
      /// by the perf subsystem are supported on any hardware.
    }
  }

  return event_codes;
}