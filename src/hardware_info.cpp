#include <algorithm>
#include <filesystem>
#include <perfcpp/hardware_info.h>
#include <unistd.h>

std::optional<bool> perf::HardwareInfo::_is_intel_aux_counter_required_cache{ std::nullopt };
std::optional<bool> perf::HardwareInfo::_is_intel_12th_generation_or_newer_cache{ std::nullopt };
std::optional<bool> perf::HardwareInfo::_is_amd_ibs_supported_cache{ std::nullopt };
std::optional<bool> perf::HardwareInfo::_is_ibs_l3_filter_supported_cache{ std::nullopt };
std::optional<std::uint64_t> perf::HardwareInfo::_memory_page_size_cache{ std::nullopt };

bool
perf::HardwareInfo::is_intel_aux_counter_required()
{
  if (HardwareInfo::_is_intel_aux_counter_required_cache.has_value()) {
    return HardwareInfo::_is_intel_aux_counter_required_cache.value();
  }

  if (!HardwareInfo::is_intel()) {
    return HardwareInfo::cached_value(HardwareInfo::_is_intel_aux_counter_required_cache, false);
  }

  return HardwareInfo::cached_value(
    HardwareInfo::_is_intel_aux_counter_required_cache,
    std::filesystem::exists(std::filesystem::path("/sys/bus/event_source/devices/cpu/events/mem-loads-aux")) ||
      std::filesystem::exists(std::filesystem::path("/sys/bus/event_source/devices/cpu_core/events/mem-loads-aux")));
}

bool
perf::HardwareInfo::is_intel_12th_generation_or_newer()
{
#if defined(__x86_64__) || defined(__i386__)
  if (HardwareInfo::_is_intel_12th_generation_or_newer_cache.has_value()) {
    return HardwareInfo::_is_intel_12th_generation_or_newer_cache.value();
  }

  if (!HardwareInfo::is_intel()) {
    return HardwareInfo::cached_value(HardwareInfo::_is_intel_12th_generation_or_newer_cache, false);
  }

  // Get processor family/model information
  std::uint32_t eax, ebx, ecx, edx;
  __get_cpuid(1, &eax, &ebx, &ecx, &edx);

  // Check the family.
  const auto family_id = (eax >> 8) & 0xF;
  const auto extended_family_id = (eax >> 20) & 0xFF;

  /// Families < 6 are older than Alder Lake (12th generation); families > 6 are newer (and do not exist up to now).
  if (const auto display_family = family_id + extended_family_id; display_family != 6U) {
    return HardwareInfo::cached_value(HardwareInfo::_is_intel_12th_generation_or_newer_cache, display_family > 6U);
  }

  /// For family 6, check the model.
  const auto model = (eax >> 4) & 0xF;
  const auto extended_model = (eax >> 16) & 0xF;

  const auto display_model = (extended_model << 4) + model;
  return HardwareInfo::cached_value(HardwareInfo::_is_intel_12th_generation_or_newer_cache, display_model >= 143U);

#else
  return false;
#endif
}

bool
perf::HardwareInfo::is_amd_ibs_supported() noexcept
{
#if defined(__x86_64__) || defined(__i386__)
  if (HardwareInfo::_is_amd_ibs_supported_cache.has_value()) {
    return HardwareInfo::_is_amd_ibs_supported_cache.value();
  }

  if (!HardwareInfo::is_amd()) {
    return HardwareInfo::cached_value(HardwareInfo::_is_amd_ibs_supported_cache, false);
  }

  /// See https://github.com/jlgreathouse/AMD_IBS_Toolkit/blob/master/ibs_with_perf_events.txt
  std::uint32_t eax, ebx, ecx, edx;
  if (__get_cpuid_count(0x80000001, 0, &eax, &ebx, &ecx, &edx) > 0) {
    return HardwareInfo::cached_value(HardwareInfo::_is_amd_ibs_supported_cache,
                                      static_cast<bool>(ecx & (std::uint32_t(1U) << 10)));
  }

  return HardwareInfo::cached_value(HardwareInfo::_is_amd_ibs_supported_cache, false);
#else
  return false;
#endif
}

bool
perf::HardwareInfo::is_ibs_l3_filter_supported() noexcept
{
#if defined(__x86_64__) || defined(__i386__)
  if (HardwareInfo::_is_ibs_l3_filter_supported_cache.has_value()) {
    return HardwareInfo::_is_ibs_l3_filter_supported_cache.value();
  }

  if (!HardwareInfo::is_amd_ibs_supported()) {
    return HardwareInfo::cached_value(HardwareInfo::_is_ibs_l3_filter_supported_cache, false);
  }

  std::uint32_t eax, ebx, ecx, edx;
  if (__get_cpuid_count(0x8000001b, 0, &eax, &ebx, &ecx, &edx) > 0) {
    return HardwareInfo::cached_value(HardwareInfo::_is_ibs_l3_filter_supported_cache,
                                      static_cast<bool>(eax & (std::uint32_t(1U) << 11)));
  }

  return HardwareInfo::cached_value(HardwareInfo::_is_ibs_l3_filter_supported_cache, false);
#else
  return false;
#endif
}

std::uint64_t
perf::HardwareInfo::memory_page_size()
{
  if (HardwareInfo::_memory_page_size_cache.has_value()) {
    return HardwareInfo::_memory_page_size_cache.value();
  }

  return HardwareInfo::cached_value(HardwareInfo::_memory_page_size_cache,
                                    std::uint64_t(std::max(0L, ::sysconf(_SC_PAGESIZE))));
}