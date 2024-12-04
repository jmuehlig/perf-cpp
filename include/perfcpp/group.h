#pragma once

#include "config.h"
#include "counter.h"

namespace perf {

/**
 * Format the counter values are stored by the perf subsystem for a single counter group.
 */
template<std::size_t S>
struct CounterValues
{
  /// Value and ID delivered by perf.
  struct value
  {
    std::uint64_t value;
    std::uint64_t id;
  };

  /// Number of counters in the following array.
  std::uint64_t count_members;

  /// Time the event was enabled.
  std::uint64_t time_enabled;

  /// Time the event was running.
  std::uint64_t time_running;

  /// Values of the members.
  std::array<value, S> values;
};

/**
 * A group presents a set of counters where the first counter is the "group leader".
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
   * @param has_auxiliary_event True, if the group has an auxiliary event as a first event.
   * @param buffer_pages Number of pages allocated for user-level buffer, std::nullopt if counter should not allocated
   * any pages.
   * @param sample_type Mask of sampled values, std::nullopt of sampling is disabled.
   * @param branch_type Mask of sampled branch types, std::nullopt of sampling is disabled.
   * @param user_registers Mask of sampled user registers, std::nullopt of sampling is disabled.
   * @param kernel_registers Mask of sampled kernel registers, std::nullopt of sampling is disabled.
   * @param max_user_stack_size Maximal size of sampled uer stack, std::nullopt of sampling is disabled.
   * @param max_callstack_size Maximal size of sampled callstacks, std::nullopt of sampling is disabled.
   * @param is_include_context_switch True, if context switches should be sampled, ignored if sampling is disabled.
   * @param is_include_cgroup True, if cgroups should be sampled, ignored if sampling is disabled.
   * @return True, if the counters could be opened.
   */
  bool open(const Config& config,
            bool is_read_format,
            bool has_auxiliary_event,
            std::optional<std::uint64_t> buffer_pages,
            std::optional<std::uint64_t> sample_type,
            std::optional<std::uint64_t> branch_type,
            std::optional<std::uint64_t> user_registers,
            std::optional<std::uint64_t> kernel_registers,
            std::optional<std::uint32_t> max_user_stack_size,
            std::optional<std::uint16_t> max_callstack_size,
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
   * @param values Value to read the counters into.
   * @return True, if reading was successful.
   */
  [[nodiscard]] bool read(CounterValues<MAX_MEMBERS>& values) const noexcept;

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
  [[nodiscard]] double get(std::size_t index) const noexcept;

  /**
   * Grants access to the counter at the given index.
   *
   * @param index Index of the counter.
   * @return Counter.
   */
  [[nodiscard]] Counter& member(const std::size_t index) noexcept { return _members[index]; }

  /**
   * Grants access to the counter at the given index.
   *
   * @param index Index of the counter.
   * @return Counter.
   */
  [[nodiscard]] const Counter& member(const std::size_t index) const noexcept { return _members[index]; }

  /**
   * @return List of all members in the group.
   */
  [[nodiscard]] std::vector<Counter>& members() noexcept { return _members; }

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
  CounterValues<Group::MAX_MEMBERS> _start_value{};

  /// End value of the hardware performance counters.
  CounterValues<Group::MAX_MEMBERS> _end_value{};

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
    const CounterValues<Group::MAX_MEMBERS>& counter_values,
    std::uint64_t id) noexcept;
};
}