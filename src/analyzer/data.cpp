#include <algorithm>
#include <iomanip>
#include <numeric>
#include <perfcpp/analyzer/data.h>
#include <perfcpp/exception.h>
#include <sstream>
#include <stdexcept>
#include <unordered_set>

void
perf::analyzer::DataAnalyzer::add(perf::analyzer::DataType&& data_type)
{
  auto name = data_type.name();

  if (this->_instances.find(name) != this->_instances.end()) {
    throw DataTypeAlreadyRegisteredError{ name };
  }

  this->_instances.insert(
    std::make_pair(std::move(name), std::make_pair(std::move(data_type), std::vector<std::uintptr_t>{})));
}

void
perf::analyzer::DataAnalyzer::annotate(const std::string& name, const std::uintptr_t reference)
{
  if (auto iterator = _instances.find(name); iterator != _instances.end()) {
    iterator->second.second.push_back(reference);
  }
}

void
perf::analyzer::DataAnalyzer::annotate(const std::string& name,
                                       const void* reference,
                                       const std::uint64_t items_in_array)
{
  if (auto iterator = _instances.find(name); iterator != _instances.end()) {
    auto& data_type = std::get<0>(iterator->second);
    const auto data_type_size = data_type.size();

    const auto array_begin = std::uintptr_t(reference);
    const auto array_end = array_begin + data_type_size * items_in_array;

    for (auto addr = array_begin; addr < array_end; addr += data_type_size) {
      std::get<1>(iterator->second).emplace_back(addr);
    }
  }
}

perf::analyzer::DataAnalyzerResult
perf::analyzer::DataAnalyzer::map(const std::vector<Sample>& samples)
{
  auto instances = this->_instances;

  auto member_map = std::vector<std::pair<DataType::Member*, std::unordered_set<std::uintptr_t>>>{};
  member_map.reserve(instances.size() * 8U);

  for (auto& [_, data_type_and_instances] : instances) {
    auto& data_type = std::get<0>(data_type_and_instances);
    const auto& data_type_instances = std::get<1>(data_type_and_instances);
    for (auto& member : data_type.members()) {
      auto member_instances = std::unordered_set<std::uintptr_t>{};
      member_instances.reserve(data_type_instances.size() * member.size());
      for (const auto instance : data_type_instances) {
        for (auto member_byte = instance + member.offset(); member_byte < instance + member.offset() + member.size();
             ++member_byte) {
          member_instances.insert(member_byte);
        }
      }

      member_map.emplace_back(&member, std::move(member_instances));
    }
  }

  for (const auto& sample : samples) {
    if (sample.logical_memory_address().has_value()) {
      const auto memory_address = sample.logical_memory_address().value();

      for (auto& member : member_map) {
        if (auto iterator = std::get<1>(member).find(memory_address); iterator != std::get<1>(member).end()) {
          std::get<0>(member)->samples().emplace_back(sample);
          break;
        }
      }
    }
  }

  auto data_object = std::vector<DataType>{};
  for (auto& [_, data_type] : instances) {
    data_object.push_back(std::move(std::get<0>(data_type)));
  }

  return DataAnalyzerResult{ std::move(data_object) };
}

std::string
perf::analyzer::DataAnalyzerResult::to_string() const
{
  auto column_headers = std::vector<std::string>{
    "",        "",        "samples",        "loads",           "avg. load lat.", "L1d hits",       "LFB hits",
    "L2 hits", "L3 hits", "local RAM hits", "remote RAM hits", "stores",         "avg. store lat.", "TLB hits", "TLB misses"
  };
  auto max_sizes = std::vector<std::uint64_t>{};
  for (const auto& header : column_headers) {
    max_sizes.emplace_back(header.size());
  }

  auto data_types = std::vector<std::pair<std::string, std::vector<std::vector<std::string>>>>{};

  for (const auto& data_type : this->_data_types) {
    auto name = data_type.name();
    auto members = std::vector<std::vector<std::string>>{};

    for (const auto& member : data_type.members()) {

      const auto statistics = std::accumulate(member.samples().cbegin(),
                                              member.samples().cend(),
                                              MemberStatistic{},
                                              [](auto& current, const auto& sample) { return current += sample; });

      auto columns = std::vector<std::string>{};
      columns.reserve(column_headers.size());

      columns.emplace_back(std::to_string(member.offset()).append(": "));
      columns.emplace_back(
        std::string{ member.name() }.append(" (").append(std::to_string(member.size())).append("B)"));
      columns.emplace_back(std::to_string(member.samples().size()));
      columns.emplace_back(std::to_string(statistics.loads()));
      columns.emplace_back(std::to_string(statistics.load_latency()));
      columns.emplace_back(std::to_string(statistics.l1_hits()));
      columns.emplace_back(std::to_string(statistics.lfb_hits()));
      columns.emplace_back(std::to_string(statistics.l2_hits()));
      columns.emplace_back(std::to_string(statistics.l3_hits()));
      columns.emplace_back(std::to_string(statistics.local_ram_hits()));
      columns.emplace_back(std::to_string(statistics.remote_ram_hits()));
      columns.emplace_back(std::to_string(statistics.stores()));
      columns.emplace_back(std::to_string(statistics.store_latency()));
      columns.emplace_back(std::to_string(statistics.tlb_hits()));
      columns.emplace_back(std::to_string(statistics.tlb_misses()));

      for (auto i = 0U; i < max_sizes.size(); ++i) {
        max_sizes[i] = std::max(max_sizes[i], columns[i].size());
      }

      members.emplace_back(std::move(columns));
    }

    data_types.emplace_back(std::move(name), std::move(members));
  }

  auto stream = std::stringstream{};
  for (auto data_type_id = 0U; data_type_id < data_types.size(); ++data_type_id) {
    const auto& [name, members] = data_types[data_type_id];

    if (data_type_id > 0U) {
      stream << "\n";
    }

    stream << "DataType " << name << " {\n";

    stream << " ";
    for (auto header_id = 0U; header_id < column_headers.size(); ++header_id) {
      if (header_id == 1U) {
        stream << column_headers[header_id] << std::string(max_sizes[1U] - column_headers[1].size(), ' ');
      } else {
        stream << "   " << std::setw(std::int32_t(max_sizes[header_id])) << column_headers[header_id];
      }
    }
    stream << "\n";

    for (const auto& member : members) {
      stream << " ";

      for (auto column_id = 0U; column_id < member.size(); ++column_id) {
        if (column_id == 1U) {
          stream << member[column_id] << std::string(max_sizes[1U] - member[1].size(), ' ');
        } else {
          stream << "   " << std::setw(std::int32_t(max_sizes[column_id])) << member[column_id];
        }
      }

      stream << "\n";
    }

    stream << "}\n";
  }

  return stream.str();
}

