#include <fstream>
#include <linux/perf_event.h>
#include <perfcpp/counter_definition.h>
#include <perfcpp/exception.h>
#include <perfcpp/feature.h>
#include <perfcpp/hardware_info.h>
#include <regex>
#include <sstream>
#include <string_view>
#include <utility>

perf::CounterDefinition::CounterDefinition()
{
  /// Add the events specified by the perf subsystem header.
  this->add_general_events_from_perf_subsystem();

  /// Read events from the "CPU" PMU, if available. This will also include Intel PEBS events.
  if (std::filesystem::exists("/sys/bus/event_source/devices/cpu/")) {
    this->add_events_from_descriptor_files("cpu", "/sys/bus/event_source/devices/cpu/");
  }

  /// Read events from the "CPU-Core" PMU, if available – but declare as "CPU" PMU (this makes things more simple as
  /// "CPU" PMU is available on most CPUs). This will also include Intel PEBS events.
  if (std::filesystem::exists("/sys/bus/event_source/devices/cpu_core/")) {
    this->add_events_from_descriptor_files("cpu", "/sys/bus/event_source/devices/cpu_core/");
  }

  /// Read events from the "CPU-Atom" PMU, if available. This will also include Intel PEBS events.
  if (std::filesystem::exists("/sys/bus/event_source/devices/cpu_atom/")) {
    this->add_events_from_descriptor_files("cpu-atom", "/sys/bus/event_source/devices/cpu_atom/");
  }

  /// AMD's Instruction Based Sampling differs in configuration (and utilization) from Intel PEBS with specific PMUs for
  /// sampling. Whenever an AMD CPU is detected, IBS PMUs will be added.
  if (HardwareInfo::is_amd_ibs_supported()) {
    this->add_amd_ibs_events();
  }

  /// Add time events, i.e., virtual counters to include time measurements.
  this->add_virtual_time_events();

  /// Add metrics based on the previous added events.
  this->add_metrics();
}

perf::CounterDefinition::CounterDefinition(const std::string& config_file)
  : CounterDefinition()
{
  this->read_counter_configuration(config_file);
}
void
perf::CounterDefinition::add(std::string&& pmu_name, std::string&& event_name, const perf::CounterConfig config)
{
  /// When the PMU (identified by the name) already exists, add the event.
  if (auto pmu_iterator = this->_performance_monitoring_unit_events.find(pmu_name);
      pmu_iterator != this->_performance_monitoring_unit_events.end()) {
    auto& event_config_map = pmu_iterator->second;
    event_config_map.insert_or_assign(std::move(event_name), config);
  }

  /// Otherwise reserve some space for upcoming events for that PMU and add that map.
  else {
    auto event_config_map = std::unordered_map<std::string, perf::CounterConfig>{};
    event_config_map.reserve(64U);

    event_config_map.insert(std::make_pair(std::move(event_name), config));

    this->_performance_monitoring_unit_events.insert(std::make_pair(std::move(pmu_name), std::move(event_config_map)));
  }
}

std::vector<std::tuple<std::string_view, std::string_view, perf::CounterConfig>>
perf::CounterDefinition::counter(const std::string& name) const noexcept
{
  auto event_configurations = std::vector<std::tuple<std::string_view, std::string_view, perf::CounterConfig>>{};
  event_configurations.reserve(this->_performance_monitoring_unit_events.size());

  /// Scan all PMUs...
  for (const auto& [pmu_name, events] : this->_performance_monitoring_unit_events) {
    /// ... for an event with that requested name.
    if (auto iterator = events.find(name); iterator != events.end()) {
      event_configurations.emplace_back(
        std::string_view(pmu_name), std::string_view(iterator->first), iterator->second);
    }
  }

  return event_configurations;
}

