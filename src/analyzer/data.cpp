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
  /// Copy of all data types; the result will contain a copy since we add the samples to the members.
  auto data_types = std::unordered_map<std::string_view, DataType>{};
  data_types.reserve(this->_instances.size());

  /// List of all registered instances and the linked data type.
  auto registered_addresses = std::vector<std::pair<std::uintptr_t, DataType*>>{};

  /// Unfold the list of (DataType, [instance addresses]) into a list of [(instance address, DataType)] to perform a
  /// lower bound search for each sample.
  for (const auto& [name, data_type_and_start_addresses] : this->_instances) {

    /// Copy the data type, if not already done.
    auto data_type_instances = data_types.find(name);
    if (data_type_instances == data_types.end()) {
      data_type_instances = std::get<0>(data_types.insert(std::make_pair(name, std::get<0>(data_type_and_start_addresses))));
    }

    /// Unfold the instance addresses into the registered_addresses list.
    const auto& data_type_start_addresses = std::get<1>(data_type_and_start_addresses);
    std::transform(
      data_type_start_addresses.cbegin(),
      data_type_start_addresses.cend(),
      std::back_inserter(registered_addresses),
      [&data_type = data_type_instances->second](const auto address) { return std::make_pair(address, &data_type); });
  }

  /// Sort the addresses to perform a lower bound search.
  std::sort(registered_addresses.begin(), registered_addresses.end(), DataTypeInstanceComp{});

  /// Scan the samples and annotate each sample to the member of a data type instance the sample may belong to.
  for (const auto& sample : samples) {
    const auto memory_address = sample.logical_memory_address().value_or(0ULL);

    if (memory_address > 0ULL) {

      /// For every sampled address, find the potentially linked data type instance.
      auto data_type_instance = std::lower_bound(
        registered_addresses.begin(), registered_addresses.end(), memory_address, DataTypeInstanceComp{});

      /// The lower bound will find the first data type instance that is not less than the sampled memory address.
      /// Thus, we have to check if (a) there is a potential data type instance and (b) not all elements are greater.
      if (data_type_instance != registered_addresses.end() && data_type_instance != registered_addresses.begin()) {

        /// Go back to the potential instance (we found the first that is greater than the potential start address).
        --data_type_instance;

        /// Calculate the offset within the data type (address - start of the instance)
        const auto offset = memory_address - data_type_instance->first;

        /// Find the member that the sample may linked to and append the sample to the member's samples.
        for (auto& member : data_type_instance->second->members()) {
          if (member.offset() <= offset && offset < (member.offset() + member.size())) {
            member.samples().emplace_back(sample);
          }
        }
      }
    }
  }

  /// Turn the map of data types (name -> DataType) into single list of DataTypes.
  auto result_data_types = std::vector<DataType>{};
  std::transform(data_types.begin(), data_types.end(), std::back_inserter(result_data_types), [](auto& data_type) {
    return std::move(data_type.second);
  });
  return DataAnalyzerResult{ std::move(result_data_types) };
}

std::string
perf::analyzer::DataAnalyzerResult::to_string() const
{
  auto column_headers = std::vector<std::string>{
    "",          "",        "samples",        "loads",           "avg. load lat.", "L1d hits",        "LFB hits",
    "L2 hits",   "L3 hits", "local RAM hits", "remote RAM hits", "stores",         "avg. store lat.", "TLB hits",
    "TLB misses"
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
    const auto& data_type = this->_data_types[data_type_index];

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
      stream << "{" << "\"name\":" << "\"" << member.name() << "\"," << "\"offset\":" << member.offset() << ","
             << "\"size\":" << member.size() << "," << "\"samples\":" << member.samples().size() << ","
             << "\"loads\":" << statistics.loads() << "," << "\"average load latency\":" << statistics.load_latency()
             << "," << "\"L1d hits\":" << statistics.l1_hits() << "," << "\"LFB hits\":" << statistics.lfb_hits() << ","
             << "\"L2 hits\":" << statistics.l2_hits() << "," << "\"L3 hits\":" << statistics.l3_hits() << ","
             << "\"L4 hits\":" << statistics.l4_hits() << "," << "\"local RAM hits\":" << statistics.local_ram_hits()
             << "," << "\"remote RAM hits\":" << statistics.remote_ram_hits() << ","
             << "\"stores\":" << statistics.stores() << ","
             << "\"average store latency\":" << statistics.store_latency() << ","
             << "\"TLB hits\":" << statistics.tlb_hits() << "," << "\"TLB misses\":" << statistics.tlb_misses() << "}";
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
           << delimiter << "remote RAM hits" << delimiter << "stores" << delimiter << "average store latency"
           << delimiter << "TLB hits" << delimiter << "TLB misses" << '\n';
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
             << statistics.stores() << delimiter << statistics.store_latency() << delimiter << statistics.tlb_hits()
             << delimiter << statistics.tlb_misses() << '\n';
    }
  }

  return stream.str();
}