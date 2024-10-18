#include <algorithm>
#include <iomanip>
#include <perfcpp/analyzer/data.h>
#include <sstream>
#include <stdexcept>
#include <unordered_set>

void
perf::analyzer::DataAnalyzer::add(perf::analyzer::DataType&& data_type)
{
  auto name = data_type.name();

  if (this->_instances.find(name) != this->_instances.end()) {
    throw std::runtime_error{ std::string{ "Data type " }.append(name).append(" is already registered.") };
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
perf::analyzer::DataAnalyzerResult::to_string() const noexcept
{
  auto column_headers = std::vector<std::string>{
    "", "",        "samples", "loads",          "avg. load lat.",  "L1d hits", "LFB hits",
    "L2 hits", "L3 hits", "local RAM hits", "remote RAM hits", "stores",   "avg. store lat."
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
      auto sum_load_latency = 0ULL;
      auto sum_store_latency = 0ULL;
      auto count_loads = 0ULL;
      auto count_stores = 0ULL;
      auto count_l1 = 0ULL;
      auto count_lfb = 0ULL;
      auto count_l2 = 0ULL;
      auto count_l3 = 0ULL;
      auto count_l4 = 0ULL;
      auto count_local_ram = 0ULL;
      auto count_remote_ram = 0ULL;
      for (const auto& sample : member.samples()) {
        if (sample.weight().has_value() && sample.data_src().has_value()) {
          sum_load_latency +=
            (static_cast<std::uint64_t>(sample.data_src().value().is_load()) * sample.weight().value().cache_latency());
          sum_store_latency += (static_cast<std::uint64_t>(sample.data_src().value().is_store()) *
                                sample.weight().value().cache_latency());

          count_loads += sample.data_src().value().is_load();
          count_stores += sample.data_src().value().is_store();
          count_l1 += sample.data_src().value().is_mem_l1();
          count_lfb += sample.data_src().value().is_mem_lfb();
          count_l2 += sample.data_src().value().is_mem_l2();
          count_l3 += sample.data_src().value().is_mem_l3();
          count_l4 += sample.data_src().value().is_mem_l4();

          count_local_ram += sample.data_src().value().is_mem_local_ram();
          count_remote_ram += sample.data_src().value().is_mem_remote_ram();
        }
      }

      auto columns = std::vector<std::string>{};
      columns.reserve(column_headers.size());

      columns.emplace_back(std::to_string(member.offset()).append(": "));
      columns.emplace_back(
        std::string{ member.name() }.append(" (").append(std::to_string(member.size())).append("B)"));
      columns.emplace_back(std::to_string(member.samples().size()));
      columns.emplace_back(std::to_string(count_loads));
      columns.emplace_back(count_loads > 0U ? std::to_string(sum_load_latency / count_loads) : "0");
      columns.emplace_back(std::to_string(count_l1));
      columns.emplace_back(std::to_string(count_lfb));
      columns.emplace_back(std::to_string(count_l2));
      columns.emplace_back(std::to_string(count_l3));
      columns.emplace_back(std::to_string(count_local_ram));
      columns.emplace_back(std::to_string(count_remote_ram));
      columns.emplace_back(std::to_string(count_stores));
      columns.emplace_back(count_stores > 0U ? std::to_string(sum_store_latency / count_stores) : "0");

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