std::optional<std::tuple<std::string_view, std::string_view, perf::CounterConfig>>
perf::CounterDefinition::counter(const std::string& pmu_name, const std::string& event_name) const noexcept
{
  /// Find all events of the PMU.
  if (auto pmu_iterator = this->_performance_monitoring_unit_events.find(pmu_name);
      pmu_iterator != this->_performance_monitoring_unit_events.end()) {
    const auto& pmu_events = pmu_iterator->second;

    /// Find the event in the PMU event list.
    if (auto event_iterator = pmu_events.find(event_name); event_iterator != pmu_events.end()) {
      return std::make_optional(std::make_tuple(
        std::string_view{ pmu_iterator->first }, std::string_view{ event_iterator->first }, event_iterator->second));
    }
  }

  return std::nullopt;
}

std::optional<std::pair<std::string_view, perf::Metric&>>
perf::CounterDefinition::metric(const std::string& name) const noexcept
{
  if (auto iterator = this->_metrics.find(name); iterator != this->_metrics.end()) {
    return std::make_optional(std::make_pair(std::string_view(iterator->first), std::ref(*iterator->second)));
  }

  return std::nullopt;
}

std::optional<std::pair<std::string_view, perf::TimeEvent&>>
perf::CounterDefinition::time_event(const std::string& name) const noexcept
{
  if (auto iterator = this->_time_events.find(name); iterator != this->_time_events.end()) {
    return std::make_optional(std::make_pair(std::string_view(iterator->first), std::ref(*iterator->second)));
  }

  return std::nullopt;
}

