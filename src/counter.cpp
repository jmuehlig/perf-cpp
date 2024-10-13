#include <algorithm>
#include <iomanip>
#include <perfcpp/counter.h>
#include <sstream>

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
    << "| Value"
    << std::setw(std::int32_t(max_value_length) - 4) << " "
    << "| Counter" << std::setw(std::int32_t(max_name_length) - 6) << " "
    << "|\n"

    /// Print the separator line.
    << "|" << std::string(max_value_length + 2U, '-') << "|" << std::string(max_name_length + 2U, '-') << "|";

  /// Print the results as columns.
  for (const auto& [name, value] : result) {
    table_stream << "\n| "
                 << std::setw(std::int32_t(max_value_length)) << value
                 << " | "
                 << name << std::setw(std::int32_t(max_name_length - name.size()) + 1) << " "
                 << "|";
  }

  table_stream << std::flush;

  return table_stream.str();
}

std::string
perf::Counter::to_string() const
{
  auto stream = std::stringstream{};

  stream << "Counter:\n"
         << "    id: " << this->_id << "\n"
         << "    file_descriptor: " << this->_file_descriptor << "\n"
         << "    perf_event_attr:\n"
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
    is_first = Counter::print_type_to_stream(
      stream, this->_event_attribute.sample_type, PERF_SAMPLE_PHYS_ADDR, "PHYS_ADDR", is_first);
    is_first =
      Counter::print_type_to_stream(stream, this->_event_attribute.sample_type, PERF_SAMPLE_CGROUP, "CGROUP", is_first);

#ifndef NO_PERF_SAMPLE_DATA_PAGE_SIZE
    is_first = Counter::print_type_to_stream(
      stream, this->_event_attribute.sample_type, PERF_SAMPLE_DATA_PAGE_SIZE, "DATA_PAGE_SIZE", is_first);
#endif

#ifndef NO_PERF_SAMPLE_CODE_PAGE_SIZE
    is_first = Counter::print_type_to_stream(
      stream, this->_event_attribute.sample_type, PERF_SAMPLE_CODE_PAGE_SIZE, "PAGE_SIZE", is_first);
#endif

#ifndef NO_PERF_SAMPLE_WEIGHT_STRUCT
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
    Counter::print_type_to_stream(stream, this->_event_attribute.read_format, PERF_FORMAT_LOST, "LOST", is_first);

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
    is_first = Counter::print_type_to_stream(
      stream, this->_event_attribute.branch_sample_type, PERF_SAMPLE_BRANCH_CALL, "BRANCH_CALL", is_first);
    is_first = Counter::print_type_to_stream(
      stream, this->_event_attribute.branch_sample_type, PERF_SAMPLE_BRANCH_IND_CALL, "BRANCH_IND_CALL", is_first);
    is_first = Counter::print_type_to_stream(
      stream, this->_event_attribute.branch_sample_type, PERF_SAMPLE_BRANCH_ANY_RETURN, "BRANCH_ANY_RETURN", is_first);
    is_first = Counter::print_type_to_stream(
      stream, this->_event_attribute.branch_sample_type, PERF_SAMPLE_BRANCH_IND_JUMP, "BRANCH_IND_JUMP", is_first);
    is_first = Counter::print_type_to_stream(
      stream, this->_event_attribute.branch_sample_type, PERF_SAMPLE_BRANCH_ABORT_TX, "BRANCH_ABORT_TX", is_first);
    is_first = Counter::print_type_to_stream(
      stream, this->_event_attribute.branch_sample_type, PERF_SAMPLE_BRANCH_IN_TX, "BRANCH_IN_TX", is_first);
    Counter::print_type_to_stream(
      stream, this->_event_attribute.branch_sample_type, PERF_SAMPLE_BRANCH_NO_TX, "BRANCH_NO_TX", is_first);

    stream << "\n";
  }

  if (this->_event_attribute.sample_max_stack > 0U) {
    stream << "        sample_max_stack: " << this->_event_attribute.sample_max_stack << "\n";
  }

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
  if (this->_event_attribute.context_switch > 0U) {
    stream << "        context_switch: " << this->_event_attribute.context_switch << "\n";
  }
  if (this->_event_attribute.cgroup > 0U) {
    stream << "        cgroup: " << this->_event_attribute.cgroup << "\n";
  }

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