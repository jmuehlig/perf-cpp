#include <cerrno>
#include <iostream>
#include <perfcpp/group.h>
#include <stdexcept>
#include <sys/ioctl.h>
#include <type_traits>
#include <unistd.h>

bool
perf::Group::open(const perf::Config config)
{
  /// File descriptor of the group leader.
  auto group_leader_file_descriptor = -1LL;

  for (auto counter_id = 0U; counter_id < this->_members.size(); ++counter_id) {
    auto& counter = this->_members[counter_id];
    const auto is_group_leader = counter_id == 0U;

    /// Open the counter for statistical monitoring (only start and end values, not sampling).
    /// If opening fails, the open() call will throw an exception.
    counter.open(config.is_debug(),
                 is_group_leader,
                 /* is_secret_leader */ false,
                 group_leader_file_descriptor,
                 config.cpu_id(),
                 config.process_id(),
                 config.is_include_child_threads(),
                 config.is_include_kernel(),
                 config.is_include_user(),
                 config.is_include_hypervisor(),
                 config.is_include_idle(),
                 config.is_include_guest(),
                 /* is_read_format */ true,
                 /* sample_type */ std::nullopt,
                 /* branch_type */ std::nullopt,
                 /* user_registers */ std::nullopt,
                 /* kernel_registers */ std::nullopt,
                 /* max_callstack */ std::nullopt,
                 /* is_include_context_switch */ false,
                 /* is_include_cgroup */ false);

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
    throw std::runtime_error{ "Cannot start an empty group." };
  }

  /// Enable the counters.
  this->enable();

  /// Read the counter values at start time.
  return this->read(this->_start_value);
}

void
perf::Group::enable() const
{
  const auto leader_file_descriptor = static_cast<std::int32_t>(this->leader_file_descriptor());

  /// Reset and enable counter group.
  ::ioctl(leader_file_descriptor, PERF_EVENT_IOC_RESET, 0);
  ::ioctl(leader_file_descriptor, PERF_EVENT_IOC_ENABLE, 0);
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
  const auto leader_file_descriptor = static_cast<std::int32_t>(this->leader_file_descriptor());

  ::ioctl(leader_file_descriptor, PERF_EVENT_IOC_DISABLE, 0);
}

bool
perf::Group::read(CounterReadFormat<MAX_MEMBERS>& value) const
{
  const auto leader_file_descriptor = static_cast<std::int32_t>(this->leader_file_descriptor());

  const auto read_size = ::read(leader_file_descriptor, &value, sizeof(std::remove_reference<decltype(value)>::type));

  return read_size > 0ULL;
}

bool
perf::Group::add(perf::CounterConfig counter)
{
  this->_members.emplace_back(counter);
  return true;
}

double
perf::Group::get(const std::size_t index) const
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
perf::Group::value_for_id(const CounterReadFormat<Group::MAX_MEMBERS>& counter_values, const std::uint64_t id) noexcept
{
  /// Check the Id the counters to find the matching one.
  for (auto i = 0U; i < counter_values.count_members; ++i) {
    if (counter_values.values[i].id == id) {
      return counter_values.values[i].value;
    }
  }

  return std::nullopt;
}