void
perf::CounterDefinition::add_general_events_from_perf_subsystem()
{
  this->_performance_monitoring_unit_events.reserve(128U);
  this->_metrics.reserve(64U);

  this->add("instructions", PERF_TYPE_HARDWARE, PERF_COUNT_HW_INSTRUCTIONS);

  /// Cycles
  this->add("cycles", PERF_TYPE_HARDWARE, PERF_COUNT_HW_CPU_CYCLES);
  this->add("cpu-cycles", PERF_TYPE_HARDWARE, PERF_COUNT_HW_CPU_CYCLES);
  this->add("bus-cycles", PERF_TYPE_HARDWARE, PERF_COUNT_HW_BUS_CYCLES);

  /// Branches
  this->add("branches", PERF_TYPE_HARDWARE, PERF_COUNT_HW_BRANCH_INSTRUCTIONS);
  this->add("branch-instructions", PERF_TYPE_HARDWARE, PERF_COUNT_HW_BRANCH_INSTRUCTIONS);
  this->add("branch-misses", PERF_TYPE_HARDWARE, PERF_COUNT_HW_BRANCH_MISSES);

  /// Stall events
  this->add("stalled-cycles-backend", PERF_TYPE_HARDWARE, PERF_COUNT_HW_STALLED_CYCLES_BACKEND);
  this->add("idle-cycles-backend", PERF_TYPE_HARDWARE, PERF_COUNT_HW_STALLED_CYCLES_BACKEND);
  this->add("stalled-cycles-frontend", PERF_TYPE_HARDWARE, PERF_COUNT_HW_STALLED_CYCLES_FRONTEND);
  this->add("idle-cycles-frontend", PERF_TYPE_HARDWARE, PERF_COUNT_HW_STALLED_CYCLES_FRONTEND);

  /// Software events
  this->add("cpu-clock", PERF_TYPE_SOFTWARE, PERF_COUNT_SW_CPU_CLOCK);
  this->add("task-clock", PERF_TYPE_SOFTWARE, PERF_COUNT_SW_TASK_CLOCK);
  this->add("page-faults", PERF_TYPE_SOFTWARE, PERF_COUNT_SW_PAGE_FAULTS);
  this->add("faults", PERF_TYPE_SOFTWARE, PERF_COUNT_SW_PAGE_FAULTS);
  this->add("major-faults", PERF_TYPE_SOFTWARE, PERF_COUNT_SW_PAGE_FAULTS_MAJ);
  this->add("minor-faults", PERF_TYPE_SOFTWARE, PERF_COUNT_SW_PAGE_FAULTS_MIN);
  this->add("alignment-faults", PERF_TYPE_SOFTWARE, PERF_COUNT_SW_ALIGNMENT_FAULTS);
  this->add("emulation-faults", PERF_TYPE_SOFTWARE, PERF_COUNT_SW_EMULATION_FAULTS);
  this->add("context-switches", PERF_TYPE_SOFTWARE, PERF_COUNT_SW_CONTEXT_SWITCHES);
#ifndef PERFCPP_NO_COUNT_SW_BPF_OUTPUT /// PERF_COUNT_SW_BPF_OUTPUT is supported since Linux Kernel 4.4
  this->add("bpf-output", PERF_TYPE_SOFTWARE, PERF_COUNT_SW_BPF_OUTPUT);
#endif
#ifndef PERFCPP_NO_CGROUP_SWITCHES /// PERF_COUNT_SW_CGROUP_SWITCHES is supported since Linux Kernel 5.13
  this->add("cgroup-switches", PERF_TYPE_SOFTWARE, PERF_COUNT_SW_CGROUP_SWITCHES);
#endif
  this->add("cpu-migrations", PERF_TYPE_SOFTWARE, PERF_COUNT_SW_CPU_MIGRATIONS);
  this->add("migrations", PERF_TYPE_SOFTWARE, PERF_COUNT_SW_CPU_MIGRATIONS);

  /// Cache events
  this->add("cache-misses", PERF_TYPE_HARDWARE, PERF_COUNT_HW_CACHE_MISSES);
  this->add("cache-references", PERF_TYPE_HARDWARE, PERF_COUNT_HW_CACHE_REFERENCES);
  this->add("L1-dcache-loads",
            PERF_TYPE_HW_CACHE,
            PERF_COUNT_HW_CACHE_L1D | (PERF_COUNT_HW_CACHE_OP_READ << 8) | (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16));
  this->add("L1-dcache-load-misses",
            PERF_TYPE_HW_CACHE,
            PERF_COUNT_HW_CACHE_L1D | (PERF_COUNT_HW_CACHE_OP_READ << 8) | (PERF_COUNT_HW_CACHE_RESULT_MISS << 16));
  this->add("L1-icache-loads",
            PERF_TYPE_HW_CACHE,
            PERF_COUNT_HW_CACHE_L1I | (PERF_COUNT_HW_CACHE_OP_READ << 8) | (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16));
  this->add("L1-icache-load-misses",
            PERF_TYPE_HW_CACHE,
            PERF_COUNT_HW_CACHE_L1I | (PERF_COUNT_HW_CACHE_OP_READ << 8) | (PERF_COUNT_HW_CACHE_RESULT_MISS << 16));

  /// TLB events
  this->add("dTLB-loads",
            PERF_TYPE_HW_CACHE,
            PERF_COUNT_HW_CACHE_DTLB | (PERF_COUNT_HW_CACHE_OP_READ << 8) | (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16));
  this->add("dTLB-load-misses",
            PERF_TYPE_HW_CACHE,
            PERF_COUNT_HW_CACHE_DTLB | (PERF_COUNT_HW_CACHE_OP_READ << 8) | (PERF_COUNT_HW_CACHE_RESULT_MISS << 16));
  this->add("iTLB-loads",
            PERF_TYPE_HW_CACHE,
            PERF_COUNT_HW_CACHE_ITLB | (PERF_COUNT_HW_CACHE_OP_READ << 8) | (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16));
  this->add("iTLB-load-misses",
            PERF_TYPE_HW_CACHE,
            PERF_COUNT_HW_CACHE_ITLB | (PERF_COUNT_HW_CACHE_OP_READ << 8) | (PERF_COUNT_HW_CACHE_RESULT_MISS << 16));
}

