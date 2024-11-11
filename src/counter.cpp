#include <algorithm>
#include <cerrno>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <perfcpp/counter.h>
#include <perfcpp/feature.h>
#include <sstream>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <utility>

std::optional<double>
perf::CounterResult::get(std::string_view name) const noexcept
{
  if (auto iterator = std::find_if(
        this->_results.begin(), this->_results.end(), [&name](const auto res) { return name == res.first; });
      iterator != this->_results.end()) {
    return iterator->second;
  }

  return std::nullopt;
}

std::string
perf::CounterResult::to_json() const
{
  auto json_stream = std::stringstream{};

  json_stream << "{";

  for (auto i = 0U; i < this->_results.size(); ++i) {
    if (i > 0U) {
      json_stream << ",";
    }

    json_stream << "\"" << this->_results[i].first << "\": " << this->_results[i].second;
  }

  json_stream << "}";

  return json_stream.str();
}

std::string
perf::CounterResult::to_csv(const char delimiter, const bool print_header) const
{
  auto csv_stream = std::stringstream{};

  if (print_header) {
    csv_stream << "counter" << delimiter << "value\n";
  }

  for (auto i = 0U; i < this->_results.size(); ++i) {
    if (i > 0U) {
      csv_stream << "\n";
    }

    csv_stream << this->_results[i].first << delimiter << this->_results[i].second;
  }

  return csv_stream.str();
}

std::string
perf::CounterResult::to_string() const
{
  auto result = std::vector<std::pair<std::string_view, std::string>>{};
  result.reserve(this->_results.size());

  /// Default column lengths, equal to the header.
  auto max_name_length = 12UL, max_value_length = 5UL;

  /// Collect counter names and values as strings.
  for (const auto& [name, value] : this->_results) {
    auto value_string = std::to_string(value);

    max_name_length = std::max(max_name_length, name.size());
    max_value_length = std::max(max_value_length, value_string.size());

    result.emplace_back(name, std::move(value_string));
  }

  auto table_stream = std::stringstream{};
  table_stream
    /// Print the header.
    << "| Value" << std::setw(std::int32_t(max_value_length) - 4) << " " << "| Counter"
    << std::setw(std::int32_t(max_name_length) - 6) << " "
    << "|\n"

    /// Print the separator line.
    << "|" << std::string(max_value_length + 2U, '-') << "|" << std::string(max_name_length + 2U, '-') << "|";

  /// Print the results as columns.
  for (const auto& [name, value] : result) {
    table_stream << "\n| " << std::setw(std::int32_t(max_value_length)) << value << " | " << name
                 << std::setw(std::int32_t(max_name_length - name.size()) + 1) << " " << "|";
  }

  table_stream << std::flush;

  return table_stream.str();
}

