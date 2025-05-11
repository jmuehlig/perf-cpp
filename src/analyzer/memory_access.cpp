#include <algorithm>
#include <numeric>
#include <perfcpp/analyzer/memory_access.h>
#include <perfcpp/exception.h>
#include <perfcpp/table.h>
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

  if (registered_addresses.empty()) {
    return MemoryAccessResult{};
  }

  /// Sort the addresses to perform a lower bound search.
  std::sort(registered_addresses.begin(), registered_addresses.end(), DataTypeInstanceComp{});

  /// Scan the samples and annotate each sample to the member of a data type instance the sample may belong to.
  for (const auto& sample : samples) {
    if (const auto memory_address = sample.data_access().logical_memory_address().value_or(0ULL);
        memory_address > 0ULL) {

      /// For every sampled address, find the potentially linked data type instance.
      auto data_type_instance = std::lower_bound(
        registered_addresses.begin(), registered_addresses.end(), memory_address, DataTypeInstanceComp{});

      /// The lower bound will find the first data type instance that is not less than the sampled memory address.
      /// Thus, we have to check if (a) there is a potential data type instance and (b) not all elements are greater.
      if (data_type_instance != registered_addresses.begin()) {

        /// Go back to the potential instance (we found the first that is greater than the potential start address).
        --data_type_instance;

        /// Calculate the offset within the data type (address - start of the instance)
        const auto offset = memory_address - data_type_instance->first;

        /// Verify that the address maps to that object instance.
        if (offset < data_type_instance->second.get().size()) {
          /// Find the member that the sample may linked to and append the sample to the member's samples.
          for (auto& member : data_type_instance->second.get().members()) {
            if (member.offset() <= offset && offset < (member.offset() + member.size())) {
              member.samples().emplace_back(sample);
              break;
            }
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

  auto size = members.size();
  for (auto i = 0U; i < size - 1U; ++i) {

    /// Check if there is a whole between two members i and i+1.
    const auto distance = members[i + 1U].offset() - (members[i].offset() + members[i].size());

    /// If there is a whole, add a new member indicating that whole.
    if (distance > 0U) {
      members.insert(members.begin() + (i + 1U),
                     DataType::Member("/* unknown */", members[i].offset() + members[i].size(), distance));
      ++i;
      ++size;
    }
  }

  /// Repeat the step for the last member and the size of the data type.
  const auto& last_member = members.back();
  const auto distance = data_type.size() - (last_member.offset() + last_member.size());
  if (distance > 0U) {
    members.emplace_back("/* unknown */", last_member.offset() + last_member.size(), distance);
  }
}

std::string
perf::analyzer::MemoryAccessResult::to_string() const
{
  auto data_types = std::vector<std::tuple<std::string, std::size_t, Table, std::size_t>>{};

  for (const auto& data_type : this->_data_types) {
    auto name = data_type.name();

    auto count_samples = 0ULL;

    /// Create the data type table.
    auto alignment = std::vector<Table::Alignment>(27U + static_cast<std::uint8_t>(HardwareInfo::is_amd()) * 6U,
                                                   Table::Alignment::Right);
    alignment[1U] = Table::Alignment::Left;

    auto table = Table{ 2U, std::move(alignment) };
    table.reserve(data_type.members().size() + 3U);

    /// Access type headers.
    auto access_type_headers = Table::Row{ 4U };
    access_type_headers << Table::Column{ "", 3U }
                        << Table::Column{ "loads",
                                          std::uint8_t(11U + static_cast<std::uint8_t>(HardwareInfo::is_amd()) * 2U) }
                        << Table::Column{ "software prefetches",
                                          std::uint8_t(11U + static_cast<std::uint8_t>(HardwareInfo::is_amd()) * 2U) }
                        << Table::Column{ "stores",
                                          std::uint8_t(2U + static_cast<std::uint8_t>(HardwareInfo::is_amd()) * 2U) };
    table.add(std::move(access_type_headers));

    /// Category headers.
    auto group_headers = Table::Row{ 13U };
    group_headers
      << Table::Column{ "", 3U } /// Loads
      << "" << Table::Column{ "latency", std::uint8_t(2U + static_cast<std::uint8_t>(HardwareInfo::is_amd())) }
      << Table::Column{ "cache hits", 4U } << Table::Column{ "RAM hits", 2U }
      << Table::Column{ "TLB", std::uint8_t(2U + static_cast<std::uint8_t>(HardwareInfo::is_amd())) }
      /// Software prefetches
      << "" << Table::Column{ "latency", std::uint8_t(2U + static_cast<std::uint8_t>(HardwareInfo::is_amd())) }
      << Table::Column{ "cache hits", 4U } << Table::Column{ "RAM hits", 2U }
      << Table::Column{ "TLB", std::uint8_t(2U + static_cast<std::uint8_t>(HardwareInfo::is_amd())) } /// Stores
      << "" << Table::Column{ "latency", std::uint8_t(1U + static_cast<std::uint8_t>(HardwareInfo::is_amd()) * 2U) };
    table.add(std::move(group_headers));

    /// Last headers.
    auto header = Table::Row{ 27U + static_cast<std::uint8_t>(HardwareInfo::is_amd()) * 6U };

    /// Offset, name, and samples.
    header << Table::Column{ "", 2U } << "samples";

    /// Loads
    header << "count";

    /// Latency
    if (HardwareInfo::is_amd()) {
      header << "cache" << "uOp" << "dTLB";
    } else {
      header << "cache" << "instr.";
    }

    /// Cache
    header << "L1d" << (HardwareInfo::is_amd() ? "MAB" : "LFB") << "L2" << "L3";

    /// RAM
    header << "local" << "remote";

    /// TLB
    if (HardwareInfo::is_amd()) {
      header << "dTLB" << "STLB" << "miss";
    } else {
      header << "hit" << "miss";
    }

    /// Software prefetches
    header << "count";

    /// Latency
    if (HardwareInfo::is_amd()) {
      header << "cache" << "uOp" << "dTLB";
    } else {
      header << "cache" << "instr.";
    }

    /// Cache
    header << "L1d" << (HardwareInfo::is_amd() ? "MAB" : "LFB") << "L2" << "L3";

    /// RAM
    header << "local" << "remote";

    /// TLB
    if (HardwareInfo::is_amd()) {
      header << "dTLB" << "STLB" << "miss";
    } else {
      header << "hit" << "miss";
    }

    /// Stores
    header << "count";

    /// Latency
    if (HardwareInfo::is_amd()) {
      header << "cache" << "uOp" << "dTLB miss";
    } else {
      header << "instr.";
    }
    table.add(std::move(header));

    /// Add column separators.
    auto separators = std::vector<char>(29U + static_cast<std::uint8_t>(HardwareInfo::is_amd()) * 4U, '\0');
    separators[2U] = '|';
    separators[3U] = '|';
    separators[4U] = '|';
    separators[6U + static_cast<std::size_t>(HardwareInfo::is_amd())] = '|';
    separators[10U + static_cast<std::size_t>(HardwareInfo::is_amd())] = '|';
    separators[12U + static_cast<std::size_t>(HardwareInfo::is_amd())] = '|';
    separators[14U + static_cast<std::size_t>(HardwareInfo::is_amd()) * 2U] = '|';
    separators[15U + static_cast<std::size_t>(HardwareInfo::is_amd()) * 2U] = '|';
    separators[17U + static_cast<std::size_t>(HardwareInfo::is_amd()) * 3U] = '|';
    separators[21U + static_cast<std::size_t>(HardwareInfo::is_amd()) * 3U] = '|';
    separators[23U + static_cast<std::size_t>(HardwareInfo::is_amd()) * 3U] = '|';
    separators[25U + static_cast<std::size_t>(HardwareInfo::is_amd()) * 4U] = '|';
    separators[26U + static_cast<std::size_t>(HardwareInfo::is_amd()) * 4U] = '|';
    separators[27U + static_cast<std::size_t>(HardwareInfo::is_amd()) * 6U] = '|';
    table.column_separators(std::move(separators));

    /// Add member attributes to table.
    for (const auto& member : data_type.members()) {

      const auto statistics = std::accumulate(member.samples().cbegin(),
                                              member.samples().cend(),
                                              MemberStatistic{},
                                              [](auto& current, const auto& sample) { return current += sample; });

      auto row = Table::Row{ 29U + static_cast<std::uint8_t>(HardwareInfo::is_amd()) * 4U };
      auto member_offset = std::to_string(member.offset()).append(": ");
      auto member_name = std::string{ member.name() }.append(" (").append(std::to_string(member.size())).append("B)");
      row << member_offset << member_name << member.samples().size();

      /// Loads
      row << statistics.loads().count() << statistics.loads().cache_latency() << statistics.loads().instr_latency();
      if (HardwareInfo::is_amd()) {
        row << statistics.loads().dtlb_latency();
      }
      row << statistics.loads().count_l1_hits() << statistics.loads().count_mhb_hits()
          << statistics.loads().count_l2_hits() << statistics.loads().count_l3_hits()
          << statistics.loads().count_local_ram_hits() << statistics.loads().count_remote_ram_hits()
          << statistics.loads().dtlb_hits() << statistics.loads().stlb_hits() << statistics.loads().stlb_misses();

      /// Software Prefetches
      row << statistics.software_prefetches().count() << statistics.software_prefetches().cache_latency()
          << statistics.software_prefetches().instr_latency();
      if (HardwareInfo::is_amd()) {
        row << statistics.software_prefetches().dtlb_latency();
      }
      row << statistics.software_prefetches().count_l1_hits() << statistics.software_prefetches().count_mhb_hits()
          << statistics.software_prefetches().count_l2_hits() << statistics.software_prefetches().count_l3_hits()
          << statistics.software_prefetches().count_local_ram_hits()
          << statistics.software_prefetches().count_remote_ram_hits() << statistics.software_prefetches().dtlb_hits()
          << statistics.software_prefetches().stlb_hits() << statistics.software_prefetches().stlb_misses();

      /// Stores
      row << statistics.stores().count();
      if (HardwareInfo::is_amd()) {
        row << statistics.stores().cache_latency() << statistics.stores().instr_latency()
            << statistics.stores().dtlb_latency();
      } else {
        row << statistics.stores().instr_latency();
      }

      table.add(std::move(row));

      count_samples += member.samples().size();
    }

    data_types.emplace_back(std::move(name), data_type.size(), std::move(table), count_samples);
  }

  /// Sort data type (instances) by number of samples to show types with more samples first.
  std::sort(data_types.begin(), data_types.end(), [](const auto& left, const auto& right) {
    return std::get<3>(left) > std::get<3>(right);
  });

  /// Print the data types with members as table.
  auto stream = std::stringstream{};
  for (auto data_type_id = 0U; data_type_id < data_types.size(); ++data_type_id) {
    const auto& [name, size, table, _] = data_types[data_type_id];

    if (data_type_id > 0U) {
      stream << "\n";
    }

    stream << "DataType " << name << " (" << size << "B)" << " {\n" << table.to_string() << "}\n";
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
    stream << R"({ "name":")" << data_type.name() << R"(", "size": )" << data_type.size() << ", \"members\": [";

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
             << "\"size\":" << member.size() << "," << "\"samples\":" << member.samples().size() << ",";

      /// Loads
      stream << "\"loads\": {" << "\"count\":" << statistics.loads().count() << ','

             << "\"latency\":{" << (HardwareInfo::is_amd() ? "\"cache-miss\":" : "\"cache\":")
             << statistics.loads().cache_latency() << ',' << (HardwareInfo::is_amd() ? "\"uop\":" : "\"instruction\":")
             << statistics.loads().instr_latency();
      if (HardwareInfo::is_amd()) {
        stream << ",\"dtlb\":" << statistics.loads().dtlb_latency();
      }
      stream << "}," << "\"cache-hits\":{" << "\"l1d\":" << statistics.loads().count_l1_hits() << ','
             << "\"l2\":" << statistics.loads().count_l2_hits() << ','
             << "\"l3\":" << statistics.loads().count_l3_hits() << ','
             << "\"mhb\":" << statistics.loads().count_mhb_hits() << "},"

             << "\"ram\":{" << "\"local\":" << statistics.loads().count_local_ram_hits() << ','
             << "\"remote\":" << statistics.loads().count_remote_ram_hits() << "},"

             << "\"tlb\":{" << "\"dtlb\":" << statistics.loads().dtlb_hits() << ','
             << "\"stlb\":" << statistics.loads().stlb_hits() << ',' << "\"miss\":" << statistics.loads().stlb_misses()
             << '}' << "},";

      /// Software prefetches
      stream << "\"software-prefetches\": {" << "\"count\":" << statistics.software_prefetches().count() << ','

             << "\"latency\":{" << (HardwareInfo::is_amd() ? "\"cache-miss\":" : "\"cache\":")
             << statistics.software_prefetches().cache_latency() << ','
             << (HardwareInfo::is_amd() ? "\"uop\":" : "\"instruction\":")
             << statistics.software_prefetches().instr_latency();
      if (HardwareInfo::is_amd()) {
        stream << ",\"dtlb\":" << statistics.software_prefetches().dtlb_latency();
      }
      stream << "}," << "\"cache-hits\":{" << "\"l1d\":" << statistics.software_prefetches().count_l1_hits() << ','
             << "\"l2\":" << statistics.software_prefetches().count_l2_hits() << ','
             << "\"l3\":" << statistics.software_prefetches().count_l3_hits() << ','
             << "\"mhb\":" << statistics.software_prefetches().count_mhb_hits() << "},"

             << "\"ram\":{" << "\"local\":" << statistics.software_prefetches().count_local_ram_hits() << ','
             << "\"remote\":" << statistics.software_prefetches().count_remote_ram_hits() << "},"

             << "\"tlb\":{" << "\"dtlb\":" << statistics.software_prefetches().dtlb_hits() << ','
             << "\"stlb\":" << statistics.software_prefetches().stlb_hits() << ','
             << "\"miss\":" << statistics.software_prefetches().stlb_misses() << '}' << "},";

      /// Stores
      stream << "\"stores\": {" << "\"count\":" << statistics.software_prefetches().count() << ','

             << "\"latency\":{" << (HardwareInfo::is_amd() ? "\"uop\":" : "\"instruction\":")
             << statistics.software_prefetches().instr_latency();
      if (HardwareInfo::is_amd()) {
        stream << ",\"cache\":" << statistics.software_prefetches().cache_latency()
               << ",\"dtlb\":" << statistics.software_prefetches().dtlb_latency();
      }
      stream << "}" << "}" << "}";
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

  auto cell = [&stream, delimiter](auto&& v, const bool is_write_delimiter = true) {
    if (is_write_delimiter) {
      stream << delimiter;
    }
    stream << v;
  };

  if (print_header) {
    std::vector<std::string> header = { "name",
                                        "offset",
                                        "size",
                                        "samples",

                                        "loads-count" };

    if (HardwareInfo::is_amd()) {
      header.insert(header.end(), { "loads-latency-cache-miss", "loads-latency-uop", "loads-latency-dtlb" });
    } else {
      header.insert(header.end(), { "loads-latency-cache", "loads-latency-instruction" });
    }

    header.insert(header.end(),
                  { "loads-cache-hits-l1d",
                    "loads-cache-hits-l2",
                    "loads-cache-hits-l3",
                    "loads-cache-hits-mhb",
                    "loads-ram-local",
                    "loads-ram-remote",
                    "loads-tlb-dtlb",
                    "loads-tlb-stlb",
                    "loads-tlb-miss",

                    "software-prefetches-count" });

    if (HardwareInfo::is_amd()) {
      header.insert(header.end(),
                    { "software-prefetches-latency-cache-miss",
                      "software-prefetches-latency-uop",
                      "software-prefetches-latency-dtlb" });
    } else {
      header.insert(header.end(), { "software-prefetches-latency-cache", "software-prefetches-latency-instruction" });
    }

    header.insert(header.end(),
                  { "software-prefetches-cache-hits-l1d",
                    "software-prefetches-cache-hits-l2",
                    "software-prefetches-cache-hits-l3",
                    "software-prefetches-cache-hits-mhb",
                    "software-prefetches-ram-local",
                    "software-prefetches-ram-remote",
                    "software-prefetches-tlb-dtlb",
                    "software-prefetches-tlb-stlb",
                    "software-prefetches-tlb-miss",

                    "stores-count",
                    "stores-latency-instruction" });

    if (HardwareInfo::is_amd()) {
      header.insert(header.end(), { "stores-latency-cache", "stores-latency-dtlb" });
    }

    /* write header row */
    for (auto index = 0U; index < header.size(); ++index) {
      cell(header[index], index > 0U);
    }
    stream << '\n';
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

      /* member */
      cell(member.name(), false);
      cell(member.offset());
      cell(member.size());
      cell(member.samples().size());

      /* loads */
      cell(statistics.loads().count());
      cell(statistics.loads().cache_latency());
      cell(statistics.loads().instr_latency());
      if (HardwareInfo::is_amd()) {
        cell(statistics.loads().dtlb_latency());
      }
      cell(statistics.loads().count_l1_hits());
      cell(statistics.loads().count_l2_hits());
      cell(statistics.loads().count_l3_hits());
      cell(statistics.loads().count_mhb_hits());
      cell(statistics.loads().count_local_ram_hits());
      cell(statistics.loads().count_remote_ram_hits());
      cell(statistics.loads().dtlb_hits());
      cell(statistics.loads().stlb_hits());
      cell(statistics.loads().stlb_misses());

      /* software prefetches */
      cell(statistics.software_prefetches().count());
      cell(statistics.software_prefetches().cache_latency());
      cell(statistics.software_prefetches().instr_latency());
      if (HardwareInfo::is_amd()) {
        cell(statistics.software_prefetches().dtlb_latency());
      }
      cell(statistics.software_prefetches().count_l1_hits());
      cell(statistics.software_prefetches().count_l2_hits());
      cell(statistics.software_prefetches().count_l3_hits());
      cell(statistics.software_prefetches().count_mhb_hits());
      cell(statistics.software_prefetches().count_local_ram_hits());
      cell(statistics.software_prefetches().count_remote_ram_hits());
      cell(statistics.software_prefetches().dtlb_hits());
      cell(statistics.software_prefetches().stlb_hits());
      cell(statistics.software_prefetches().stlb_misses());

      /* stores */
      cell(statistics.stores().count());
      cell(statistics.stores().instr_latency());
      if (HardwareInfo::is_amd()) {
        cell(statistics.stores().cache_latency());
        cell(statistics.stores().dtlb_latency());
      }

      stream << '\n';
    }
  }

  return stream.str();
}