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
  for (const auto& event : other._members) {
    copy._members.push_back(Counter::copy_from_template(event));
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
  for (auto member_id = 1U; member_id < this->_members.size(); ++member_id) {
    this->_members[member_id].open(config, this->_members.front().file_descriptor());
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
                  const bool is_include_context_switch,
                  bool is_include_extended_mmap_information)
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
                              is_include_context_switch,
                              is_include_extended_mmap_information);

  /// The group leader's file descriptor will be passed to further counters.
  const auto& group_leader_file_descriptor = this->_members.front().file_descriptor();

  /// Open all the other events after the first (group leader).
  for (auto event_id = 1U; event_id < this->_members.size(); ++event_id) {
    /// Some events on some Intel architectures (from Sapphire Rapids) need an auxiliary event in front (e.g.,
    /// mem-loads, mem-stores). However, this auxiliary event is of a special function; e.g., although it is the group
    /// leader, it does not need a buffer – the buffer is then allocated for the first "real" event after the
    /// auxiliary event.
    const auto is_event_after_auxiliary = has_auxiliary_event && event_id == 1U;
    const auto event_buffer_pages = is_event_after_auxiliary ? buffer_pages : 0ULL;

    /// Open the event as a secondary counter after the group leader. Only the event after an auxiliary event will
    /// have a buffer.
    this->_members[event_id].open(config,
                                  event_buffer_pages,
                                  sample_type,
                                  branch_type,
                                  user_registers,
                                  kernel_registers,
                                  max_user_stack_size,
                                  max_callstack_size,
                                  is_include_context_switch,
                                  is_include_extended_mmap_information,
                                  group_leader_file_descriptor);
  }
}

void
perf::Group::close()
{
  for (auto& member : this->_members) {
    member.close();
  }
}

void
perf::Group::start()
{
  if (this->_members.empty()) {
    throw CannotStartEmptyGroupError{};
  }

  /// Enable the counter.
  this->enable();

  /// Read the event values at start time.
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

  /// Read the event values at stop time.
  this->read(this->_end_value);

  /// Disable group.
  this->disable();

  /// Calculate multiplexing correction.
  this->_multiplexing_correction = Group::calculate_multiplexing_factor(this->_start_value, this->_end_value);
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
      this->_members.front().file_descriptor().value(), &values, sizeof(std::remove_reference_t<decltype(values)>));

    if (read_size < 0LL) {
      throw CannotReadCounter{};
    }
  }
}

void
perf::Group::add(const perf::CounterConfig event_config)
{
  this->_members.emplace_back(event_config);
}

double
perf::Group::get(const std::size_t index) const
{
  if (index < this->_members.size()) {
    const auto& event = this->_members[index];

    /// Read start and end values for the requested event.
    if (const auto start_value = this->_start_value.value(event.id()); start_value.has_value()) {
      /// Correct and return the result, if the event was found.
      if (const auto end_value = this->_end_value.value(event.id()); end_value.has_value()) {
        const auto result = static_cast<double>(end_value.value() - start_value.value()) * event.scale();

        /// Fall back to zero, of the event value is 0 (or lower).
        return std::max(.0, result) * this->_multiplexing_correction;
      }
    }
  }

  /// Return if event or values were not found.
  return .0;
}