void
perf::Counter::open(const bool is_print_debug,
                    const bool is_group_leader,
                    const bool is_secret_leader,
                    const std::int64_t group_leader_file_descriptor,
                    const std::optional<std::uint16_t> cpu_id,
                    const pid_t process_id,
                    const bool is_inherit,
                    const bool is_include_kernel,
                    const bool is_include_user,
                    const bool is_include_hypervisor,
                    const bool is_include_idle,
                    const bool is_include_guest,
                    const bool is_read_format,
                    const std::optional<std::uint64_t> sample_type,
                    const std::optional<std::uint64_t> branch_type,
                    const std::optional<std::uint64_t> user_registers,
                    const std::optional<std::uint64_t> kernel_registers,
                    const std::optional<std::uint16_t> max_callstack,
                    const bool is_include_context_switch,
                    const bool is_include_cgroup)
{
  std::memset(&this->_event_attribute, 0, sizeof(perf_event_attr));
  this->_event_attribute.type = this->_config.type();
  this->_event_attribute.size = sizeof(perf_event_attr);
  this->_event_attribute.config = this->_config.event_id();
  this->_event_attribute.config1 = this->_config.event_id_extension()[0U];
  this->_event_attribute.config2 = this->_config.event_id_extension()[1U];
  this->_event_attribute.disabled = is_group_leader;

  this->_event_attribute.inherit = is_inherit;
  this->_event_attribute.exclude_kernel = !is_include_kernel;
  this->_event_attribute.exclude_user = !is_include_user;
  this->_event_attribute.exclude_hv = !is_include_hypervisor;
  this->_event_attribute.exclude_idle = !is_include_idle;
  this->_event_attribute.exclude_guest = !is_include_guest;

  if (sample_type.has_value()) {
    if (is_group_leader || is_secret_leader) {
      this->_event_attribute.sample_type = sample_type.value();
      this->_event_attribute.sample_id_all = 1U;

      /// Set period of frequency.
      this->_event_attribute.freq = static_cast<std::uint64_t>(this->_config.is_frequency());
      this->_event_attribute.sample_freq = this->_config.period_or_frequency();

      if (branch_type.has_value()) {
        this->_event_attribute.branch_sample_type = branch_type.value();
      }

#ifndef PERFCPP_NO_SAMPLE_MAX_STACK
      if (max_callstack.has_value()) {
        this->_event_attribute.sample_max_stack = max_callstack.value();
      }
#endif

      if (user_registers.has_value()) {
        this->_event_attribute.sample_regs_user = user_registers.value();
      }

      if (kernel_registers.has_value()) {
        this->_event_attribute.sample_regs_intr = kernel_registers.value();
      }

#ifndef PERFCPP_NO_RECORD_SWITCH
      this->_event_attribute.context_switch = is_include_context_switch;
#endif

#ifndef PERFCPP_NO_RECORD_CGROUP
      this->_event_attribute.cgroup = is_include_cgroup;
#endif
    }
  }

  if (is_read_format) {
    this->_event_attribute.read_format = PERF_FORMAT_GROUP | PERF_FORMAT_ID;

    if (is_group_leader) {
      this->_event_attribute.read_format |= PERF_FORMAT_TOTAL_TIME_ENABLED | PERF_FORMAT_TOTAL_TIME_RUNNING;
    }
  }

  const std::int32_t real_cpu_id = cpu_id.has_value() ? std::int32_t{ cpu_id.value() } : -1;

  if (sample_type.has_value()) {
    /// For sampling, we try to adjust the precision (precise_ip) if we cannot successfully open the counter with the
    /// provided value.
    for (auto precise_ip = std::int32_t{ this->_config.precise_ip() }; precise_ip > -1; --precise_ip) {

      /// Adjust the precision, which may be lower than initially requested.
      this->_event_attribute.precise_ip = std::uint64_t(precise_ip);

      /// Try to open using the perf subsystem.
      this->_file_descriptor =
        this->perf_event_open(process_id, real_cpu_id, is_group_leader, group_leader_file_descriptor);

      /// If opening the file descriptor not successful or the error indicates that adjusting (decreasing) the precision
      /// does not help, we are done.
      if (this->_file_descriptor > -1LL || (errno != EINVAL && errno != EOPNOTSUPP)) {
        break;
      }
    }
  } else {
    /// For monitoring (not sampling), we do not need to adjust the precision; a single try is enough.
    this->_file_descriptor =
      this->perf_event_open(process_id, real_cpu_id, is_group_leader, group_leader_file_descriptor);
  }

  /// Read the counter's id.
  if (this->_file_descriptor > -1LL) {
    ::ioctl(static_cast<std::int32_t>(_file_descriptor), PERF_EVENT_IOC_ID, &_id);
  }

  /// Print debug output, if requested.
  if (is_print_debug) {
    std::cout << this->to_string(is_group_leader, group_leader_file_descriptor, process_id, real_cpu_id) << std::flush;
  }

  if (this->_file_descriptor < 0LL) {
    throw std::runtime_error{
      std::string{ "Cannot create file descriptor for counter (error no: " }.append(std::to_string(errno)).append(").")
    };
  }
}

void
perf::Counter::close()
{
  if (const auto file_descriptor = std::exchange(_file_descriptor, -1LL); file_descriptor > -1LL) {
    ::close(static_cast<std::int32_t>(file_descriptor));
  }
}

std::int64_t
perf::Counter::perf_event_open(pid_t process_id,
                               std::int32_t cpu_id,
                               bool is_group_leader,
                               std::int64_t group_leader_file_descriptor)
{
  return ::syscall(__NR_perf_event_open,
                   &this->_event_attribute,
                   process_id,
                   cpu_id,
                   is_group_leader ? -1LL : group_leader_file_descriptor,
                   0);
}

