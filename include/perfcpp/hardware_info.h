#pragma once

#include <cstdint>
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
  [[nodiscard]] static bool is_intel_aux_counter_required();

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
   * @return The page size of memory of the underlying machine.
   */
  [[nodiscard]] static std::uint64_t memory_page_size();
};
}