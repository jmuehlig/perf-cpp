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
  if (!HardwareInfo::_is_intel_aux_counter_required_cache.has_value()) {
    if (HardwareInfo::is_intel()) {
      HardwareInfo::_is_intel_aux_counter_required_cache =
        std::filesystem::exists(std::filesystem::path("/sys/bus/event_source/devices/cpu/events/mem-loads-aux")) ||
        std::filesystem::exists(std::filesystem::path("/sys/bus/event_source/devices/cpu_core/events/mem-loads-aux"));
    } else {
      HardwareInfo::_is_intel_aux_counter_required_cache = false;
    }
  }

  return HardwareInfo::_is_intel_aux_counter_required_cache.value();
}

bool
perf::HardwareInfo::is_intel_12th_generation_or_newer()
{
#if defined(__x86_64__) || defined(__i386__)
  if (!HardwareInfo::_is_intel_12th_generation_or_newer_cache.has_value()) {
    if (!HardwareInfo::is_intel()) {
      HardwareInfo::_is_intel_12th_generation_or_newer_cache = false;
    } else {
      std::uint32_t eax, ebx, ecx, edx;

      // Get processor family/model information
      __get_cpuid(1, &eax, &ebx, &ecx, &edx);

      // Check the family.
      const auto family_id = (eax >> 8) & 0xF;
      const auto extended_family_id = (eax >> 20) & 0xFF;
      const auto display_family = family_id + extended_family_id;

      /// Families < 6 are older than Alder Lake (12th generation); families > 6 are newer (and do not exist up to now).
      if (display_family != 6U) {
        HardwareInfo::_is_intel_12th_generation_or_newer_cache = display_family > 6U;
      }

      /// For family 6, check the model: models > 0x97 (Alder Lake) are newer or equal to Alder Lake.
      else {

        const auto model = (eax >> 4) & 0xF;
        const auto extended_model = (eax >> 16) & 0xF;

        const auto display_model = (extended_model << 4) + model;
        HardwareInfo::_is_intel_12th_generation_or_newer_cache = display_model >= /* Alder Lake */ 0x97;
      }
    }
  }

  return HardwareInfo::_is_intel_12th_generation_or_newer_cache.value();
#else
  return false;
#endif
}

bool
perf::HardwareInfo::is_amd_ibs_supported() noexcept
{
#if defined(__x86_64__) || defined(__i386__)
  if (!perf::HardwareInfo::_is_amd_ibs_supported_cache.has_value()) {
    /// See https://github.com/jlgreathouse/AMD_IBS_Toolkit/blob/master/ibs_with_perf_events.txt
    if (is_amd()) {
      std::uint32_t eax, ebx, ecx, edx;

      if (__get_cpuid_count(0x80000001, 0, &eax, &ebx, &ecx, &edx)) {
        perf::HardwareInfo::_is_amd_ibs_supported_cache = static_cast<bool>(ecx & (std::uint32_t(1U) << 10));
      } else {
        perf::HardwareInfo::_is_amd_ibs_supported_cache = false;
      }
    } else {
      perf::HardwareInfo::_is_amd_ibs_supported_cache = false;
    }
  }

  return perf::HardwareInfo::_is_amd_ibs_supported_cache.value();
#else
  return false;
#endif
}

bool
perf::HardwareInfo::is_ibs_l3_filter_supported() noexcept
{
#if defined(__x86_64__) || defined(__i386__)
  if (!perf::HardwareInfo::_is_ibs_l3_filter_supported_cache.has_value()) {
    if (is_amd_ibs_supported()) {
      std::uint32_t eax, ebx, ecx, edx;

      if (__get_cpuid_count(0x8000001b, 0, &eax, &ebx, &ecx, &edx)) {
        perf::HardwareInfo::_is_ibs_l3_filter_supported_cache = static_cast<bool>(eax & (std::uint32_t(1U) << 11));
      } else {
        perf::HardwareInfo::_is_ibs_l3_filter_supported_cache = false;
      }
    } else {
      perf::HardwareInfo::_is_ibs_l3_filter_supported_cache = false;
    }
  }

  return perf::HardwareInfo::_is_ibs_l3_filter_supported_cache.value();
#else
  return false;
#endif
}

std::uint64_t
perf::HardwareInfo::memory_page_size()
{
  if (!HardwareInfo::_memory_page_size_cache.has_value()) {
    HardwareInfo::_memory_page_size_cache = std::uint64_t(std::max(0L, ::sysconf(_SC_PAGESIZE)));
  }

  return HardwareInfo::_memory_page_size_cache.value();
}