void
perf::CounterDefinition::add_events_from_descriptor_files(std::string&& pmu_name, std::string&& path)
{
  /// Parse the type for the PMU.
  if (auto type = CounterDefinition::parse_event_file_descriptor_type(path + "type"); type.has_value()) {
    /// Iterate over all files in the descriptor path.
    for (auto& file_descriptor : std::filesystem::directory_iterator(path + "events")) {

      /// Check if a counter with the given filename already exists. If yes, do not add another.
      if (!this->counter(pmu_name, file_descriptor.path().filename()).has_value()) {

        /// Parse the file descriptor containing configuration code and further information.
        if (const auto event_configuration =
              CounterDefinition::parse_event_file_descriptor_config(file_descriptor.path());
            event_configuration.has_value()) {

          /// Add the event, if parsing was successfully.
          auto config = CounterConfig{ type.value(),
                                       std::get<0>(event_configuration.value()),
                                       std::get<1>(event_configuration.value()).value_or(0U) };
          this->add(std::string{ pmu_name }, file_descriptor.path().filename(), config);
        }
      }
    }
  }
}

void
perf::CounterDefinition::add_amd_ibs_events()
{
  if (HardwareInfo::is_amd_ibs_supported()) {
    /// Check if the hardware supports filtering samples that miss the L3 cache.
    const auto is_support_l3miss_filter = HardwareInfo::is_ibs_l3_filter_supported();

    /// Add ibs_op PMU.
    if (const auto ibs_op_type =
          CounterDefinition::parse_event_file_descriptor_type("/sys/bus/event_source/devices/ibs_op/type");
        ibs_op_type.has_value()) {
      /// Event that is triggered by cycles.
      this->add("ibs_op", "ibs_op", CounterConfig{ ibs_op_type.value(), 0U });

      /// Event that is triggered by uops.
      auto ibs_op_uops_bit = std::optional<std::uint8_t>{ std::nullopt };
      if (const auto ibs_uops_bit_format = CounterDefinition::parse_event_file_descriptor_format(
            "/sys/bus/event_source/devices/ibs_op/format/cnt_ctl");
          ibs_uops_bit_format.size() == 1UL) {
        ibs_op_uops_bit = std::get<0U>(std::get<1U>(ibs_uops_bit_format.front()));
      }

      if (ibs_op_uops_bit.has_value()) {
        this->add("ibs_op", "ibs_op_uops", CounterConfig{ ibs_op_type.value(), 1ULL << ibs_op_uops_bit.value() });
      }

      /// Cycle and uops events with L3 miss filter.
      if (is_support_l3miss_filter) {
        if (const auto ibs_op_l3miss_bit_format = CounterDefinition::parse_event_file_descriptor_format(
              "/sys/bus/event_source/devices/ibs_op/format/l3missonly");
            ibs_op_l3miss_bit_format.size() == 1UL) {
          const auto ibs_op_l3miss_bit = std::get<0U>(std::get<1U>(ibs_op_l3miss_bit_format.front()));

          /// Event that is triggered by cycles and applies the L3 miss only filter.
          this->add("ibs_op", "ibs_op_l3missonly", CounterConfig{ ibs_op_type.value(), 1ULL << ibs_op_l3miss_bit });

          /// Event that is triggered by uops and applies the L3 miss only filter.
          if (ibs_op_uops_bit.has_value()) {
            this->add(
              "ibs_op",
              "ibs_op_uops_l3missonly",
              CounterConfig{ ibs_op_type.value(), (1ULL << ibs_op_uops_bit.value()) | (1ULL << ibs_op_l3miss_bit) });
          }
        }
      }
    }

    /// Add the ibs_fetch PMU.
    if (const auto ibs_fetch_type =
          CounterDefinition::parse_event_file_descriptor_type("/sys/bus/event_source/devices/ibs_fetch/type");
        ibs_fetch_type.has_value()) {
      if (const auto ibs_fetch_bit_format = CounterDefinition::parse_event_file_descriptor_format(
            "/sys/bus/event_source/devices/ibs_fetch/format/rand_en");
          ibs_fetch_bit_format.size() == 1UL) {
        const auto ibs_fetch_bit = std::get<0U>(std::get<1U>(ibs_fetch_bit_format.front()));

        /// Event that is triggered by cycles.
        this->add("ibs_fetch", "ibs_fetch", CounterConfig{ ibs_fetch_type.value(), 1ULL << ibs_fetch_bit });

        if (is_support_l3miss_filter) {
          if (const auto ibs_fetch_l3miss_bit_format = CounterDefinition::parse_event_file_descriptor_format(
                "/sys/bus/event_source/devices/ibs_fetch/format/l3missonly");
              ibs_fetch_l3miss_bit_format.size() == 1UL) {
            const auto ibs_fetch_l3miss_bit = std::get<0U>(std::get<1U>(ibs_fetch_l3miss_bit_format.front()));

            /// Event that is triggered by cycles and applies the L3 miss filter.
            this->add(
              "ibs_fetch",
              "ibs_fetch_l3missonly",
              CounterConfig{ ibs_fetch_type.value(), (1ULL << ibs_fetch_bit) | (1ULL << ibs_fetch_l3miss_bit) });
          }
        }
      }
    }
  }
}

