#pragma once

#include <cstdint>
#include <optional>
#include <string>
#if defined(__x86_64__) || defined(__i386__)
#include <cpuid.h>
#endif

#if !(defined(__x86_64__) || defined(__i386__))
#define __builtin_cpu_is(x) 0
#endif

namespace perf {
/**
 * Access to information about the underlying hardware substrate like manufacturer and perf specifics.
 */
class HardwareInfo
{
public:
  /**
   * @return True, if the underlying hardware is an Intel processor.
   */
  [[nodiscard]] static bool is_intel() noexcept { return static_cast<bool>(__builtin_cpu_is("intel")); }

  /**
   * @return True, if the underlying Intel processor requires an aux counter for memory sampling.
   */
  [[nodiscard]] static bool is_intel_aux_counter_required() noexcept
  {
#if (defined(__GNUC__) && __GNUC__ > 10) || (defined(__clang__) &&  __clang_major__ > 11)
    /// "sapphirerapids" and "alderlake" is only supported since clang-12 and gcc-11
    if (is_intel()) {
      return static_cast<bool>(__builtin_cpu_is("sapphirerapids")) || static_cast<bool>(__builtin_cpu_is("alderlake"));
    }
#endif

    return false;
  }

  /**
   * @return The id of Intel's PEBS "mem-loads-aux" event.
   */
  [[nodiscard]] static std::optional<std::uint64_t> intel_pebs_mem_loads_aux_event_id();

  /**
   * @return The id of Intel's PEBS "mem-loads" event.
   */
  [[nodiscard]] static std::optional<std::uint64_t> intel_pebs_mem_loads_event_id();

  /**
   * @return The id of Intel's PEBS "mem-stores" event.
   */
  [[nodiscard]] static std::optional<std::uint64_t> intel_pebs_mem_stores_event_id();

  /**
   * @return True, if the underlying hardware is an AMD processor.
   */
  [[nodiscard]] static bool is_amd() noexcept { return static_cast<bool>(__builtin_cpu_is("amd")); }

  /**
   * @return True, if the underlying AMD processor supports Instruction Based Sampling (IBS).
   */
  [[nodiscard]] static bool is_amd_ibs_supported() noexcept
  {
#if defined(__x86_64__) || defined(__i386__)
    /// See https://github.com/jlgreathouse/AMD_IBS_Toolkit/blob/master/ibs_with_perf_events.txt
    if (is_amd()) {
      std::uint32_t eax, ebx, ecx, edx;

      if (__get_cpuid_count(0x80000001, 0, &eax, &ebx, &ecx, &edx)) {
        return static_cast<bool>(ecx & (std::uint32_t(1U) << 10U));
      }
    }
#endif
    return false;
  }

  /**
   * @return True, if the underlying AMD processor supports Instruction Based Sampling (IBS) with L3 filter.
   */
  [[nodiscard]] static bool is_ibs_l3_filter_supported() noexcept
  {
#if defined(__x86_64__) || defined(__i386__)
    if (is_amd_ibs_supported()) {
      std::uint32_t eax, ebx, ecx, edx;

      if (__get_cpuid_count(0x8000001b, 0, &eax, &ebx, &ecx, &edx)) {
        return static_cast<bool>(eax & (std::uint32_t(1U) << 11U));
      }
    }
#endif
    return false;
  }

  /**
   * @return The config type for IBS execution counter, if IBS is supported by the underlying hardware.
   */
  [[nodiscard]] static std::optional<std::uint32_t> amd_ibs_op_type();

  /**
   * @return The config type for IBS fetch counter, if IBS is supported by the underlying hardware.
   */
  [[nodiscard]] static std::optional<std::uint32_t> amd_ibs_fetch_type();

private:
  /**
   * Tries to read event and umask from the provided file.
   *
   * @param path Path to the file.
   * @return Integer representation of event and umask.
   */
  [[nodiscard]] static std::optional<std::uint64_t> parse_event_umask_from_file(std::string&& path);
};
}