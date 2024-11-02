#pragma once

#include "config.h"
#include "counter.h"

namespace perf {
/**
 * A group presents a set of counters where the first counter is the group leader.
 * All counters can be started and stopped together, not individually.
 */
class Group
{
public:
  /// Number of maximal members per group.
  constexpr static inline auto MAX_MEMBERS = 8U;

  Group() = default;
  Group(Group&&) noexcept = default;
  Group(const Group&) = default;

  ~Group() = default;

  /**
   * Adds the given event to the group.
   *
   * @param counter Event to add.
   * @return True, if the event could be added.
   */
  bool add(CounterConfig counter);

  /**
   * Opens all counters of the group, configured by the provided config.
   *
   * @param config Configuration.
   * @param is_read_format True, if counters should be read.
   * @param is_sample True, if counter should be configured for sampling. Some counters (e.g., live readable counters) can have a sample type without being sampled.
   * @param has_auxiliary_event True, if the group has an auxiliary event as a first event.
   * @param buffer_pages Number of pages allocated for user-level buffer, std::nullopt if counter should not allocated
   * any pages.
   * @param sample_type Mask of sampled values, std::nullopt of sampling is disabled.
   * @param branch_type Mask of sampled branch types, std::nullopt of sampling is disabled.
   * @param user_registers Mask of sampled user registers, std::nullopt of sampling is disabled.
   * @param kernel_registers Mask of sampled kernel registers, std::nullopt of sampling is disabled.
   * @param max_callstack Maximal size of sampled callstacks, std::nullopt of sampling is disabled.
   * @param is_include_context_switch True, if context switches should be sampled, ignored if sampling is disabled.
   * @param is_include_cgroup True, if cgroups should be sampled, ignored if sampling is disabled.
   *
   * @return True, if the counters could be opened.
   */
  bool open(const Config& config,
            bool is_read_format,
            bool is_sample,
            bool has_auxiliary_event,
            std::optional<std::uint64_t> buffer_pages,
            std::optional<std::uint64_t> sample_type,
            std::optional<std::uint64_t> branch_type,
            std::optional<std::uint64_t> user_registers,
            std::optional<std::uint64_t> kernel_registers,
            std::optional<std::uint16_t> max_callstack,
            bool is_include_context_switch,
            bool is_include_cgroup);

  /**
   * Closes all counters of the group.
   */
  void close();

  /**
   * Starts monitoring the counters in the group.
   *
   * @return True, if the counters could be started.
   */
  bool start();

  /**
   * Enables the group to start monitoring.
   */
  void enable() const;

  /**
   * Stops monitoring of all counters in the group.
   *
   * @return True, if the counters could be stopped.
   */
  bool stop();

  /**
   * Disables the group to stop monitoring.
   */
  void disable() const;

  /**
   * Reads the counter into the given value.
   *
   * @param value Value to read the counters into.
   * @return True, if reading was successful.
   */
  [[nodiscard]] bool read(CounterReadFormat<MAX_MEMBERS>& value) const;

  /**
   * @return Number of counters in the group.
   */
  [[nodiscard]] std::size_t size() const noexcept { return _members.size(); }

  /**
   * @return True, if the group is empty.
   */
  [[nodiscard]] bool empty() const noexcept { return _members.empty(); }

  /**
   * Reads the result of counter at the given index.
   *
   * @param index Index of the counter to read the result for.
   * @return Result of the counter.
   */
  [[nodiscard]] double get(std::size_t index) const;

  /**
   * Performs a "lightweight" read of the group leader without stopping/starting the counter.
   *
   * @return The current value of the group leader.
   */
  [[nodiscard]] std::uint64_t lget() const { return !_members.empty() ? _members.front().lread() : 0ULL; }

  /**
   * Grants access to the counter at the given index.
   *
   * @param index Index of the counter.
   * @return Counter.
   */
  [[nodiscard]] Counter& member(const std::size_t index) { return _members[index]; }

  /**
   * Grants access to the counter at the given index.
   *
   * @param index Index of the counter.
   * @return Counter.
   */
  [[nodiscard]] const Counter& member(const std::size_t index) const { return _members[index]; }

  /**
   * @return List of all members in the group.
   */
  [[nodiscard]] std::vector<Counter>& members() { return _members; }

  /**
   * @return User-level buffer of the first counter (if not nullptr) or the second counter.
   */
  [[nodiscard]] perf_event_mmap_page* user_level_buffer() const noexcept
  {
    if (!_members.empty()) {
      if (_members[0U].user_level_buffer() != nullptr) {
        return _members[0U].user_level_buffer();
      }

      if (_members.size() > 1U) {
        return _members[1U].user_level_buffer();
      }
    }

    return nullptr;
  }

private:
  /// List of all the group members.
  std::vector<Counter> _members;

  /// Start value of the hardware performance counters.
  CounterReadFormat<Group::MAX_MEMBERS> _start_value{};

  /// End value of the hardware performance counters.
  CounterReadFormat<Group::MAX_MEMBERS> _end_value{};

  /// After stopping the group, we calculate the multiplexing correction once from start- and end-values.
  double _multiplexing_correction{ 1. };

  /**
   * Reads the value of a specific counter (identified by the given ID) from the provided value set.
   *
   * @param counter_values Set of counter values.
   * @param id Identifier of the counter to read.
   *
   * @return The value of the specified counter or std::nullopt of the ID was not found.
   */
  [[nodiscard]] static std::optional<std::uint64_t> value_for_id(
    const CounterReadFormat<Group::MAX_MEMBERS>& counter_values,
    std::uint64_t id) noexcept;
};
}