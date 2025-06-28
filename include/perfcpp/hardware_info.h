#pragma once
#include <optional>

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
   * @return True, if the underlying Intel processor is equal or newer than the 12th generation.
   */
  [[nodiscard]] static bool is_intel_12th_generation_or_newer();

  /**
   * @return True, if the underlying hardware is an AMD processor.
   */
  [[nodiscard]] static bool is_amd() noexcept { return static_cast<bool>(__builtin_cpu_is("amd")); }

  /**
   * @return True, if the underlying AMD processor supports Instruction Based Sampling (IBS).
   */
  [[nodiscard]] static bool is_amd_ibs_supported() noexcept;

  /**
   * @return True, if the underlying AMD processor supports Instruction Based Sampling (IBS) with L3 filter.
   */
  [[nodiscard]] static bool is_ibs_l3_filter_supported() noexcept;

  /**
   * @return The page size of memory of the underlying machine.
   */
  [[nodiscard]] static std::uint64_t memory_page_size();

  /**
   * @return The number of physical performance counters per logical CPU core.
   */
  [[nodiscard]] static std::uint8_t physical_performance_counters_per_logical_core();

  /**
   * @return The number of events that can be scheduled to the same physical performance counter.
   */
  [[nodiscard]] static std::uint8_t events_per_physical_performance_counter();

private:
  static std::optional<bool> _is_intel_aux_event_required;
  static std::optional<bool> _is_intel_12th_generation_or_newer;
  static std::optional<bool> _is_amd_ibs_supported;
  static std::optional<bool> _is_ibs_l3_filter_supported;
  static std::optional<std::uint64_t> _memory_page_size;
  static std::optional<std::uint8_t> _physical_performance_counters_per_logical_core;
  static std::optional<std::uint8_t> _events_per_physical_performance_counter;

  /**
   * Writes a value into the cache variable and returns the value.
   *
   * @param variable Cache variable.
   * @param value Value to write into the cache variable.
   * @return The cached value.
   */
  template<typename T>
  [[nodiscard]] static T cache_value(std::optional<T>& variable, const T value)
  {
    variable = value;
    return value;
  }

  /**
   * Tries to open a performance counter with more and more events until it cannot open more events on a single physical
   * performance counter.
   *
   * @param is_identify_hardware_counters If true, identify the number of hardware counters. Otherwise, identify the
   * number of events per hardware counter.
   * @return The maximum number of events on a single physical performance counter.
   */
  [[nodiscard]] static std::optional<std::uint8_t>
  identify_hardware_counters_per_cpu_or_events_per_hardware_counter_experimentally(bool is_identify_hardware_counters);
};
}