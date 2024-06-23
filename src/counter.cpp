#include <algorithm>
#include <perfcpp/counter.h>
#include <sstream>

std::optional<double>
perf::CounterResult::get(const std::string& name) const noexcept
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
perf::Counter::to_string() const
{
  auto type_to_str =
    [](std::stringstream& stream, const auto mask, const auto type, std::string&& name, bool& is_first) {
      if (mask & type) {
        if (is_first == false) {
          stream << " | ";
        }

        stream << std::move(name);
        is_first = false;
      }
    };

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

    auto is_first = true;
    type_to_str(stream, this->_event_attribute.sample_type, PERF_SAMPLE_IP, "IP", is_first);
    type_to_str(stream, this->_event_attribute.sample_type, PERF_SAMPLE_TID, "TID", is_first);
    type_to_str(stream, this->_event_attribute.sample_type, PERF_SAMPLE_TIME, "TIME", is_first);
    type_to_str(stream, this->_event_attribute.sample_type, PERF_SAMPLE_ADDR, "ADDR", is_first);
    type_to_str(stream, this->_event_attribute.sample_type, PERF_SAMPLE_READ, "READ", is_first);
    type_to_str(stream, this->_event_attribute.sample_type, PERF_SAMPLE_CALLCHAIN, "CALLCHAIN", is_first);
    type_to_str(stream, this->_event_attribute.sample_type, PERF_SAMPLE_CPU, "CPU", is_first);
    type_to_str(stream, this->_event_attribute.sample_type, PERF_SAMPLE_PERIOD, "PERIOD", is_first);
    type_to_str(stream, this->_event_attribute.sample_type, PERF_SAMPLE_BRANCH_STACK, "BRANCH_STACK", is_first);
    type_to_str(stream, this->_event_attribute.sample_type, PERF_SAMPLE_REGS_USER, "REGS_USER", is_first);
    type_to_str(stream, this->_event_attribute.sample_type, PERF_SAMPLE_WEIGHT, "WEIGHT", is_first);
    type_to_str(stream, this->_event_attribute.sample_type, PERF_SAMPLE_DATA_SRC, "DATA_SRC", is_first);
    type_to_str(stream, this->_event_attribute.sample_type, PERF_SAMPLE_IDENTIFIER, "IDENTIFIER", is_first);
    type_to_str(stream, this->_event_attribute.sample_type, PERF_SAMPLE_REGS_INTR, "REGS_INTR", is_first);
    type_to_str(stream, this->_event_attribute.sample_type, PERF_SAMPLE_PHYS_ADDR, "PHYS_ADDR", is_first);

#ifndef NO_PERF_SAMPLE_DATA_PAGE_SIZE
    type_to_str(stream, this->_event_attribute.sample_type, PERF_SAMPLE_DATA_PAGE_SIZE, "DATA_PAGE_SIZE", is_first);
#endif

#ifndef NO_PERF_SAMPLE_CODE_PAGE_SIZE
    type_to_str(stream, this->_event_attribute.sample_type, PERF_SAMPLE_CODE_PAGE_SIZE, "PAGE_SIZE", is_first);
#endif

#ifndef NO_PERF_SAMPLE_WEIGHT_STRUCT
    type_to_str(stream, this->_event_attribute.sample_type, PERF_SAMPLE_WEIGHT_STRUCT, "WEIGHT_STRUCT", is_first);
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

  if (this->_event_attribute.read_format > 0U) {
    stream << "        read_format: ";

    auto is_first = true;
    type_to_str(
      stream, this->_event_attribute.read_format, PERF_FORMAT_TOTAL_TIME_ENABLED, "TOTAL_TIME_ENABLED", is_first);
    type_to_str(
      stream, this->_event_attribute.read_format, PERF_FORMAT_TOTAL_TIME_RUNNING, "TOTAL_TIME_RUNNING", is_first);
    type_to_str(stream, this->_event_attribute.read_format, PERF_FORMAT_ID, "ID", is_first);
    type_to_str(stream, this->_event_attribute.read_format, PERF_FORMAT_GROUP, "GROUP", is_first);
    type_to_str(stream, this->_event_attribute.read_format, PERF_FORMAT_LOST, "LOST", is_first);

    stream << "\n";
  }

  if (this->_event_attribute.branch_sample_type > 0U) {
    stream << "        branch_sample_type: ";

    auto is_first = true;
    type_to_str(stream, this->_event_attribute.branch_sample_type, PERF_SAMPLE_BRANCH_USER, "BRANCH_USER", is_first);
    type_to_str(
      stream, this->_event_attribute.branch_sample_type, PERF_SAMPLE_BRANCH_KERNEL, "BRANCH_KERNEL", is_first);
    type_to_str(stream, this->_event_attribute.branch_sample_type, PERF_SAMPLE_BRANCH_HV, "BRANCH_HV", is_first);
    type_to_str(stream, this->_event_attribute.branch_sample_type, PERF_SAMPLE_BRANCH_ANY, "BRANCH_ANY", is_first);
    type_to_str(
      stream, this->_event_attribute.branch_sample_type, PERF_SAMPLE_BRANCH_ANY_CALL, "BRANCH_ANY_CALL", is_first);
    type_to_str(stream, this->_event_attribute.branch_sample_type, PERF_SAMPLE_BRANCH_CALL, "BRANCH_CALL", is_first);
    type_to_str(
      stream, this->_event_attribute.branch_sample_type, PERF_SAMPLE_BRANCH_IND_CALL, "BRANCH_IND_CALL", is_first);
    type_to_str(
      stream, this->_event_attribute.branch_sample_type, PERF_SAMPLE_BRANCH_ANY_RETURN, "BRANCH_ANY_RETURN", is_first);
    type_to_str(
      stream, this->_event_attribute.branch_sample_type, PERF_SAMPLE_BRANCH_IND_JUMP, "BRANCH_IND_JUMP", is_first);
    type_to_str(
      stream, this->_event_attribute.branch_sample_type, PERF_SAMPLE_BRANCH_ABORT_TX, "BRANCH_ABORT_TX", is_first);
    type_to_str(stream, this->_event_attribute.branch_sample_type, PERF_SAMPLE_BRANCH_IN_TX, "BRANCH_IN_TX", is_first);
    type_to_str(stream, this->_event_attribute.branch_sample_type, PERF_SAMPLE_BRANCH_NO_TX, "BRANCH_NO_TX", is_first);

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

  return stream.str();
}