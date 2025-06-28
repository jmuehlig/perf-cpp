#include <algorithm>
#include <errno.h>
#include <filesystem>
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

bool
perf::HardwareInfo::is_intel_aux_counter_required()
{
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

  // Get processor family/model information
  std::uint32_t eax, ebx, ecx, edx;
  if (__get_cpuid(1, &eax, &ebx, &ecx, &edx) == 0) {
    return HardwareInfo::cache_value(HardwareInfo::_is_intel_12th_generation_or_newer, false);
  }

  // Check the family.
  const auto family_id = (eax >> 8) & 0xF;
  const auto extended_family_id = (eax >> 20) & 0xFF;

  /// Families < 6 are older than Alder Lake (12th generation); families > 6 are newer (and do not exist up to now).
  if (const auto display_family = family_id + extended_family_id; display_family != 6U) {
    return HardwareInfo::cache_value(HardwareInfo::_is_intel_12th_generation_or_newer, display_family > 6U);
  }

  /// For family 6, check the model.
  const auto model = (eax >> 4) & 0xF;
  const auto extended_model = (eax >> 16) & 0xF;

  const auto display_model = (extended_model << 4) + model;
  return HardwareInfo::cache_value(HardwareInfo::_is_intel_12th_generation_or_newer, display_model >= 143U);

#else
  return false;
#endif
}

bool
perf::HardwareInfo::is_amd_ibs_supported() noexcept
{
#if defined(__x86_64__) || defined(__i386__)
  if (HardwareInfo::_is_amd_ibs_supported.has_value()) {
    return HardwareInfo::_is_amd_ibs_supported.value();
  }

  if (!HardwareInfo::is_amd()) {
    return HardwareInfo::cache_value(HardwareInfo::_is_amd_ibs_supported, false);
  }

  /// See https://github.com/jlgreathouse/AMD_IBS_Toolkit/blob/master/ibs_with_perf_events.txt
  std::uint32_t eax, ebx, ecx, edx;
  if (__get_cpuid_count(0x80000001, 0, &eax, &ebx, &ecx, &edx) > 0) {
    return HardwareInfo::cache_value(HardwareInfo::_is_amd_ibs_supported,
                                     static_cast<bool>(ecx & (std::uint32_t(1U) << 10)));
  }

  return HardwareInfo::cache_value(HardwareInfo::_is_amd_ibs_supported, false);
#else
  return false;
#endif
}

bool
perf::HardwareInfo::is_ibs_l3_filter_supported() noexcept
{
#if defined(__x86_64__) || defined(__i386__)
  if (HardwareInfo::_is_ibs_l3_filter_supported.has_value()) {
    return HardwareInfo::_is_ibs_l3_filter_supported.value();
  }

  if (!HardwareInfo::is_amd_ibs_supported()) {
    return HardwareInfo::cache_value(HardwareInfo::_is_ibs_l3_filter_supported, false);
  }

  std::uint32_t eax, ebx, ecx, edx;
  if (__get_cpuid_count(0x8000001b, 0, &eax, &ebx, &ecx, &edx) > 0) {
    const auto is_ibs_l3_filter_supported = static_cast<bool>(eax & (std::uint32_t(1U) << 11));
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

  const auto memory_page_size = std::uint64_t(std::max(0L, ::sysconf(_SC_PAGESIZE)));
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
    std::uint32_t eax, ebx, ecx, edx;

    /// Read CPUID information with 0x0A (see https://www.felixcloutier.com/x86/cpuid).
    if (__get_cpuid_count(0x0A, 0, &eax, &ebx, &ecx, &edx) > 0) {
      /// Number of general-purpose performance monitoring counter per logical processor is in bits 15-08.
      const auto performance_counters_per_logical_core = (eax >> 8) & 0xFF;

      return HardwareInfo::cache_value(HardwareInfo::_physical_performance_counters_per_logical_core,
                                       std::uint8_t(performance_counters_per_logical_core));
    }
  }

  if (HardwareInfo::is_amd()) {
    std::uint32_t eax, ebx, ecx, edx;

    if (__get_cpuid_count(0x80000001, 0, &eax, &ebx, &ecx, &edx) > 0 && ecx & (std::uint32_t(1U) << 23)) {
      if (__get_cpuid_count(0x80000000, 0, &eax, &ebx, &ecx, &edx) > 0 && eax >= 0x80000022) {
        if (__get_cpuid_count(0x80000022, 0, &eax, &ebx, &ecx, &edx) > 0) {
          const auto performance_counters_per_logical_core = eax & 0xFF;

          return HardwareInfo::cache_value(HardwareInfo::_physical_performance_counters_per_logical_core,
                                           std::uint8_t(performance_counters_per_logical_core));
        }
      }
    }
  }
// #elif defined(__aarch64__)
//   std::uint64_t pmcr_el0_value;
//
//   /// Aarch64 uses the Performance Monitors Control Register PMCR_EL0 (see
//   ///
//   https://developer.arm.com/documentation/ddi0601/2025-03/AArch64-Registers/PMCR-EL0--Performance-Monitors-Control-Register).
//   __asm__ volatile("mrs %0, pmcr_el0" : "=r"(pmcr_el0_value));
//
//   /// The number of counters is in bits 15-11.
//   const auto performance_counters_per_logical_core = (pmcr_el0_value >> 11) & 0x1F;
//
//   return HardwareInfo::cache_value(HardwareInfo::_physical_performance_counters_per_logical_core,
//                                    std::uint8_t(performance_counters_per_logical_core));
#endif

  /// Try to find the number of hardware counters per logical core.
  const auto hardware_counters =
    HardwareInfo::identify_hardware_counters_per_cpu_or_events_per_hardware_counter_experimentally(true);

  /// Fallback: Set to one, if the experiment failed.
  if (!hardware_counters.has_value()) {
    return HardwareInfo::cache_value(HardwareInfo::_physical_performance_counters_per_logical_core, std::uint8_t(0U));
  }

  return HardwareInfo::cache_value(HardwareInfo::_physical_performance_counters_per_logical_core,
                                   hardware_counters.value());

  return 0U;
}