void
perf::CounterDefinition::add_virtual_time_events()
{
  this->add("seconds", std::make_unique<SecondsTimeEvent>());
  this->add("s", std::make_unique<SecondsTimeEvent>());
  this->add("milliseconds", std::make_unique<MillisecondsTimeEvent>());
  this->add("ms", std::make_unique<MillisecondsTimeEvent>());
  this->add("microseconds", std::make_unique<MicrosecondsTimeEvent>());
  this->add("us", std::make_unique<MicrosecondsTimeEvent>());
  this->add("nanoseconds", std::make_unique<NanosecondsTimeEvent>());
  this->add("ns", std::make_unique<NanosecondsTimeEvent>());
}

void
perf::CounterDefinition::add_metrics()
{
  /// Pre-defined metrics.
  this->add(std::make_unique<CyclesPerInstruction>());
  this->add(std::make_unique<Gigahertz>());
  this->add(std::make_unique<InstructionsPerCycle>());
  this->add(std::make_unique<CacheHitRatio>());
  this->add(std::make_unique<CacheMissRatio>());
  this->add(std::make_unique<DTLBMissRatio>());
  this->add(std::make_unique<ITLBMissRatio>());
  this->add(std::make_unique<L1DataMissRatio>());
  this->add(std::make_unique<BranchMissRatio>());
}

void
perf::CounterDefinition::read_counter_configuration(const std::string& csv_filename)
{
  /// Read all counter values from the config file in the format
  ///     name,<config>[,<extended config>,<type>]
  /// where <config> and <extended config> are either integer or hex values.

  auto input_file = std::ifstream{ csv_filename };
  if (!input_file.is_open()) {
    throw CannotOpenFileError{ csv_filename };
  }

  std::string line;
  while (std::getline(input_file, line)) {
    auto line_stream = std::istringstream{ line };

    std::string name;
    std::uint64_t config;
    auto extended_config = 0ULL;
    auto type = std::uint32_t{ PERF_TYPE_RAW };

    /// Read name.
    if (std::getline(line_stream, name, ','); !name.empty()) {

      /// Read config-field and translate into integer.
      std::string config_str;
      if (std::getline(line_stream, config_str, ',')) {
        if (config_str.rfind("0x", 0ULL) == 0ULL) {
          config = std::stoull(config_str.substr(2ULL), nullptr, 16);
        } else {
          config = std::stoull(config_str, nullptr, 0);
        }

        /// Read extended config-field and translate into integer.
        std::string extended_config_str;
        if (std::getline(line_stream, extended_config_str, ',')) {
          if (extended_config_str.rfind("0x", 0ULL) == 0ULL) {
            extended_config = std::stoull(extended_config_str.substr(2ULL), nullptr, 16);
          } else {
            extended_config = std::stoull(extended_config_str, nullptr, 0);
          }

          /// Read type-field and translate into integer.
          std::string type_str;
          if (std::getline(line_stream, type_str, ',')) {
            if (type_str.rfind("0x", 0ULL) == 0ULL) {
              type = std::uint32_t(std::stoul(type_str.substr(2ULL), nullptr, 16));
            } else {
              type = std::uint32_t(std::stoul(extended_config_str, nullptr, 0));
            }
          }
        }

        /// Add counter configuration.
        this->add(std::move(name), CounterConfig{ type, config, extended_config });
      }
    }
  }
}

