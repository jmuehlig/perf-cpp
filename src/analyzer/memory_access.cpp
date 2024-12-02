#include <algorithm>
#include <iomanip>
#include <numeric>
#include <perfcpp/analyzer/memory_access.h>
#include <perfcpp/exception.h>
#include <sstream>
#include <stdexcept>
#include <unordered_set>

void
perf::analyzer::MemoryAccess::add(perf::analyzer::DataType&& data_type)
{
  const auto data_type_iterator = this->find(data_type.name());
  if (data_type_iterator == this->_data_type_instances.end()) {
    this->_data_type_instances.emplace_back(std::move(data_type),
                                            std::unordered_map<std::string, std::vector<std::uintptr_t>>{});
  } else {
    throw DataTypeAlreadyRegisteredError{ data_type.name() };
  }
}

void
perf::analyzer::MemoryAccess::annotate(const std::string_view data_type_name,
                                       const std::uintptr_t data_object,
                                       const std::string& instance_name)
{
  if (auto type_iterator = this->find(data_type_name); type_iterator != this->_data_type_instances.end()) {
    auto& type_instances = type_iterator->second;

    /// Check if the data object already contains the tag.
    if (auto tag_iterator = type_instances.find(instance_name); tag_iterator != type_instances.end()) {
      tag_iterator->second.push_back(data_object);
    } else {
      /// If the tag did not exist, add it as a new map tag -> [addresses].
      auto instances = std::vector<std::uintptr_t>{};
      instances.reserve(2048U);
      instances.push_back(data_object);
      type_instances.insert(std::make_pair(instance_name, std::move(instances)));
    }
  } else {
    throw DataTypeNotRegisteredError{ data_type_name };
  }
}

std::vector<std::pair<perf::analyzer::DataType, std::unordered_map<std::string, std::vector<std::uintptr_t>>>>::iterator
perf::analyzer::MemoryAccess::find(std::string_view data_type_name) noexcept
{
  return std::find_if(this->_data_type_instances.begin(),
                      this->_data_type_instances.end(),
                      [data_type_name](const auto& data_type_and_tags) {
                        return std::get<0>(data_type_and_tags).name() == data_type_name;
                      });
}

perf::analyzer::MemoryAccessResult
perf::analyzer::MemoryAccess::map(const std::vector<Sample>& samples)
{
  /// Copy of all data types; the result will contain a copy since we add the samples to the members.
  auto data_types = std::vector<DataType>{};
  data_types.reserve(this->_data_type_instances.size());

  /// List of all registered instances and the linked data type.
  auto registered_addresses = std::vector<std::pair<std::uintptr_t, std::reference_wrapper<DataType>>>{};

  /// Unfold the list of (DataType, [instance addresses]) into a list of [(instance address, DataType)] to perform a
  /// lower bound search for each sample.
  for (const auto& [data_type, tags] : this->_data_type_instances) {

    for (const auto& [tag, addresses] : tags) {
      auto data_typ_tag_name =
        tag.empty() ? data_type.name() : std::string{ data_type.name() }.append("::").append(tag);

      /// Copy the data type, if not already done.
      auto& tagged_data_type = data_types.emplace_back(std::move(data_typ_tag_name), data_type);

      /// Fill up the empty spaces in the data type.
      MemoryAccess::add_empty_attributes(tagged_data_type);

      /// Unfold the instance addresses into the registered_addresses list.
      std::transform(
        addresses.cbegin(),
        addresses.cend(),
        std::back_inserter(registered_addresses),
        [&tagged_data_type](const auto address) { return std::make_pair(address, std::ref(tagged_data_type)); });
    }
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
        for (auto& member : data_type_instance->second.get().members()) {
          if (member.offset() <= offset && offset < (member.offset() + member.size())) {
            member.samples().emplace_back(sample);
          }
        }
      }
    }
  }

  return MemoryAccessResult{ std::move(data_types) };
}

void
perf::analyzer::MemoryAccess::add_empty_attributes(perf::analyzer::DataType& data_type)
{
  auto& members = data_type.members();
  if (members.empty()) {
    return;
  }

  const auto size = members.size();
  for (auto i = 0U; i < size - 1U; ++i) {

    /// Check if there is a whole between two members i and i+1.
    const auto distance = members[i + 1U].offset() - (members[i].offset() + members[i].size());

    /// If there is a whole, add a new member indicating that whole.
    if (distance > 0U) {
      members.insert(members.begin() + i + 1U,
                     DataType::Member("/* unknown */", members[i].offset() + members[i].size(), distance));
      ++i;
    }
  }

  /// Repeat the step for the last member and the size of the data type.
  const auto& last_member = members[size - 1U];
  const auto distance = data_type.size() - (last_member.offset() + last_member.size());
  if (distance > 0U) {
    members.emplace_back("/* unknown */", last_member.offset() + last_member.size(), distance);
  }
}

std::string
perf::analyzer::MemoryAccessResult::to_string() const
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

  auto data_types = std::vector<std::tuple<std::string, std::size_t, std::vector<std::vector<std::string>>>>{};

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

    data_types.emplace_back(std::move(name), data_type.size(), std::move(members));
  }

  auto stream = std::stringstream{};
  for (auto data_type_id = 0U; data_type_id < data_types.size(); ++data_type_id) {
    const auto& [name, size, members] = data_types[data_type_id];

    if (data_type_id > 0U) {
      stream << "\n";
    }

    stream << "DataType " << name << " (" << size << "B)" << " {\n";

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
perf::analyzer::MemoryAccessResult::to_json() const
{
  auto stream = std::stringstream{};
  stream << "[";

  for (auto data_type_index = 0U; data_type_index < this->_data_types.size(); ++data_type_index) {
    const auto& data_type = this->_data_types[data_type_index];

    if (data_type_index != 0U) {
      stream << ",";
    }
    stream << "{ \"name\":\"" << data_type.name() << "\", \"size\": " << data_type.size() << ", \"members\": [";

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
perf::analyzer::MemoryAccessResult::to_csv(const std::string& data_type_name,
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

  if (auto data_type_iterator =
        std::find_if(this->_data_types.cbegin(),
                     this->_data_types.cend(),
                     [&data_type_name](const auto& data_type) { return data_type.name() == data_type_name; });
      data_type_iterator != this->_data_types.cend()) {
    for (const auto& member : data_type_iterator->members()) {

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