std::string
perf::Counter::to_string(const std::optional<bool> is_group_leader,
                         const std::optional<std::int64_t> group_leader_file_descriptor,
                         const std::optional<pid_t> process_id,
                         const std::optional<std::int32_t> cpu_id) const
{
  auto stream = std::stringstream{};

  stream << "Counter:\n"
         << "    id: " << this->_id << "\n"
         << "    file_descriptor: " << this->_file_descriptor << "\n";

  /// Role (leader or member).
  if (is_group_leader.has_value()) {
    if (is_group_leader.value()) {
      stream << "    role: group leader\n";
    } else {
      stream << "    role: group member\n";
      if (group_leader_file_descriptor.has_value()) {
        stream << "    leader's file_descriptor: " << group_leader_file_descriptor.value() << "\n";
      }
      stream << "\n";
    }
  }

  /// Process
  if (process_id.has_value()) {
    stream << "    process: ";
    if (process_id.value() == 0) {
      stream << "0 (calling)\n";
    } else if (process_id.value() > 0) {
      stream << process_id.value() << " (specific process)\n";
    } else {
      stream << process_id.value() << " (all)\n";
    }
  }

  /// CPU
  if (cpu_id.has_value()) {
    stream << "    cpu: ";
    if (cpu_id.value() >= 0) {
      stream << cpu_id.value() << "\n";
    } else {
      stream << cpu_id.value() << "(any)\n";
    }
  }

  /// Perf Event
  stream << "    perf_event_attr:\n"
         << "        type: " << this->_event_attribute.type << "\n"
         << "        size: " << this->_event_attribute.size << "\n"
         << "        config: 0x" << std::hex << this->_event_attribute.config << std::dec << "\n";

  if (this->_event_attribute.sample_type > 0U) {
    stream << "        sample_type: ";

    auto is_first =
      Counter::print_type_to_stream(stream, this->_event_attribute.sample_type, PERF_SAMPLE_IP, "IP", true);
    is_first =
      Counter::print_type_to_stream(stream, this->_event_attribute.sample_type, PERF_SAMPLE_TID, "TID", is_first);
    is_first =
      Counter::print_type_to_stream(stream, this->_event_attribute.sample_type, PERF_SAMPLE_TIME, "TIME", is_first);
    is_first =
      Counter::print_type_to_stream(stream, this->_event_attribute.sample_type, PERF_SAMPLE_ADDR, "ADDR", is_first);
    is_first =
      Counter::print_type_to_stream(stream, this->_event_attribute.sample_type, PERF_SAMPLE_READ, "READ", is_first);
    is_first = Counter::print_type_to_stream(
      stream, this->_event_attribute.sample_type, PERF_SAMPLE_CALLCHAIN, "CALLCHAIN", is_first);
    is_first =
      Counter::print_type_to_stream(stream, this->_event_attribute.sample_type, PERF_SAMPLE_CPU, "CPU", is_first);
    is_first =
      Counter::print_type_to_stream(stream, this->_event_attribute.sample_type, PERF_SAMPLE_PERIOD, "PERIOD", is_first);
    is_first = Counter::print_type_to_stream(
      stream, this->_event_attribute.sample_type, PERF_SAMPLE_STREAM_ID, "STREAM_ID", is_first);
    is_first =
      Counter::print_type_to_stream(stream, this->_event_attribute.sample_type, PERF_SAMPLE_RAW, "RAW", is_first);
    is_first = Counter::print_type_to_stream(
      stream, this->_event_attribute.sample_type, PERF_SAMPLE_BRANCH_STACK, "BRANCH_STACK", is_first);
    is_first = Counter::print_type_to_stream(
      stream, this->_event_attribute.sample_type, PERF_SAMPLE_REGS_USER, "REGS_USER", is_first);
    is_first = Counter::print_type_to_stream(
      stream, this->_event_attribute.sample_type, PERF_SAMPLE_STACK_USER, "REGS_USER", is_first);
    is_first =
      Counter::print_type_to_stream(stream, this->_event_attribute.sample_type, PERF_SAMPLE_WEIGHT, "WEIGHT", is_first);
    is_first = Counter::print_type_to_stream(
      stream, this->_event_attribute.sample_type, PERF_SAMPLE_DATA_SRC, "DATA_SRC", is_first);
    is_first = Counter::print_type_to_stream(
      stream, this->_event_attribute.sample_type, PERF_SAMPLE_IDENTIFIER, "IDENTIFIER", is_first);
    is_first = Counter::print_type_to_stream(
      stream, this->_event_attribute.sample_type, PERF_SAMPLE_REGS_INTR, "REGS_INTR", is_first);
#ifndef PERFCPP_NO_SAMPLE_PHYS_ADDR
    is_first = Counter::print_type_to_stream(
      stream, this->_event_attribute.sample_type, PERF_SAMPLE_PHYS_ADDR, "PHYS_ADDR", is_first);
#endif

#ifndef PERFCPP_NO_SAMPLE_CGROUP
    is_first =
      Counter::print_type_to_stream(stream, this->_event_attribute.sample_type, PERF_SAMPLE_CGROUP, "CGROUP", is_first);
#endif

#ifndef PERFCPP_NO_SAMPLE_DATA_PAGE_SIZE
    is_first = Counter::print_type_to_stream(
      stream, this->_event_attribute.sample_type, PERF_SAMPLE_DATA_PAGE_SIZE, "DATA_PAGE_SIZE", is_first);
#endif

#ifndef PERFCPP_NO_SAMPLE_CODE_PAGE_SIZE
    is_first = Counter::print_type_to_stream(
      stream, this->_event_attribute.sample_type, PERF_SAMPLE_CODE_PAGE_SIZE, "PAGE_SIZE", is_first);
#endif

#ifndef PERFCPP_NO_SAMPLE_WEIGHT_STRUCT
    Counter::print_type_to_stream(
      stream, this->_event_attribute.sample_type, PERF_SAMPLE_WEIGHT_STRUCT, "WEIGHT_STRUCT", is_first);
#endif

    stream << "\n";
  }

  if (this->_event_attribute.freq > 0U && this->_event_attribute.sample_freq > 0U) {
    stream << "        sample_freq: " << this->_event_attribute.sample_freq << "\n";
  } else if (this->_event_attribute.sample_period > 0U) {
    stream << "        sample_period: " << this->_event_attribute.sample_period << "\n";
  }

  if (this->_event_attribute.precise_ip > 0U) {
    stream << "        precise_ip: " << this->_event_attribute.precise_ip << "\n";
  }

  if (this->_event_attribute.mmap > 0U) {
    stream << "        mmap: " << this->_event_attribute.mmap << "\n";
  }

  if (this->_event_attribute.sample_id_all > 0U) {
    stream << "        sample_id_all: " << this->_event_attribute.sample_id_all << "\n";
  }

  if (this->_event_attribute.read_format > 0U) {
    stream << "        read_format: ";

    auto is_first = Counter::print_type_to_stream(
      stream, this->_event_attribute.read_format, PERF_FORMAT_TOTAL_TIME_ENABLED, "TOTAL_TIME_ENABLED", true);
    is_first = Counter::print_type_to_stream(
      stream, this->_event_attribute.read_format, PERF_FORMAT_TOTAL_TIME_RUNNING, "TOTAL_TIME_RUNNING", is_first);
    is_first =
      Counter::print_type_to_stream(stream, this->_event_attribute.read_format, PERF_FORMAT_ID, "ID", is_first);
    is_first =
      Counter::print_type_to_stream(stream, this->_event_attribute.read_format, PERF_FORMAT_GROUP, "GROUP", is_first);
#ifndef PERFCPP_NO_FORMAT_LOST
    Counter::print_type_to_stream(stream, this->_event_attribute.read_format, PERF_FORMAT_LOST, "LOST", is_first);
#endif
    stream << "\n";
  }

  if (this->_event_attribute.branch_sample_type > 0U) {
    stream << "        branch_sample_type: ";

    auto is_first = Counter::print_type_to_stream(
      stream, this->_event_attribute.branch_sample_type, PERF_SAMPLE_BRANCH_USER, "BRANCH_USER", true);
    is_first = Counter::print_type_to_stream(
      stream, this->_event_attribute.branch_sample_type, PERF_SAMPLE_BRANCH_KERNEL, "BRANCH_KERNEL", is_first);
    is_first = Counter::print_type_to_stream(
      stream, this->_event_attribute.branch_sample_type, PERF_SAMPLE_BRANCH_HV, "BRANCH_HV", is_first);
    is_first = Counter::print_type_to_stream(
      stream, this->_event_attribute.branch_sample_type, PERF_SAMPLE_BRANCH_ANY, "BRANCH_ANY", is_first);
    is_first = Counter::print_type_to_stream(
      stream, this->_event_attribute.branch_sample_type, PERF_SAMPLE_BRANCH_ANY_CALL, "BRANCH_ANY_CALL", is_first);
#ifndef PERFCPP_NO_SAMPLE_BRANCH_CALL
    is_first = Counter::print_type_to_stream(
      stream, this->_event_attribute.branch_sample_type, PERF_SAMPLE_BRANCH_CALL, "BRANCH_CALL", is_first);
#endif
    is_first = Counter::print_type_to_stream(
      stream, this->_event_attribute.branch_sample_type, PERF_SAMPLE_BRANCH_IND_CALL, "BRANCH_IND_CALL", is_first);
    is_first = Counter::print_type_to_stream(
      stream, this->_event_attribute.branch_sample_type, PERF_SAMPLE_BRANCH_ANY_RETURN, "BRANCH_ANY_RETURN", is_first);
#ifndef PERFCPP_NO_SAMPLE_BRANCH_IND_JUMP
    is_first = Counter::print_type_to_stream(
      stream, this->_event_attribute.branch_sample_type, PERF_SAMPLE_BRANCH_IND_JUMP, "BRANCH_IND_JUMP", is_first);
#endif
    is_first = Counter::print_type_to_stream(
      stream, this->_event_attribute.branch_sample_type, PERF_SAMPLE_BRANCH_ABORT_TX, "BRANCH_ABORT_TX", is_first);
    is_first = Counter::print_type_to_stream(
      stream, this->_event_attribute.branch_sample_type, PERF_SAMPLE_BRANCH_IN_TX, "BRANCH_IN_TX", is_first);
    Counter::print_type_to_stream(
      stream, this->_event_attribute.branch_sample_type, PERF_SAMPLE_BRANCH_NO_TX, "BRANCH_NO_TX", is_first);

    stream << "\n";
  }
#ifndef PERFCPP_NO_SAMPLE_MAX_STACK
  if (this->_event_attribute.sample_max_stack > 0U) {
    stream << "        sample_max_stack: " << this->_event_attribute.sample_max_stack << "\n";
  }
#endif

  if (this->_event_attribute.sample_regs_user > 0U) {
    stream << "        sample_regs_user: " << this->_event_attribute.sample_regs_user << "\n";
  }

  if (this->_event_attribute.sample_regs_intr > 0U) {
    stream << "        sample_regs_intr: " << this->_event_attribute.sample_regs_intr << "\n";
  }

  if (this->_event_attribute.config1 > 0U) {
    stream << "        config1: 0x" << std::hex << this->_event_attribute.config1 << std::dec << "\n";
  }
  if (this->_event_attribute.config2 > 0U) {
    stream << "        config2: 0x" << std::hex << this->_event_attribute.config2 << std::dec << "\n";
  }
  if (this->_event_attribute.disabled > 0U) {
    stream << "        disabled: " << this->_event_attribute.disabled << "\n";
  }
  if (this->_event_attribute.inherit > 0U) {
    stream << "        inherit: " << this->_event_attribute.inherit << "\n";
  }
  if (this->_event_attribute.exclude_kernel > 0U) {
    stream << "        exclude_kernel: " << this->_event_attribute.exclude_kernel << "\n";
  }
  if (this->_event_attribute.exclude_user > 0U) {
    stream << "        exclude_user: " << this->_event_attribute.exclude_user << "\n";
  }
  if (this->_event_attribute.exclude_hv > 0U) {
    stream << "        exclude_hv: " << this->_event_attribute.exclude_hv << "\n";
  }
  if (this->_event_attribute.exclude_idle > 0U) {
    stream << "        exclude_idle: " << this->_event_attribute.exclude_idle << "\n";
  }
  if (this->_event_attribute.exclude_guest > 0U) {
    stream << "        exclude_guest: " << this->_event_attribute.exclude_guest << "\n";
  }
#ifndef PERFCPP_NO_RECORD_SWITCH
  if (this->_event_attribute.context_switch > 0U) {
    stream << "        context_switch: " << this->_event_attribute.context_switch << "\n";
  }
#endif
#ifndef PERFCPP_NO_RECORD_CGROUP
  if (this->_event_attribute.cgroup > 0U) {
    stream << "        cgroup: " << this->_event_attribute.cgroup << "\n";
  }
#endif

  return stream.str();
}

bool
perf::Counter::print_type_to_stream(std::stringstream& stream,
                                    const std::uint64_t mask,
                                    const std::uint64_t type,
                                    std::string&& name,
                                    const bool is_first)
{
  if (mask & type) {
    if (!is_first) {
      stream << " | ";
    }

    stream << name;
    return false;
  }

  return is_first;
}