std::uint8_t
perf::HardwareInfo::events_per_physical_performance_counter()
{
  if (HardwareInfo::_events_per_physical_performance_counter.has_value()) {
    return HardwareInfo::_events_per_physical_performance_counter.value();
  }

  /// Try to find the number of events per physical performance counter.
  const auto events_per_physical_performance_counter =
    HardwareInfo::identify_hardware_counters_per_cpu_or_events_per_hardware_counter_experimentally(false);

  /// Fallback: Set to one, if the experiment failed.
  if (!events_per_physical_performance_counter.has_value()) {
    return HardwareInfo::cache_value(HardwareInfo::_events_per_physical_performance_counter, std::uint8_t(1U));
  }

  return HardwareInfo::cache_value(HardwareInfo::_events_per_physical_performance_counter,
                                   events_per_physical_performance_counter.value());
}

std::optional<std::uint8_t>
perf::HardwareInfo::identify_hardware_counters_per_cpu_or_events_per_hardware_counter_experimentally(
  const bool is_identify_hardware_counters)
{
  /// Translate event names into codes.
  auto events = HardwareInfo::generate_events_for_counter_identification();

  for (auto number_events = std::uint8_t(1U); number_events <= std::uint8_t(events.size()); ++number_events) {
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

      /// If the perf subsystem fails with error code EINVAL, it is likely that we hit the number.
      if (error.error_code() == EINVAL) {

        /// However, if we only added a single counter, we another issue seems to cause the error.
        if (number_events > 1U) {
          return --number_events;
        }
      }

      return std::nullopt;
    } catch (const std::runtime_error& error) {
      /// For every other error, we cannot tell the reason.
      return std::nullopt;
    }
  }

  return events.size();
}

std::vector<perf::CounterConfig>
perf::HardwareInfo::generate_events_for_counter_identification()
{
  /// Maximum number of events we need to experiment.
  constexpr auto max_events = 12UL;

  /// List of event codes.
  auto event_codes = std::vector<CounterConfig>{};
  event_codes.reserve(max_events);

  const auto counter_definition = CounterDefinition{};

  /// Fetch all PMU names that are registered.
  const auto pmu_names = counter_definition.pmu_names();

  /// Check if ARM events are registered. Some ARM CPUs do not support all events provided by the perf subsystem. Hence,
  /// we rely on the events coming from the ARM pmu.
  if (const auto arm_pmu_name =
        std::find_if(pmu_names.begin(),
                     pmu_names.end(),
                     [](const std::string& name) { return name.length() >= 3 && name.substr(0, 3) == "arm"; });
      arm_pmu_name != pmu_names.end()) {
    const auto events = counter_definition.pmu(*arm_pmu_name);

    /// Translate events into codes.
    for (auto i = 0U; i < std::max(events.size(), max_events); ++i) {
      event_codes.push_back(std::get<1>(events[i]));
    }

    return event_codes;
  }

  /// If we could not detect an ARM PMU, we use events provided by the perf subsystem.
  const auto event_names = std::vector<std::string>{ "instructions",    "cycles",
                                                     "branches",        "branch-misses",
                                                     "cache-misses",    "cache-references",
                                                     "L1-dcache-loads", "L1-dcache-load-misses",
                                                     "L1-icache-loads", "L1-icache-load-misses",
                                                     "dTLB-loads",      "dTLB-load-misses",
                                                     "iTLB-loads",      "iTLB-load-misses" };

  /// Translate event names into configurations.
  for (const auto& name : event_names) {
    const auto event_config = counter_definition.counter("cpu", name);
    /// Verify that the event is available. Since these events are defined by the perf subsystem, they should be
    /// available.
    if (event_config.has_value()) {
      event_codes.push_back(std::get<2>(event_config.value()));

      if (event_codes.size() == max_events) {
        break;
      }
    }
  }

  return event_codes;
}