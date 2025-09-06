#include <algorithm>
#include <iomanip>
#include <perfcpp/counter_result.h>
#include <sstream>

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
    << "| Value" << std::setw(static_cast<std::int32_t>(max_value_length) - 4) << " " << "| Counter"
    << std::setw(static_cast<std::int32_t>(max_name_length) - 6) << " "
    << "|\n"

    /// Print the separator line.
    << "|" << std::string(max_value_length + 2U, '-') << "|" << std::string(max_name_length + 2U, '-') << "|";

  /// Print the results as columns.
  for (const auto& [name, value] : result) {
    table_stream << "\n| " << std::setw(static_cast<std::int32_t>(max_value_length)) << value << " | " << name
                 << std::setw(static_cast<std::int32_t>(max_name_length - name.size()) + 1) << " " << "|";
  }

  table_stream << std::flush;

  return table_stream.str();
}