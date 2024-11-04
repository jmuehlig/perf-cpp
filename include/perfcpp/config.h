#pragma once

#include "branch.h"
#include "period.h"
#include "precision.h"
#include "registers.h"
#include <cstdint>
#include <optional>

namespace perf {
class Config
{
public:
  Config() noexcept = default;
  ~Config() noexcept = default;
  Config(const Config&) noexcept = default;
  Config& operator=(const Config&) noexcept = default;

  [[nodiscard]] std::uint8_t max_groups() const noexcept { return _max_groups; }
  [[nodiscard]] std::uint8_t max_counters_per_group() const noexcept { return _max_counters_per_group; }

  [[nodiscard]] bool is_include_child_threads() const noexcept { return _is_include_child_threads; }
  [[nodiscard]] bool is_include_kernel() const noexcept { return _is_include_kernel; }
  [[nodiscard]] bool is_include_user() const noexcept { return _is_include_user; }
  [[nodiscard]] bool is_include_hypervisor() const noexcept { return _is_include_hypervisor; }
  [[nodiscard]] bool is_include_idle() const noexcept { return _is_include_idle; }
  [[nodiscard]] bool is_include_guest() const noexcept { return _is_include_guest; }

  [[nodiscard]] bool is_debug() const noexcept { return _is_debug; }

  [[nodiscard]] std::optional<std::uint16_t> cpu_id() const noexcept { return _cpu_id; }
  [[nodiscard]] std::optional<pid_t> process_id() const noexcept { return _process_id; }

  /**
   * Specify the number of maximum groups per EventCounter.
   *
   * @param max_groups Number of maximum groups.
   */
  void max_groups(const std::uint8_t max_groups) noexcept { _max_groups = max_groups; }

  /**
   * Specify the maximum number of counters per group.
   *
   * @param max_counters_per_group Number of maximum hardware event counters per group.
   */
  void max_counters_per_group(const std::uint8_t max_counters_per_group) noexcept
  {
    _max_counters_per_group = max_counters_per_group;
  }

  /**
   * If set, child threads from the recording thread will be monitored.
   *
   * @param is_include_child_threads Flag indicating that child threads should be monitored.
   */
  void include_child_threads(const bool is_include_child_threads) noexcept
  {
    _is_include_child_threads = is_include_child_threads;
  }

  /**
   * If set, kernel-activity will be monitored.
   *
   * @param is_include_kernel Flag indicating that kernel-activity should be monitored.
   */
  void include_kernel(const bool is_include_kernel) noexcept { _is_include_kernel = is_include_kernel; }

  /**
   * If set, user-activity will be monitored.
   *
   * @param is_include_user Flag indicating that user-activity should be monitored.
   */
  void include_user(const bool is_include_user) noexcept { _is_include_user = is_include_user; }

  /**
   * If set, user-activity will be monitored.
   *
   * @param is_include_hypervisor Flag indicating that hypervisor-activity should be monitored.
   */
  void include_hypervisor(const bool is_include_hypervisor) noexcept { _is_include_hypervisor = is_include_hypervisor; }

  /**
   * If set, idle-activity will be monitored.
   *
   * @param is_include_idle Flag indicating that idle-activity should be monitored.
   */
  void include_idle(const bool is_include_idle) noexcept { _is_include_idle = is_include_idle; }

  /**
   * If set, guest-activity will be monitored.
   *
   * @param is_include_guest Flag indicating that guest-activity should be monitored.
   */
  void include_guest(const bool is_include_guest) noexcept { _is_include_guest = is_include_guest; }

  /**
   * If debug is set to true (false by default), the counter configuration will be dumped to the console upon opening
   * the counter (in both sampling and monitoring mode). This is especially useful when debugging counter
   * configurations.
   *
   * @param is_debug If set to true, counter configurations will be dumped to console.
   */
  void is_debug(const bool is_debug) noexcept { _is_debug = is_debug; }

