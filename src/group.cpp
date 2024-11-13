#include <iostream>
#include <perfcpp/exception.h>
#include <perfcpp/group.h>
#include <stdexcept>
#include <type_traits>
#include <unistd.h>

bool
perf::Group::open(const perf::Config& config,
                  const bool is_read_format,
                  const bool has_auxiliary_event,
                  const std::optional<std::uint64_t> buffer_pages,
                  const std::optional<std::uint64_t> sample_type,
                  const std::optional<std::uint64_t> branch_type,
                  const std::optional<std::uint64_t> user_registers,
                  const std::optional<std::uint64_t> kernel_registers,
                  const std::optional<std::uint32_t> max_user_stack_size,
                  const std::optional<std::uint16_t> max_callstack_size,
                  const bool is_include_context_switch,
                  const bool is_include_cgroup)
{
  /// File descriptor of the group leader (the first counter in the group).
  auto group_leader_file_descriptor = -1LL;

  for (auto counter_id = 0U; counter_id < this->_members.size(); ++counter_id) {
    auto& counter = this->_members[counter_id];

    /// The first event is the group leader.
    const auto is_group_leader = counter_id == 0U;

    /// The user-level buffer is allocated (via mmap) for the group leader, if the group has no auxiliary event
    /// (before Intel Sapphire Rapids and all AMD) – or if the group has an auxiliary event but this is the first
    /// "real" (non-auxiliary) event.
    const auto is_counter_needs_buffer =
      (is_group_leader && !has_auxiliary_event) || (has_auxiliary_event && counter_id == 1U);

    /// Open the counter for statistical monitoring (only start and end values, not sampling).
    /// If opening fails, the open() call will throw an exception.
    counter.open(config,
                 is_group_leader,
                 has_auxiliary_event && counter_id == 1U,
                 group_leader_file_descriptor,
                 is_read_format,
                 is_counter_needs_buffer ? buffer_pages : std::nullopt,
                 sample_type,
                 branch_type,
                 user_registers,
                 kernel_registers,
                 max_user_stack_size,
                 max_callstack_size,
                 is_include_context_switch,
                 is_include_cgroup);

    /// Set the group leader file descriptor.
    if (is_group_leader) {
      group_leader_file_descriptor = counter.file_descriptor();
    }
  }

  /// If we cannot open any counter, we will throw an exception.
  return true;
}

void
perf::Group::close()
{
  for (auto& counter : this->_members) {
    counter.close();
  }
}

bool
perf::Group::start()
{
  if (this->_members.empty()) {
    throw CannotStartEmptyGroupError{};
  }

  /// Enable the counters.
  this->enable();

  /// Read the counter values at start time.
  return this->read(this->_start_value);
}

void
perf::Group::enable() const
{
  /// Enable the group leader.
  if (!this->_members.empty()) {
    this->_members.front().enable();
  }
}

bool
perf::Group::stop()
{
  if (this->_members.empty()) {
    return false;
  }

  /// Read the counter values at stop time.
  const auto is_read_successful = this->read(this->_end_value);

  /// Disable counter group.
  this->disable();

  /// Calculate multiplexing correction.
  const auto time_enabled = double(this->_end_value.time_enabled - this->_start_value.time_enabled);
  const auto time_running = double(this->_end_value.time_running - this->_start_value.time_running);
  this->_multiplexing_correction = time_running > .0 ? time_enabled / time_running : 1.;

  return is_read_successful;
}

void
perf::Group::disable() const
{
  /// Disable the group leader.
  if (!this->empty()) {
    this->_members.front().disable();
  }
}

bool
perf::Group::read(CounterValues<MAX_MEMBERS>& values) const noexcept
{
  if (!this->empty()) {
    const auto leader_file_descriptor = static_cast<std::int32_t>(this->_members.front().file_descriptor());

    const auto read_size =
      ::read(leader_file_descriptor, &values, sizeof(std::remove_reference<decltype(values)>::type));
    return read_size > 0L;
  }

  return false;
}

bool
perf::Group::add(perf::CounterConfig counter)
{
  this->_members.emplace_back(counter);
  return true;
}

double
perf::Group::get(const std::size_t index) const noexcept
{
  if (index < this->_members.size()) {
    const auto& counter = this->_members[index];

    /// Read start and end values for the requested counter.
    const auto counter_start_value = Group::value_for_id(this->_start_value, counter.id());
    const auto counter_end_value = Group::value_for_id(this->_end_value, counter.id());

    /// Correct and return the result, if the counter was found.
    if (counter_start_value.has_value() && counter_end_value.has_value()) {
      const auto result = double(counter_end_value.value() - counter_start_value.value());

      /// Fall back to zero, of the counter value is 0 (or lower).
      return std::max(.0, result) * this->_multiplexing_correction;
    }
  }

  /// Return if counter or values were not found.
  return .0;
}

std::optional<std::uint64_t>
perf::Group::value_for_id(const CounterValues<Group::MAX_MEMBERS>& counter_values, const std::uint64_t id) noexcept
{
  /// Check the Id the counters to find the matching one.
  for (auto i = 0U; i < counter_values.count_members; ++i) {
    if (counter_values.values[i].id == id) {
      return counter_values.values[i].value;
    }
  }

  return std::nullopt;
}
