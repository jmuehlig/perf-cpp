#include <asm/unistd.h>
#include <cstring>
#include <iostream>
#include <perfcpp/group.h>
#include <stdexcept>
#include <sys/ioctl.h>
#include <unistd.h>

using namespace perf;

bool
perf::Group::open(const perf::Config config)
{
  /// File descriptor of the group leader.
  auto leader_file_descriptor = std::int64_t{ -1 };

  auto is_all_open = true;

  for (auto& counter : this->_members) {
    /// The first counter will become the leader.
    const auto is_leader = leader_file_descriptor == -1;

    /// Initialize the event.
    auto& perf_event = counter.event_attribute();
    std::memset(&perf_event, 0, sizeof(perf_event_attr));
    perf_event.type = counter.type();
    perf_event.size = sizeof(perf_event_attr);
    perf_event.config = counter.event_id();
    perf_event.config1 = counter.event_id_extension()[0U];
    perf_event.config2 = counter.event_id_extension()[1U];
    perf_event.disabled = is_leader;
    perf_event.inherit = static_cast<std::int32_t>(config.is_include_child_threads());
    perf_event.exclude_kernel = static_cast<std::int32_t>(!config.is_include_kernel());
    perf_event.exclude_user = static_cast<std::int32_t>(!config.is_include_user());
    perf_event.exclude_hv = static_cast<std::int32_t>(!config.is_include_hypervisor());
    perf_event.exclude_idle = static_cast<std::int32_t>(!config.is_include_idle());
    perf_event.exclude_guest = static_cast<std::int32_t>(!config.is_include_guest());

    perf_event.read_format = PERF_FORMAT_GROUP | PERF_FORMAT_ID;

    /// Record the time activated and running for leaders.
    if (is_leader) {
      perf_event.read_format |= PERF_FORMAT_TOTAL_TIME_ENABLED | PERF_FORMAT_TOTAL_TIME_RUNNING;
    }

    /// Open the counter.
    const std::int32_t cpu_id = config.cpu_id().has_value() ? std::int32_t{ config.cpu_id().value() } : -1;
    const std::int64_t file_descriptor =
      syscall(__NR_perf_event_open, &perf_event, config.process_id(), cpu_id, leader_file_descriptor, 0);
    counter.file_descriptor(file_descriptor);

    /// Print debug output, if requested.
    if (config.is_debug()) {
      std::cout << counter.to_string() << std::flush;
    }

    if (counter.is_open()) {
      ::ioctl(static_cast<std::int32_t>(file_descriptor), PERF_EVENT_IOC_ID, &counter.id());
    } else {
      throw std::runtime_error{ "Cannot create file descriptor for counter." };
    }

    /// Set the leader file descriptor.
    if (is_leader) {
      leader_file_descriptor = file_descriptor;
    }

    is_all_open &= counter.is_open();
  }

  return is_all_open;
}

void
perf::Group::close()
{
  for (auto& counter : this->_members) {
    if (counter.is_open()) {
      ::close(static_cast<std::int32_t>(counter.file_descriptor()));
      counter.file_descriptor(-1);
    }
  }
}

bool
perf::Group::start()
{
  if (this->_members.empty()) {
    throw std::runtime_error{ "Cannot start an empty group." };
  }

  const auto leader_file_descriptor = this->leader_file_descriptor();

  ::ioctl(leader_file_descriptor, PERF_EVENT_IOC_RESET, 0);
  ::ioctl(leader_file_descriptor, PERF_EVENT_IOC_ENABLE, 0);

  const auto read_size = ::read(leader_file_descriptor, &this->_start_value, sizeof(read_format));
  return read_size > 0U;
}

bool
perf::Group::stop()
{
  if (this->_members.empty()) {
    return false;
  }

  const auto leader_file_descriptor = this->leader_file_descriptor();

  const auto read_size = ::read(leader_file_descriptor, &this->_end_value, sizeof(read_format));
  ioctl(leader_file_descriptor, PERF_EVENT_IOC_DISABLE, 0);
  return read_size > 0U;
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
  const auto multiplexing_correction = double(this->_end_value.time_enabled - this->_start_value.time_enabled) /
                                       double(this->_end_value.time_running - this->_start_value.time_running);

  const auto& counter = this->_members[index];
  const auto start_value = Group::value_for_id(this->_start_value, counter.id());
  const auto end_value = Group::value_for_id(this->_end_value, counter.id());

  if (start_value.has_value() && end_value.has_value()) {
    const auto result = double(end_value.value() - start_value.value());
    return result > .0 ? result * multiplexing_correction : .0;
  }

  return 0;
}