std::optional<std::pair<std::uint64_t, std::optional<std::uint64_t>>>
perf::CounterDefinition::parse_event_file_descriptor_config(const std::filesystem::path& path)
{
  auto event_stream = std::ifstream{ path };
  if (event_stream.is_open()) {
    std::string line;
    std::getline(event_stream, line);

    if (!line.empty()) {
      /// The line should look like "event=0xcd,umask=0x1[,ldlat=3]".

      auto event = std::optional<std::string>{ std::nullopt };
      auto umask = std::optional<std::string>{ std::nullopt };
      auto ldlat = std::optional<std::string>{ std::nullopt };

      auto token_stream = std::stringstream{ line };
      std::string token;

      /// Process every token where tokens are separated by ','.
      while (std::getline(token_stream, token, ',')) {

        /// Locate eq-char.
        const auto pos = token.find('=');
        if (pos == std::string::npos) {
          continue;
        }

        auto key = token.substr(0ULL, pos);
        auto value = token.substr(pos + 1ULL);

        /// Remove possible whitespace
        key.erase(std::remove_if(key.begin(), key.end(), ::isspace), key.end());
        value.erase(std::remove_if(value.begin(), value.end(), ::isspace), value.end());

        /// Convert key to lowercase for case-insensitivity
        std::transform(key.begin(), key.end(), key.begin(), ::tolower);

        /// Remove the values "0x" prefix.
        if (value.rfind("0x", 0ULL) == 0ULL) {
          value = value.substr(2ULL);
        }

        if (key == "event") {
          event = std::move(value);
        } else if (key == "umask") {
          umask = std::move(value);
        } else if (key == "ldlat") {
          ldlat = std::move(value);
        }
      }

      /// Combine event and umask to a single event id.
      if (event.has_value() && umask.has_value()) {
        const auto event_configuration =
          std::stoull(/* combine <umask><event> */ umask.value().append(event.value()), nullptr, 16);

        if (ldlat.has_value()) {
          return std::make_pair(event_configuration, std::stoull(ldlat.value()));
        }

        return std::make_pair(event_configuration, std::nullopt);
      }
    }
  }

  return std::nullopt;
}

std::optional<std::uint32_t>
perf::CounterDefinition::parse_event_file_descriptor_type(std::filesystem::path&& path)
{
  if (!std::filesystem::exists(path)) {
    return std::nullopt;
  }

  auto type_stream = std::ifstream{ path };
  if (type_stream.is_open()) {
    std::uint32_t type;
    type_stream >> type;

    return type;
  }

  return std::nullopt;
}

std::vector<std::pair<std::uint8_t, std::pair<std::uint8_t, std::optional<std::uint8_t>>>>
perf::CounterDefinition::parse_event_file_descriptor_format(std::filesystem::path&& path)
{
  auto configs = std::vector<std::pair<std::uint8_t, std::pair<std::uint8_t, std::optional<std::uint8_t>>>>{};
  if (!std::filesystem::exists(path)) {
    return configs;
  }

  auto format_file = std::ifstream{ path };

  if (!format_file.is_open()) {
    return configs;
  }

  std::string line;
  if (std::getline(format_file, line); !line.empty()) {
    auto config_pattern = std::regex("config([0-9]?):(\\d+)(?:-(\\d+))?");

    auto stream = std::stringstream{ line };
    std::string entry;

    while (std::getline(stream, entry, ',')) {
      if (std::smatch match; std::regex_match(entry, match, config_pattern)) {
        const auto config_id = match[1U].length() == 0U ? 0 : std::stoi(match[1U].str());
        const auto bit_start = std::stoi(match[2U].str());
        const auto bit_end = match[3U].length() == 0U ? std::nullopt : std::make_optional(std::stoi(match[2U].str()));

        configs.emplace_back(config_id, std::make_pair(bit_start, bit_end));
      }
    }
  }

  return configs;
}