std::string
perf::analyzer::DataAnalyzerResult::to_json() const
{
  auto stream = std::stringstream{};
  stream << "[";

  for (auto data_type_index = 0U; data_type_index < this->_data_types.size(); ++data_type_index) {
    const auto &data_type = this->_data_types[data_type_index];

    if (data_type_index != 0U) {
      stream << ",";
    }
    stream << "{ \"name\":\"" << data_type.name() << "\", \"members\": [";

    for (auto member_index = 0U; member_index < data_type.members().size(); ++member_index) {
      const auto& member = data_type.members()[member_index];

      const auto statistics = std::accumulate(member.samples().cbegin(),
                                              member.samples().cend(),
                                              MemberStatistic{},
                                              [](auto& current, const auto& sample) { return current += sample; });

      if (member_index != 0U) {
        stream << ",";
      }
      stream
        << "{"
        << "\"name\":" << "\"" << member.name() << "\","
        << "\"offset\":" << member.offset() << ","
        << "\"size\":" << member.size() << ","
        << "\"samples\":" << member.samples().size() << ","
        << "\"loads\":" << statistics.loads() << ","
        << "\"average load latency\":" << statistics.load_latency() << ","
        << "\"L1d hits\":" << statistics.l1_hits() << ","
        << "\"LFB hits\":" << statistics.lfb_hits() << ","
        << "\"L2 hits\":" << statistics.l2_hits() << ","
        << "\"L3 hits\":" << statistics.l3_hits() << ","
        << "\"L4 hits\":" << statistics.l4_hits() << ","
        << "\"local RAM hits\":" << statistics.local_ram_hits() << ","
        << "\"remote RAM hits\":" << statistics.remote_ram_hits() << ","
        << "\"stores\":" << statistics.stores() << ","
        << "\"average store latency\":" << statistics.store_latency() << ","
        << "\"TLB hits\":" << statistics.tlb_hits() << ","
        << "\"TLB misses\":" << statistics.tlb_misses()
        << "}";
    }

    stream << "]}";

  }

  stream << "]";
  return stream.str();
}

std::string
perf::analyzer::DataAnalyzerResult::to_csv(const std::string& data_type_name,
                                           const char delimiter,
                                           const bool print_header) const
{
  auto stream = std::stringstream{};

  if (print_header) {
    stream << "name" << delimiter << "offset" << delimiter << "size" << delimiter << "samples" << delimiter << "loads"
           << delimiter << "average load latency" << delimiter << "L1d hits" << delimiter << "LFB hits" << delimiter
           << "L2 hits" << delimiter << "L3 hits" << delimiter << "L4 hits" << delimiter << "local RAM hits"
           << delimiter << "remote RAM hits" << delimiter << "stores" << delimiter << "average store latency" << delimiter << "TLB hits" << delimiter << "TLB misses" << '\n';
  }

  if (auto data_type =
        std::find_if(this->_data_types.cbegin(),
                     this->_data_types.cend(),
                     [&data_type_name](const auto& data_type) { return data_type.name() == data_type_name; });
      data_type != this->_data_types.cend()) {
    for (const auto& member : data_type->members()) {

      const auto statistics = std::accumulate(member.samples().cbegin(),
                                              member.samples().cend(),
                                              MemberStatistic{},
                                              [](auto& current, const auto& sample) { return current += sample; });

      stream << member.name() << delimiter << member.offset() << delimiter << member.size() << delimiter
             << member.samples().size() << delimiter << statistics.loads() << delimiter << statistics.load_latency()
             << delimiter << statistics.l1_hits() << delimiter << statistics.lfb_hits() << delimiter
             << statistics.l2_hits() << delimiter << statistics.l3_hits() << delimiter << statistics.l4_hits()
             << delimiter << statistics.local_ram_hits() << delimiter << statistics.remote_ram_hits() << delimiter
             << statistics.stores() << delimiter << statistics.store_latency() << delimiter << statistics.tlb_hits() << delimiter << statistics.tlb_misses() << '\n';
    }
  }

  return stream.str();
}