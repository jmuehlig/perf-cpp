#include <algorithm>
#include <filesystem>
#include <perfcpp/hardware_info.h>
#include <unistd.h>

/// Cache variable to remember if Intel's auxiliary counter is required for sampling.
std::optional<bool> perf::HardwareInfo::_is_intel_aux_counter_required{ std::nullopt };

/// Cache variable to remember if the underlying Intel hardware is from the 12th generation or even newer; these
/// machines have heterogeneous CPUs and PMUs.
std::optional<bool> perf::HardwareInfo::_is_intel_12th_generation_or_newer{ std::nullopt };

/// Cache variable to remember AMD IBS is supported.
std::optional<bool> perf::HardwareInfo::_is_amd_ibs_supported{ std::nullopt };

/// Cache variable to remember if AMD's IBS supports filtering for L3 cache misses.
std::optional<bool> perf::HardwareInfo::_is_ibs_l3_filter_supported{ std::nullopt };

/// Cache variable to remember the memory page size.
std::optional<std::uint64_t> perf::HardwareInfo::_memory_page_size{ std::nullopt };

bool
perf::HardwareInfo::is_intel_aux_counter_required()
{
  if (HardwareInfo::_is_intel_aux_counter_required.has_value()) {
    return HardwareInfo::_is_intel_aux_counter_required.value();
  }

  if (!HardwareInfo::is_intel()) {
    return HardwareInfo::cache_value(HardwareInfo::_is_intel_aux_counter_required, false);
  }

  return HardwareInfo::cache_value(
    HardwareInfo::_is_intel_aux_counter_required,
    std::filesystem::exists(std::filesystem::path("/sys/bus/event_source/devices/cpu/events/mem-loads-aux")) ||
      std::filesystem::exists(std::filesystem::path("/sys/bus/event_source/devices/cpu_core/events/mem-loads-aux")));
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
  __get_cpuid(1, &eax, &ebx, &ecx, &edx);

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
    return HardwareInfo::cache_value(HardwareInfo::_is_ibs_l3_filter_supported,
                                     static_cast<bool>(eax & (std::uint32_t(1U) << 11)));
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

  return HardwareInfo::cache_value(HardwareInfo::_memory_page_size,
                                   std::uint64_t(std::max(0L, ::sysconf(_SC_PAGESIZE))));
}