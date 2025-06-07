#include <iostream>
#include <perfcpp/exception.h>
#include <perfcpp/group.h>
#include <stdexcept>
#include <type_traits>
#include <unistd.h>

perf::Group
perf::Group::copy_from_template(const perf::Group& other)
{
  auto copy = perf::Group{};
  copy._members.reserve(other._members.size());
  for (const auto& counter : other._members) {
    copy._members.push_back(Counter::copy_from_template(counter));
  }

  return copy;
}

void
perf::Group::open(const perf::Config& config)
{
  if (this->_members.empty()) {
    return;
  }

  /// Open the group leader.
  this->_members.front().open(config, /* is live counter */ false);

  /// Open the other events for that counter.
  for (auto counter_id = 1U; counter_id < this->_members.size(); ++counter_id) {
    this->_members[counter_id].open(config, this->_members.front().file_descriptor());
  }
}

void
perf::Group::open(const perf::Config& config,
                  const bool has_auxiliary_event,
                  const std::uint64_t buffer_pages,
                  const std::uint64_t sample_type,
                  const std::optional<std::uint64_t> branch_type,
                  const std::optional<std::uint64_t> user_registers,
                  const std::optional<std::uint64_t> kernel_registers,
                  const std::optional<std::uint32_t> max_user_stack_size,
                  const std::optional<std::uint16_t> max_callstack_size,
                  const bool is_include_context_switch)
{
  if (this->_members.empty()) {
    return;
  }

  /// Open the group leader. If the group contains an auxiliary event, the group leader will not contain a buffer (i.e.,
  /// no buffer pages).
  const auto group_leader_buffer_pages = !has_auxiliary_event ? buffer_pages : 0ULL;
  this->_members.front().open(config,
                              group_leader_buffer_pages,
                              sample_type,
                              branch_type,
                              user_registers,
                              kernel_registers,
                              max_user_stack_size,
                              max_callstack_size,
                              is_include_context_switch);

  /// The group leader's file descriptor will be passed to further counters.
  const auto& group_leader_file_descriptor = this->_members.front().file_descriptor();

  /// Open all the other counters after the first (group leader).
  for (auto counter_id = 1U; counter_id < this->_members.size(); ++counter_id) {
    /// Some counters on some Intel architectures (from Sapphire Rapids) need an auxiliary event in front (e.g.,
    /// mem-loads, mem-stores). However, this auxiliary event is of a special function; e.g., although it is the group
    /// leader, it does not need a buffer – the buffer is then allocated for the first "real" counter after the
    /// auxiliary counter.
    const auto is_counter_after_auxiliary = has_auxiliary_event && counter_id == 1U;
    const auto counter_buffer_pages = is_counter_after_auxiliary ? buffer_pages : 0ULL;

    /// Open the counter as a secondary counter after the group leader. Only the counter after an auxiliary counter will
    /// have a buffer.
    this->_members[counter_id].open(config,
                                    counter_buffer_pages,
                                    sample_type,
                                    branch_type,
                                    user_registers,
                                    kernel_registers,
                                    max_user_stack_size,
                                    max_callstack_size,
                                    is_include_context_switch,
                                    group_leader_file_descriptor);
  }
}

void
perf::Group::close()
{
  for (auto& counter : this->_members) {
    counter.close();
  }
}

void
perf::Group::start()
{
  if (this->_members.empty()) {
    throw CannotStartEmptyGroupError{};
  }

  /// Enable the counters.
  this->enable();

  /// Read the counter values at start time.
  this->read(this->_start_value);
}

void
perf::Group::enable() const
{
  /// Enable the group leader.
  if (!this->_members.empty()) {
    this->_members.front().enable();
  }
}

void
perf::Group::stop()
{
  if (this->_members.empty()) {
    return;
  }

  /// Read the counter values at stop time.
  this->read(this->_end_value);

  /// Disable counter group.
  this->disable();

  /// Calculate multiplexing correction.
  const auto time_enabled = double(this->_end_value.time_enabled() - this->_start_value.time_enabled());
  const auto time_running = double(this->_end_value.time_running() - this->_start_value.time_running());
  this->_multiplexing_correction = time_running > .0 ? time_enabled / time_running : 1.;
}

void
perf::Group::disable() const
{
  /// Disable the group leader.
  if (!this->empty()) {
    this->_members.front().disable();
  }
}

void
perf::Group::read(CounterValues<MAX_MEMBERS>& values)
{
  if (!this->empty()) {
    const auto read_size = ::read(
      this->_members.front().file_descriptor().value(), &values, sizeof(std::remove_reference<decltype(values)>::type));

    if (read_size < 0LL) {
      throw CannotReadCounter{};
    }
  }
}

void
perf::Group::add(const perf::CounterConfig counter)
{
  this->_members.emplace_back(counter);
}

double
perf::Group::get(const std::size_t index) const noexcept
{
  if (index < this->_members.size()) {
    const auto& counter = this->_members[index];

    /// Read start and end values for the requested counter.
    if (const auto start_value = this->_start_value.value(counter.id()); start_value.has_value()) {
      /// Correct and return the result, if the counter was found.
      if (const auto end_value = this->_end_value.value(counter.id()); end_value.has_value()) {
        const auto result = double(end_value.value() - start_value.value());

        /// Fall back to zero, of the counter value is 0 (or lower).
        return std::max(.0, result) * this->_multiplexing_correction;
      }
    }
  }

  /// Return if counter or values were not found.
  return .0;
}