  /**
   * If specified, the EventCounter or Sampler will monitor only that specified CPU.
   *
   * @param cpu_id CPU to monitor.
   */
  void cpu_id(const std::uint16_t cpu_id) noexcept { _cpu_id = cpu_id; }

  /**
   * If specified, the EventCounter or Sampler will only monitor that specified process.
   *
   * @param process_id Process to monitor.
   */
  void process_id(const pid_t process_id) noexcept { _process_id = process_id; }

private:
  std::uint8_t _max_groups{ 5U };
  std::uint8_t _max_counters_per_group{ 4U };

  bool _is_include_child_threads{ false };
  bool _is_include_kernel{ true };
  bool _is_include_user{ true };
  bool _is_include_hypervisor{ true };
  bool _is_include_idle{ true };
  bool _is_include_guest{ true };

  bool _is_debug{ false };

  std::optional<std::uint16_t> _cpu_id{ std::nullopt };
  std::optional<pid_t> _process_id{ std::nullopt };
};

class SampleConfig final : public Config
{
public:
  SampleConfig() noexcept = default;
  ~SampleConfig() noexcept = default;

  /**
   * @return Default precision for sampling.
   */
  [[nodiscard]] Precision precise_ip() const noexcept { return _precise_ip; }

  /**
   * @return Number of pages to allocate for the user-level buffer that receives samples.
   */
  [[nodiscard]] std::uint64_t buffer_pages() const noexcept { return _buffer_pages; }

  /**
   * @return Default period or frequency for sampling.
   */
  [[nodiscard]] PeriodOrFrequency period_for_frequency() const noexcept { return _period_or_frequency; }

  /**
   * Default frequency to sample, if not specified along with a trigger. The frequency denotes to samples per second.
   * Note that either frequency or period can be specified.
   *
   * @param frequency Frequency to sample (samples per second).
   */
  void frequency(const std::uint64_t frequency) noexcept { _period_or_frequency = Frequency{ frequency }; }

  /**
   * Default period to sample, if not specified along with a trigger. The period denotes to one sample every <period>
   * trigger events. Note that either frequency or period can be specified.
   *
   * @param period Period to sample (one sample every <period> eventy reported by the trigger).
   */
  void period(const std::uint64_t period) noexcept { _period_or_frequency = Period{ period }; }

  /**
   * Default precision for sampling, if not specified along with a trigger.
   *
   * @param precision Default precision for sampling.
   */
  void precise_ip(const Precision precision) noexcept { _precise_ip = precision; }

  /**
   * Default precision for sampling, if not specified along with a trigger.
   *
   * @param precision Default precision for sampling.
   */
  void precision(const Precision precision) noexcept { _precise_ip = precision; }

  /**
   * Default precision for sampling, if not specified along with a trigger.
   *
   * @param precision Default precision for sampling.
   */
  void precise_ip(const std::uint8_t precise_ip) noexcept
  {
    switch (precise_ip) {
      case 0U:
        _precise_ip = Precision::AllowArbitrarySkid;
        return;
      case 1U:
        _precise_ip = Precision::MustHaveConstantSkid;
        return;
      case 2U:
        _precise_ip = Precision::RequestZeroSkid;
        return;
      default:
        _precise_ip = Precision::MustHaveZeroSkid;
    }
  }

  /**
   * Specifies the number of pages allocated for the user-level buffer that receives samples.
   *
   * @param buffer_pages Number of pages allocated for the user-level buffer that receives samples.
   */
  void buffer_pages(const std::uint64_t buffer_pages) noexcept { _buffer_pages = buffer_pages; }

private:
  /// Number of pages allocated for the user-level buffer.
  std::uint64_t _buffer_pages{ 8192U + 1U };

  /// Default frequency or period, if not specified for a trigger.
  PeriodOrFrequency _period_or_frequency{ Period{ 4000U } };

  /// Default precision for sampling, if not specified for a trigger.
  Precision _precise_ip{ Precision::MustHaveConstantSkid /* Enable PEBS by default */ };
};
}