#include <algorithm>
#include <cerrno>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <perfcpp/counter.h>
#include <perfcpp/exception.h>
#include <perfcpp/feature.h>
#include <sstream>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <utility>
#include <variant>

#if defined(__x86_64__) || defined(__i386__)
#include <x86intrin.h>
#endif

std::optional<double>
perf::CounterResult::get(std::string_view name) const noexcept
{
  if (const auto result_iterator = std::find_if(
        this->_results.begin(), this->_results.end(), [&name](const auto res) { return name == res.first; });
      result_iterator != this->_results.end()) {
    return result_iterator->second;
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

  /// Format the counters as a table.
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

perf::Counter::~Counter()
{
  /// Close the counter, if not already done.
  this->close();
}

void
perf::Counter::open(const perf::Config& config,
                    const bool is_group_leader,
                    const bool is_secret_leader,
                    const std::int64_t group_leader_file_descriptor,
                    const bool is_read_format,
                    const std::optional<std::uint64_t> buffer_pages,
                    const std::optional<std::uint64_t> sample_type,
                    const std::optional<std::uint64_t> branch_type,
                    const std::optional<std::uint64_t> user_registers,
                    const std::optional<std::uint64_t> kernel_registers,
                    const std::optional<std::uint32_t> max_user_stack_size,
                    [[maybe_unused]] const std::optional<std::uint16_t> max_callstack_size,
                    [[maybe_unused]] const bool is_include_context_switch,
                    [[maybe_unused]] const bool is_include_cgroup)
{
  std::memset(&this->_event_attribute, 0, sizeof(perf_event_attr));
  this->_event_attribute.type = this->_config.type();
  this->_event_attribute.size = sizeof(perf_event_attr);
  this->_event_attribute.config = this->_config.event_id();
  this->_event_attribute.config1 = this->_config.event_id_extension()[0U];
  this->_event_attribute.config2 = this->_config.event_id_extension()[1U];
  this->_event_attribute.disabled = is_group_leader;

  this->_event_attribute.inherit = config.is_include_child_threads();
  this->_event_attribute.exclude_kernel = !config.is_include_kernel();
  this->_event_attribute.exclude_user = !config.is_include_user();
  this->_event_attribute.exclude_hv = !config.is_include_hypervisor();
  this->_event_attribute.exclude_idle = !config.is_include_idle();
  this->_event_attribute.exclude_guest = !config.is_include_guest();

  /// Set attributes needed for sampling, if sampling is requested.
  if (sample_type.has_value()) {
    if (is_group_leader || is_secret_leader) {
      /// Set the sample type for the group leader (or the counter after the auxiliary-event).
      this->_event_attribute.sample_type = sample_type.value();

      /// Sampling is not only indicated by the sample_type since "live" events (read without stopping the counter) also
      /// have a sample type but are not truly sampling. We assume that true sampling is only requested when
      /// period/frequency and precision is set since both are needed for sampling but not for reading counter without
      /// stopping.
      if (this->_config.period_or_frequency().has_value() && this->_config.precise_ip().has_value()) {
        this->_event_attribute.sample_id_all = 1U;

        /// Set period of frequency, based on the PeriodOrFrequency variant.
        std::visit(
          [&event_attribute = this->_event_attribute](const auto period_or_frequency) {
            using T = std::decay_t<decltype(period_or_frequency)>;
            if constexpr (std::is_same_v<T, class Period>) {
              event_attribute.sample_period = period_or_frequency.get();
            } else if constexpr (std::is_same_v<T, class Frequency>) {
              event_attribute.freq = true;
              event_attribute.sample_period = period_or_frequency.get();
            }
          },
          this->_config.period_or_frequency().value());

        /// Set sampled fields.
        this->_event_attribute.branch_sample_type = branch_type.value_or(0ULL);
#ifndef PERFCPP_NO_SAMPLE_MAX_STACK /// Max sample stack is only supported since Linux 4.8
        this->_event_attribute.sample_max_stack = max_callstack_size.value_or(0U);
#endif
        this->_event_attribute.sample_regs_user = user_registers.value_or(0ULL);
        this->_event_attribute.sample_regs_intr = kernel_registers.value_or(0ULL);
        this->_event_attribute.sample_stack_user = max_user_stack_size.value_or(0U);
#ifndef PERFCPP_NO_RECORD_SWITCH /// Record switch is supported since Linux 4.3.
        this->_event_attribute.context_switch = is_include_context_switch;
#endif
#ifndef PERFCPP_NO_RECORD_CGROUP /// Recording cgroup is supported since Linux 5.7.
        this->_event_attribute.cgroup = is_include_cgroup;
#endif
      }
    }
  }

  /// Set format of counter values (which is only needed when counters are read).
  /// The group leader additionally records the running time.
  if (is_read_format) {
    this->_event_attribute.read_format = PERF_FORMAT_GROUP | PERF_FORMAT_ID;

    if (is_group_leader) {
      this->_event_attribute.read_format |= PERF_FORMAT_TOTAL_TIME_ENABLED | PERF_FORMAT_TOTAL_TIME_RUNNING;
    }
  }

  /// Transform the CPU id to the format expected by the perf subsystem – which is -1 for any CPU (but perf-cpp uses an
  /// optional unsigned integer for that case).
  const std::int32_t cpu_id = config.cpu_id().has_value() ? std::int32_t{ config.cpu_id().value() } : -1;

  /// Use the specified process id or 0 for indicating that the counter should monitor the calling thread/process.
  const auto process_id = config.process_id().value_or(0);

  /// Try to open the counter. For sampling, we might try to adjust the precise_ip configuration (see
  /// Counter::is_adjust_precise_ip).
  auto precise_ip = this->_config.precise_ip().value_or(0U);
  do {
    /// precise_ip is only needed for sampling, not counting events and live events; thus, only set when it has a value.
    if (this->_config.precise_ip().has_value()) {
      this->_event_attribute.precise_ip =
        precise_ip & 0b11; /// Use only two bits as perf_event_attr.precise_ip has only two bits.
    }

    /// Try to open using the perf subsystem. This might fail. If precise_ip is the reason (derived by the error code),
    /// we try to adjust the precision and try again (see Counter::is_adjust_precise_ip).
    this->_file_descriptor = this->perf_event_open(process_id, cpu_id, is_group_leader, group_leader_file_descriptor);

    /// Repeat until success (file_descriptor has a "valid" value or trying again is hopeless.
  } while (this->_file_descriptor < 0LL && Counter::is_adjust_precise_ip(precise_ip--, sample_type, errno));

  /// In case perf_event_open reported an error, notice it here, but process it later.
  const auto error_code = errno;

  /// Read and set the counter's id.
  /// This is done before (possibly) printing the counter to include the counter id into printing.
  if (this->_file_descriptor > -1LL) {
    ::ioctl(static_cast<std::int32_t>(_file_descriptor), PERF_EVENT_IOC_ID, &_id);
  }

  /// Print debug output, if requested.
  if (config.is_debug()) {
    std::cout << this->to_string(is_group_leader, group_leader_file_descriptor, config.process_id(), cpu_id)
              << std::flush;
  }

  /// Notify the caller that opening the counter via the perf subsystem failed.
  if (this->_file_descriptor < 0LL) {
    throw CannotOpenCounterError{ Counter::error_message_from_errno(error_code), error_code };
  }

  if (buffer_pages.has_value()) {
    /// Open the mapped buffer.
    this->_user_level_buffer =
      reinterpret_cast<perf_event_mmap_page*>(::mmap(nullptr,
                                                     buffer_pages.value() * /* page size */ 4096U,
                                                     PROT_READ,
                                                     MAP_SHARED,
                                                     static_cast<std::int32_t>(this->_file_descriptor),
                                                     0));

    /// Notify the caller if buffer-allocation via ::mmap() failed.
    if (this->_user_level_buffer == MAP_FAILED) {
      throw MmapError{ errno };
    } else if (this->_user_level_buffer == nullptr) {
      throw MmapNullError{};
    }

    this->_user_level_buffer_pages = buffer_pages;
  }
}

void
perf::Counter::close()
{
  /// Close/un-map the mmaped buffer, if any.
  if (auto* const user_level_buffer = std::exchange(this->_user_level_buffer, nullptr); user_level_buffer != nullptr) {
    if (const auto user_level_buffer_pages = std::exchange(this->_user_level_buffer_pages, std::nullopt);
        user_level_buffer_pages.has_value()) {
      ::munmap(user_level_buffer, user_level_buffer_pages.value() * /* page size */ 4096U);
    }
  }

  /// Close the file descriptor.
  if (const auto file_descriptor = std::exchange(this->_file_descriptor, -1LL); file_descriptor > -1LL) {
    ::close(static_cast<std::int32_t>(file_descriptor));
  }
}

void
perf::Counter::enable() const
{
  ::ioctl(static_cast<std::int32_t>(this->_file_descriptor), PERF_EVENT_IOC_RESET, 0);
  ::ioctl(static_cast<std::int32_t>(this->_file_descriptor), PERF_EVENT_IOC_ENABLE, 0);
}

void
perf::Counter::disable() const
{
  ::ioctl(static_cast<std::int32_t>(this->_file_descriptor), PERF_EVENT_IOC_DISABLE, 0);
}

std::uint64_t
perf::Counter::read_live() const noexcept
{
  /// Read the counter without stopping/disabling it via the "rdpmc" instruction.
  /// This is only possible on x86 architectures.
  /// For more details see https://man7.org/linux/man-pages/man2/perf_event_open.2.html (section MMAP layout).

#if defined(__x86_64__) || defined(__i386__)
  std::uint64_t value;
  std::uint32_t lock;

  do {
    lock = this->_user_level_buffer->lock;

    /// Memory fence.
    asm volatile("" ::: "memory");

    /// Read the hardware counter identifier.
    const auto index = this->_user_level_buffer->index;

    /// Verify that "rdpmc" is allowed.
    if (index == 0U) {
      return 0ULL;
    }

    /// Offset that must be added to the value.
    const auto offset = this->_user_level_buffer->offset;

    /// Read the value.
    value = std::uint64_t(std::int64_t(_rdpmc(index - 1U)) + offset);

    asm volatile("" ::: "memory");
  } while (this->_user_level_buffer->lock != lock);

  return value;
#else
  return 0ULL;
#endif
}

std::int64_t
perf::Counter::perf_event_open(const pid_t process_id,
                               const std::int32_t cpu_id,
                               const bool is_group_leader,
                               const std::int64_t group_leader_file_descriptor)
{
  /// Finally, pass the configuration to the perf subsystem to open the hardware performance counter.
  return ::syscall(__NR_perf_event_open,
                   &this->_event_attribute,
                   process_id,
                   cpu_id,
                   is_group_leader ? -1LL : group_leader_file_descriptor,
                   0);
}

bool
perf::Counter::is_adjust_precise_ip(const std::uint8_t current_precise_ip,
                                    const std::optional<std::uint64_t> sample_type,
                                    const std::int64_t error_code) noexcept
{
  /// If the counter was only opened for counting or live counting (indicated by PERF_SAMPLE_READ), precise_ip has no
  /// impact.
  if (sample_type.value_or(PERF_SAMPLE_READ) == PERF_SAMPLE_READ) {
    return false;
  }

  /// When precise_ip is already the lowest possible configuration (0 or lower), lowering has no impact.
  if (current_precise_ip < 1U || current_precise_ip > 3U) {
    return false;
  }

  /// EINVAL indicates an invalid argument, which could be a too high value for precise_ip.
  /// Likewise, EOPNOTSUPP could indicate that such a high value of precise_ip is not supported on the underlying
  /// machine. In both scenarios, we should decrease the value and try again.
  return error_code == EINVAL || error_code == EOPNOTSUPP;
}

std::string
perf::Counter::error_message_from_errno(const std::int64_t error_code)
{
  switch (error_code) {
    case ENOENT:
      return "configuration might not be valid (e.g., wrong type or too many counters scheduled to the same hardware "
             "counter)";
    case E2BIG:
      return "perf_event_attr.size was not configured properly – this could be a bug in the perf-cpp library";
    case EACCES:
      return "insufficient access rights to start the counter, e.g. profiling a not user-owned process or "
             "perf_event_paranoid value too high";
#ifndef PERFCPP_NO_ERROR_EBUSY /// Busy error is reported since Linux 4.1
    case EBUSY:
      return "another event has exclusive access to the PMU";
#endif
    case EINVAL:
      return "counter is configured with an invalid argument (e.g., too high sample frequency, unknown CPU, invalid "
             "sample type)";
    case EMFILE:
      return "too many open file descriptors (e.g., too many opened counters?)";
    case ENODEV:
      return "configured with feature that does not exist on this CPU";
    case EOVERFLOW:
      return "maximal callchain stack size is higher than the maximum (see /proc/sys/kernel/perf_event_max_stack)";
    case EPERM:
      return "one of the following features is set but not supported: excluding hypervisor, excluding idle, excluding "
             "user, or excluding kernel";
    case ESRCH:
      return "specified process does not exist";
    default:
      return "perf_event_open failed with unknown error";
  }
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

  /// Sample type
  if (this->_event_attribute.sample_type > 0U) {
    stream << "        sample_type: ";

    auto is_print_delimiter =
      Counter::print_type_to_stream(stream, this->_event_attribute.sample_type, PERF_SAMPLE_IP, "IP", true);
    is_print_delimiter = Counter::print_type_to_stream(
      stream, this->_event_attribute.sample_type, PERF_SAMPLE_TID, "TID", is_print_delimiter);
    is_print_delimiter = Counter::print_type_to_stream(
      stream, this->_event_attribute.sample_type, PERF_SAMPLE_TIME, "TIME", is_print_delimiter);
    is_print_delimiter = Counter::print_type_to_stream(
      stream, this->_event_attribute.sample_type, PERF_SAMPLE_ADDR, "ADDR", is_print_delimiter);
    is_print_delimiter = Counter::print_type_to_stream(
      stream, this->_event_attribute.sample_type, PERF_SAMPLE_READ, "READ", is_print_delimiter);
    is_print_delimiter = Counter::print_type_to_stream(
      stream, this->_event_attribute.sample_type, PERF_SAMPLE_CALLCHAIN, "CALLCHAIN", is_print_delimiter);
    is_print_delimiter = Counter::print_type_to_stream(
      stream, this->_event_attribute.sample_type, PERF_SAMPLE_CPU, "CPU", is_print_delimiter);
    is_print_delimiter = Counter::print_type_to_stream(
      stream, this->_event_attribute.sample_type, PERF_SAMPLE_PERIOD, "PERIOD", is_print_delimiter);
    is_print_delimiter = Counter::print_type_to_stream(
      stream, this->_event_attribute.sample_type, PERF_SAMPLE_STREAM_ID, "STREAM_ID", is_print_delimiter);
    is_print_delimiter = Counter::print_type_to_stream(
      stream, this->_event_attribute.sample_type, PERF_SAMPLE_RAW, "RAW", is_print_delimiter);
    is_print_delimiter = Counter::print_type_to_stream(
      stream, this->_event_attribute.sample_type, PERF_SAMPLE_BRANCH_STACK, "BRANCH_STACK", is_print_delimiter);
    is_print_delimiter = Counter::print_type_to_stream(
      stream, this->_event_attribute.sample_type, PERF_SAMPLE_REGS_USER, "REGS_USER", is_print_delimiter);
    is_print_delimiter = Counter::print_type_to_stream(
      stream, this->_event_attribute.sample_type, PERF_SAMPLE_STACK_USER, "REGS_USER", is_print_delimiter);
    is_print_delimiter = Counter::print_type_to_stream(
      stream, this->_event_attribute.sample_type, PERF_SAMPLE_STACK_USER, "STACK_USER", is_print_delimiter);
    is_print_delimiter = Counter::print_type_to_stream(
      stream, this->_event_attribute.sample_type, PERF_SAMPLE_WEIGHT, "WEIGHT", is_print_delimiter);
    is_print_delimiter = Counter::print_type_to_stream(
      stream, this->_event_attribute.sample_type, PERF_SAMPLE_DATA_SRC, "DATA_SRC", is_print_delimiter);
    is_print_delimiter = Counter::print_type_to_stream(
      stream, this->_event_attribute.sample_type, PERF_SAMPLE_IDENTIFIER, "IDENTIFIER", is_print_delimiter);
    is_print_delimiter = Counter::print_type_to_stream(
      stream, this->_event_attribute.sample_type, PERF_SAMPLE_REGS_INTR, "REGS_INTR", is_print_delimiter);
#ifndef PERFCPP_NO_SAMPLE_PHYS_ADDR /// Sampling for physical address is supported since Linux 4.13
    is_print_delimiter = Counter::print_type_to_stream(
      stream, this->_event_attribute.sample_type, PERF_SAMPLE_PHYS_ADDR, "PHYS_ADDR", is_print_delimiter);
#endif

#ifndef PERFCPP_NO_SAMPLE_CGROUP /// Sampling for cgroup is supported since Linux 5.7
    is_print_delimiter = Counter::print_type_to_stream(
      stream, this->_event_attribute.sample_type, PERF_SAMPLE_CGROUP, "CGROUP", is_print_delimiter);
#endif

#ifndef PERFCPP_NO_SAMPLE_DATA_PAGE_SIZE /// Sampling for data page size is supported since Linux 5.11
    is_print_delimiter = Counter::print_type_to_stream(
      stream, this->_event_attribute.sample_type, PERF_SAMPLE_DATA_PAGE_SIZE, "DATA_PAGE_SIZE", is_print_delimiter);
#endif

#ifndef PERFCPP_NO_SAMPLE_CODE_PAGE_SIZE /// Sampling for code page size is supported since Linux 5.11
    is_print_delimiter = Counter::print_type_to_stream(
      stream, this->_event_attribute.sample_type, PERF_SAMPLE_CODE_PAGE_SIZE, "PAGE_SIZE", is_print_delimiter);
#endif

#ifndef PERFCPP_NO_SAMPLE_WEIGHT_STRUCT /// Sampling for weight struct is supported since Linux 5.12
    Counter::print_type_to_stream(
      stream, this->_event_attribute.sample_type, PERF_SAMPLE_WEIGHT_STRUCT, "WEIGHT_STRUCT", is_print_delimiter);
#endif

    stream << "\n";
  }

  /// Frequency or Period
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

  /// Read format
  if (this->_event_attribute.read_format > 0U) {
    stream << "        read_format: ";

    auto is_print_delimiter = Counter::print_type_to_stream(
      stream, this->_event_attribute.read_format, PERF_FORMAT_TOTAL_TIME_ENABLED, "TOTAL_TIME_ENABLED", true);
    is_print_delimiter = Counter::print_type_to_stream(stream,
                                                       this->_event_attribute.read_format,
                                                       PERF_FORMAT_TOTAL_TIME_RUNNING,
                                                       "TOTAL_TIME_RUNNING",
                                                       is_print_delimiter);
    is_print_delimiter = Counter::print_type_to_stream(
      stream, this->_event_attribute.read_format, PERF_FORMAT_ID, "ID", is_print_delimiter);
    is_print_delimiter = Counter::print_type_to_stream(
      stream, this->_event_attribute.read_format, PERF_FORMAT_GROUP, "GROUP", is_print_delimiter);
#ifndef PERFCPP_NO_FORMAT_LOST /// Reading lost values is supported since Linux 6.0
    Counter::print_type_to_stream(
      stream, this->_event_attribute.read_format, PERF_FORMAT_LOST, "LOST", is_print_delimiter);
#endif

    stream << "\n";
  }

  /// Branch type
  if (this->_event_attribute.branch_sample_type > 0U) {
    stream << "        branch_sample_type: ";

    auto is_print_delimiter = Counter::print_type_to_stream(
      stream, this->_event_attribute.branch_sample_type, PERF_SAMPLE_BRANCH_USER, "BRANCH_USER", true);
    is_print_delimiter = Counter::print_type_to_stream(stream,
                                                       this->_event_attribute.branch_sample_type,
                                                       PERF_SAMPLE_BRANCH_KERNEL,
                                                       "BRANCH_KERNEL",
                                                       is_print_delimiter);
    is_print_delimiter = Counter::print_type_to_stream(
      stream, this->_event_attribute.branch_sample_type, PERF_SAMPLE_BRANCH_HV, "BRANCH_HV", is_print_delimiter);
    is_print_delimiter = Counter::print_type_to_stream(
      stream, this->_event_attribute.branch_sample_type, PERF_SAMPLE_BRANCH_ANY, "BRANCH_ANY", is_print_delimiter);
    is_print_delimiter = Counter::print_type_to_stream(stream,
                                                       this->_event_attribute.branch_sample_type,
                                                       PERF_SAMPLE_BRANCH_ANY_CALL,
                                                       "BRANCH_ANY_CALL",
                                                       is_print_delimiter);
#ifndef PERFCPP_NO_SAMPLE_BRANCH_CALL /// Branch type "call" is supported since Linux 4.4
    is_print_delimiter = Counter::print_type_to_stream(
      stream, this->_event_attribute.branch_sample_type, PERF_SAMPLE_BRANCH_CALL, "BRANCH_CALL", is_print_delimiter);
#endif
    is_print_delimiter = Counter::print_type_to_stream(stream,
                                                       this->_event_attribute.branch_sample_type,
                                                       PERF_SAMPLE_BRANCH_IND_CALL,
                                                       "BRANCH_IND_CALL",
                                                       is_print_delimiter);
    is_print_delimiter = Counter::print_type_to_stream(stream,
                                                       this->_event_attribute.branch_sample_type,
                                                       PERF_SAMPLE_BRANCH_ANY_RETURN,
                                                       "BRANCH_ANY_RETURN",
                                                       is_print_delimiter);
#ifndef PERFCPP_NO_SAMPLE_BRANCH_IND_JUMP /// Branch type "indirect jump" is supported since Linux 4.2
    is_print_delimiter = Counter::print_type_to_stream(stream,
                                                       this->_event_attribute.branch_sample_type,
                                                       PERF_SAMPLE_BRANCH_IND_JUMP,
                                                       "BRANCH_IND_JUMP",
                                                       is_print_delimiter);
#endif
    is_print_delimiter = Counter::print_type_to_stream(stream,
                                                       this->_event_attribute.branch_sample_type,
                                                       PERF_SAMPLE_BRANCH_ABORT_TX,
                                                       "BRANCH_ABORT_TX",
                                                       is_print_delimiter);
    is_print_delimiter = Counter::print_type_to_stream(
      stream, this->_event_attribute.branch_sample_type, PERF_SAMPLE_BRANCH_IN_TX, "BRANCH_IN_TX", is_print_delimiter);
    Counter::print_type_to_stream(
      stream, this->_event_attribute.branch_sample_type, PERF_SAMPLE_BRANCH_NO_TX, "BRANCH_NO_TX", is_print_delimiter);

    stream << "\n";
  }
#ifndef PERFCPP_NO_SAMPLE_MAX_STACK /// Max sample stack is supported since Linux 4.8
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
#ifndef PERFCPP_NO_RECORD_SWITCH /// Context switch is supported since Linux 4.3
  if (this->_event_attribute.context_switch > 0U) {
    stream << "        context_switch: " << this->_event_attribute.context_switch << "\n";
  }
#endif
#ifndef PERFCPP_NO_RECORD_CGROUP /// cgroup is supported since Linux 5.7
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
                                    const bool is_need_print_delimiter)
{
  if (mask & type) {
    if (!is_need_print_delimiter) {
      stream << " | ";
    }

    stream << name;
    return false;
  }

  return is_need_print_delimiter;
}