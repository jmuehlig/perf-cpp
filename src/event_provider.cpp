#include <algorithm>
#include <fstream>
#include <linux/perf_event.h>
#include <perfcpp/counter_definition.h>
#include <perfcpp/event_provider.h>
#include <perfcpp/exception.h>
#include <perfcpp/feature.h>
#include <perfcpp/hardware_info.h>
#include <perfcpp/metric.h>
#include <perfcpp/time_event.h>
#include <regex>
#include <sstream>
#include <cctype>
#include <algorithm>

void
perf::PerfSubsystemEventProvider::add_events(perf::CounterDefinition& counter_definition)
{
  counter_definition.add("instructions", PERF_TYPE_HARDWARE, PERF_COUNT_HW_INSTRUCTIONS);

  /// Cycles
  counter_definition.add("cycles", PERF_TYPE_HARDWARE, PERF_COUNT_HW_CPU_CYCLES);
  counter_definition.add("cpu-cycles", PERF_TYPE_HARDWARE, PERF_COUNT_HW_CPU_CYCLES);
  counter_definition.add("bus-cycles", PERF_TYPE_HARDWARE, PERF_COUNT_HW_BUS_CYCLES);

  /// Branches
  counter_definition.add("branches", PERF_TYPE_HARDWARE, PERF_COUNT_HW_BRANCH_INSTRUCTIONS);
  counter_definition.add("branch-instructions", PERF_TYPE_HARDWARE, PERF_COUNT_HW_BRANCH_INSTRUCTIONS);
  counter_definition.add("branch-misses", PERF_TYPE_HARDWARE, PERF_COUNT_HW_BRANCH_MISSES);

  /// Stall events
  counter_definition.add("stalled-cycles-backend", PERF_TYPE_HARDWARE, PERF_COUNT_HW_STALLED_CYCLES_BACKEND);
  counter_definition.add("idle-cycles-backend", PERF_TYPE_HARDWARE, PERF_COUNT_HW_STALLED_CYCLES_BACKEND);
  counter_definition.add("stalled-cycles-frontend", PERF_TYPE_HARDWARE, PERF_COUNT_HW_STALLED_CYCLES_FRONTEND);
  counter_definition.add("idle-cycles-frontend", PERF_TYPE_HARDWARE, PERF_COUNT_HW_STALLED_CYCLES_FRONTEND);

  /// Cache events
  counter_definition.add("cache-misses", PERF_TYPE_HARDWARE, PERF_COUNT_HW_CACHE_MISSES);
  counter_definition.add("cache-references", PERF_TYPE_HARDWARE, PERF_COUNT_HW_CACHE_REFERENCES);
  counter_definition.add("L1-dcache-loads",
                         PERF_TYPE_HW_CACHE,
                         PERF_COUNT_HW_CACHE_L1D | (PERF_COUNT_HW_CACHE_OP_READ << 8) |
                           (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16));
  counter_definition.add("L1-dcache-load-misses",
                         PERF_TYPE_HW_CACHE,
                         PERF_COUNT_HW_CACHE_L1D | (PERF_COUNT_HW_CACHE_OP_READ << 8) |
                           (PERF_COUNT_HW_CACHE_RESULT_MISS << 16));
  counter_definition.add("L1-icache-loads",
                         PERF_TYPE_HW_CACHE,
                         PERF_COUNT_HW_CACHE_L1I | (PERF_COUNT_HW_CACHE_OP_READ << 8) |
                           (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16));
  counter_definition.add("L1-icache-load-misses",
                         PERF_TYPE_HW_CACHE,
                         PERF_COUNT_HW_CACHE_L1I | (PERF_COUNT_HW_CACHE_OP_READ << 8) |
                           (PERF_COUNT_HW_CACHE_RESULT_MISS << 16));

  /// TLB events
  counter_definition.add("dTLB-loads",
                         PERF_TYPE_HW_CACHE,
                         PERF_COUNT_HW_CACHE_DTLB | (PERF_COUNT_HW_CACHE_OP_READ << 8) |
                           (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16));
  counter_definition.add("dTLB-load-misses",
                         PERF_TYPE_HW_CACHE,
                         PERF_COUNT_HW_CACHE_DTLB | (PERF_COUNT_HW_CACHE_OP_READ << 8) |
                           (PERF_COUNT_HW_CACHE_RESULT_MISS << 16));
  counter_definition.add("iTLB-loads",
                         PERF_TYPE_HW_CACHE,
                         PERF_COUNT_HW_CACHE_ITLB | (PERF_COUNT_HW_CACHE_OP_READ << 8) |
                           (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16));
  counter_definition.add("iTLB-load-misses",
                         PERF_TYPE_HW_CACHE,
                         PERF_COUNT_HW_CACHE_ITLB | (PERF_COUNT_HW_CACHE_OP_READ << 8) |
                           (PERF_COUNT_HW_CACHE_RESULT_MISS << 16));

  /// Software events
  counter_definition.add("software", "cpu-clock", PERF_TYPE_SOFTWARE, PERF_COUNT_SW_CPU_CLOCK);
  counter_definition.add("software", "task-clock", PERF_TYPE_SOFTWARE, PERF_COUNT_SW_TASK_CLOCK);
  counter_definition.add("software", "page-faults", PERF_TYPE_SOFTWARE, PERF_COUNT_SW_PAGE_FAULTS);
  counter_definition.add("software", "faults", PERF_TYPE_SOFTWARE, PERF_COUNT_SW_PAGE_FAULTS);
  counter_definition.add("software", "major-faults", PERF_TYPE_SOFTWARE, PERF_COUNT_SW_PAGE_FAULTS_MAJ);
  counter_definition.add("software", "minor-faults", PERF_TYPE_SOFTWARE, PERF_COUNT_SW_PAGE_FAULTS_MIN);
  counter_definition.add("software", "alignment-faults", PERF_TYPE_SOFTWARE, PERF_COUNT_SW_ALIGNMENT_FAULTS);
  counter_definition.add("software", "emulation-faults", PERF_TYPE_SOFTWARE, PERF_COUNT_SW_EMULATION_FAULTS);
  counter_definition.add("software", "context-switches", PERF_TYPE_SOFTWARE, PERF_COUNT_SW_CONTEXT_SWITCHES);
#ifndef PERFCPP_NO_COUNT_SW_BPF_OUTPUT /// PERF_COUNT_SW_BPF_OUTPUT is supported since Linux Kernel 4.4
  counter_definition.add("software", "bpf-output", PERF_TYPE_SOFTWARE, PERF_COUNT_SW_BPF_OUTPUT);
#endif
#ifndef PERFCPP_NO_CGROUP_SWITCHES /// PERF_COUNT_SW_CGROUP_SWITCHES is supported since Linux Kernel 5.13
  counter_definition.add("software", "cgroup-switches", PERF_TYPE_SOFTWARE, PERF_COUNT_SW_CGROUP_SWITCHES);
#endif
  counter_definition.add("software", "cpu-migrations", PERF_TYPE_SOFTWARE, PERF_COUNT_SW_CPU_MIGRATIONS);
  counter_definition.add("software", "migrations", PERF_TYPE_SOFTWARE, PERF_COUNT_SW_CPU_MIGRATIONS);
}

void
perf::TimeEventProvider::add_events(perf::CounterDefinition& counter_definition)
{
  counter_definition.add("seconds", std::make_unique<SecondsTimeEvent>());
  counter_definition.add("s", std::make_unique<SecondsTimeEvent>());
  counter_definition.add("milliseconds", std::make_unique<MillisecondsTimeEvent>());
  counter_definition.add("ms", std::make_unique<MillisecondsTimeEvent>());
  counter_definition.add("microseconds", std::make_unique<MicrosecondsTimeEvent>());
  counter_definition.add("us", std::make_unique<MicrosecondsTimeEvent>());
  counter_definition.add("nanoseconds", std::make_unique<NanosecondsTimeEvent>());
  counter_definition.add("ns", std::make_unique<NanosecondsTimeEvent>());
}

void
perf::MetricEventProvider::add_events(perf::CounterDefinition& counter_definition)
{
  counter_definition.add(std::make_unique<CyclesPerInstruction>());
  counter_definition.add(std::make_unique<Gigahertz>());
  counter_definition.add(std::make_unique<InstructionsPerCycle>());
  counter_definition.add(std::make_unique<CacheHitRatio>());
  counter_definition.add(std::make_unique<CacheMissRatio>());
  counter_definition.add(std::make_unique<DTLBMissRatio>());
  counter_definition.add(std::make_unique<ITLBMissRatio>());
  counter_definition.add(std::make_unique<L1DataMissRatio>());
  counter_definition.add(std::make_unique<BranchMissRatio>());
}

void
perf::SystemSpecificEventProvider::add_events(perf::CounterDefinition& counter_definition)
{
  auto performance_monitoring_units = std::vector<std::pair<std::string, std::string>>{
    { "/sys/bus/event_source/devices/cpu/", "cpu" },                 /// CPU PMU
    { "/sys/bus/event_source/devices/cpu_core/", "cpu" },            /// CPU PMU on heterogeneous Intel architectures
    { "/sys/bus/event_source/devices/cpu_atom/", "cpu-atom" },       /// Atom PMU on heterogeneous Intel architectures
    { "/sys/bus/event_source/devices/cstate_core/", "cstate-core" }, /// CState Core PMU on Intel architectures
    { "/sys/bus/event_source/devices/cstate_pkg/", "cstate-pkg" },   /// CState Pkg PMU on Intel architectures
    { "/sys/bus/event_source/devices/power/", "power" }              /// Power PMU
  };

  /// Identify more dynamic PMUs (e.g., amd-iommu-0, etc).
  SystemSpecificEventProvider::detect_performance_monitoring_units("amd_iommu.*|armv.*", performance_monitoring_units);

  /// Add the events by reading from the filesystem as specified above.
  for (const auto& [path, pmu_name] : performance_monitoring_units) {
    if (std::filesystem::exists(path)) {
      SystemSpecificEventProvider::add_events(counter_definition, pmu_name, path);
    }
  }
}

void
perf::SystemSpecificEventProvider::add_events(perf::CounterDefinition& counter_definition,
                                              const std::string& pmu_name,
                                              const std::string& path)
{
  /// Parse the type for the PMU.
  if (const auto type = SystemSpecificEventProvider::parse_event_file_descriptor_type(path + "type");
      type.has_value()) {
    /// Iterate over all files in the descriptor path.
    for (const auto& file_entry : std::filesystem::directory_iterator(path + "events")) {

      /// Events are only described in files without extension.
      if (file_entry.path().extension() == "") {

        /// Check if a counter with the given filename already exists. If yes, do not add another.
        if (!counter_definition.counter(pmu_name, file_entry.path().filename()).has_value()) {

          /// Parse the file descriptor containing configuration code and further information.
          if (const auto event_configuration =
                SystemSpecificEventProvider::parse_event_file_descriptor_config(file_entry.path());
              event_configuration.has_value()) {

            /// Add the event, if parsing was successfully.
            auto config = CounterConfig{ type.value(),
                                         std::get<0>(event_configuration.value()),
                                         std::get<1>(event_configuration.value()).value_or(0U) };

            /// Try to find and parse a .scale file for the given event. Only a few events (e.g., the power PMU)
            /// provide/need a scale factor.
            if (const auto scale =
                  SystemSpecificEventProvider::parse_event_file_descriptor_scale(file_entry.path().string() + ".scale");
                scale.has_value()) {
              config.scale(scale.value());
            }

            counter_definition.add(std::string{ pmu_name }, file_entry.path().filename(), config);
          }
        }
      }
    }
  }
}

std::optional<std::pair<std::uint64_t, std::optional<std::uint64_t>>>
perf::SystemSpecificEventProvider::parse_event_file_descriptor_config(const std::filesystem::path& path)
{
  auto event_stream = std::ifstream{ path };
  if (event_stream.is_open()) {
    std::string line;
    std::getline(event_stream, line);

    if (line.empty()) {
      return std::nullopt;
    }

    /// Store all entries (A,B) from parsing the line in the format "A=B[,C=D]*", with entries being "event", "umask",
    /// or "ldlat".
    auto entries = std::unordered_map<std::string, std::uint64_t>{};

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

      /// Transform value into integer and add to entries.
      if (!key.empty()) {
        if (const auto integer = SystemSpecificEventProvider::parse_integer(value); integer.has_value()) {
          entries.insert(std::make_pair(std::move(key), integer.value()));
        }
      }
    }

    /// Combine event and umask to a single event id.
    if (const auto event = entries.find("event"); event != entries.end()) {

      /// Fetch event value.
      auto event_value = event->second;

      /// Apply umask, if available.
      if (const auto umask = entries.find("umask"); umask != entries.end()) {
        event_value = (umask->second << 8) | event_value;
      }

      /// Add load latency, if found (only available for mem-load on Intel PEBS).
      if (const auto load_latency = entries.find("ldlat"); load_latency != entries.end()) {
        return std::make_pair(event_value, load_latency->second);
      }

      return std::make_pair(event_value, std::nullopt);
    }

    /// Some AMD IO MMU events are configured via csource instead.
    if (const auto csource = entries.find("csource"); csource != entries.end()) {
      return std::make_pair(csource->second, std::nullopt);
    }
  }

  return std::nullopt;
}

std::optional<std::uint32_t>
perf::SystemSpecificEventProvider::parse_event_file_descriptor_type(std::filesystem::path&& path)
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

std::optional<double>
perf::SystemSpecificEventProvider::parse_event_file_descriptor_scale(std::filesystem::path&& path)
{
  if (!std::filesystem::exists(path)) {
    return std::nullopt;
  }

  auto type_stream = std::ifstream{ path };
  if (type_stream.is_open()) {
    double type;
    type_stream >> type;

    return type;
  }

  return std::nullopt;
}

std::vector<std::pair<std::uint8_t, std::pair<std::uint8_t, std::optional<std::uint8_t>>>>
perf::SystemSpecificEventProvider::parse_event_file_descriptor_format(std::filesystem::path&& path)
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

std::optional<std::uint64_t>
perf::SystemSpecificEventProvider::parse_integer(const std::string& value)
{
  std::string copied_str = value;
  copied_str.erase(std::remove_if(copied_str.begin(), copied_str.end(), [](unsigned char c){return std::isspace(c);}), copied_str.end());

  if (copied_str.empty()) {
    return std::nullopt;
  }

  /// Strings starting with '0x' are considered hex numbers.
  if (copied_str.rfind("0x", 0ULL) == 0ULL) {
    return std::stoull(copied_str.substr(2ULL), nullptr, 16);
  }

  /// Strings containing digits are considered dec numbers.
  if (std::all_of(copied_str.begin(), copied_str.end(), [](const auto c) { return std::isdigit(c); })) {
    return std::stoull(copied_str, nullptr, 0);
  }

  return std::nullopt;
}

void
perf::SystemSpecificEventProvider::detect_performance_monitoring_units(
  std::string&& regex_pattern,
  std::vector<std::pair<std::string, std::string>>& performance_monitoring_units)
{
  const auto pattern = std::regex{ regex_pattern };

  /// Iterate through the directory
  for (const auto& entry : std::filesystem::directory_iterator("/sys/bus/event_source/devices/")) {
    if (entry.is_directory()) {
      /// Check if folder name matches the regex pattern
      if (const auto subfolder = entry.path().filename().string(); std::regex_match(subfolder, pattern)) {

        /// If yes, replace all underscores by dashes.
        auto pmu_name = subfolder;
        std::replace(pmu_name.begin(), pmu_name.end(), '_', '-');

        performance_monitoring_units.emplace_back(entry.path().string().append("/"), std::move(pmu_name));
      }
    }
  }
}

void
perf::AMDIbsEventProvider::add_events(perf::CounterDefinition& counter_definition)
{
  /// AMD's Instruction Based Sampling differs in configuration (and utilization) from Intel PEBS with specific PMUs for
  /// sampling. Whenever an AMD CPU is detected, IBS PMUs will be added.
  if (HardwareInfo::is_amd_ibs_supported()) {
    AMDIbsEventProvider::add_fetch_events(counter_definition);
    AMDIbsEventProvider::add_op_events(counter_definition);
  }
}

void
perf::AMDIbsEventProvider::add_fetch_events(perf::CounterDefinition& counter_definition)
{
  if (const auto ibs_fetch_type =
        SystemSpecificEventProvider::parse_event_file_descriptor_type("/sys/bus/event_source/devices/ibs_fetch/type");
      ibs_fetch_type.has_value()) {
    if (const auto ibs_fetch_bit_format = SystemSpecificEventProvider::parse_event_file_descriptor_format(
          "/sys/bus/event_source/devices/ibs_fetch/format/rand_en");
        ibs_fetch_bit_format.size() == 1UL) {
      const auto ibs_fetch_bit = std::get<0U>(std::get<1U>(ibs_fetch_bit_format.front()));

      /// Event that is triggered by cycles.
      counter_definition.add("ibs_fetch", "ibs_fetch", CounterConfig{ ibs_fetch_type.value(), 1ULL << ibs_fetch_bit });

      if (HardwareInfo::is_ibs_l3_filter_supported()) {
        if (const auto ibs_fetch_l3miss_bit_format = SystemSpecificEventProvider::parse_event_file_descriptor_format(
              "/sys/bus/event_source/devices/ibs_fetch/format/l3missonly");
            ibs_fetch_l3miss_bit_format.size() == 1UL) {
          const auto ibs_fetch_l3miss_bit = std::get<0U>(std::get<1U>(ibs_fetch_l3miss_bit_format.front()));

          /// Event that is triggered by cycles and applies the L3 miss filter.
          counter_definition.add(
            "ibs_fetch",
            "ibs_fetch_l3missonly",
            CounterConfig{ ibs_fetch_type.value(), (1ULL << ibs_fetch_bit) | (1ULL << ibs_fetch_l3miss_bit) });
        }
      }
    }
  }
}

void
perf::AMDIbsEventProvider::add_op_events(perf::CounterDefinition& counter_definition)
{
  if (const auto ibs_op_type =
        SystemSpecificEventProvider::parse_event_file_descriptor_type("/sys/bus/event_source/devices/ibs_op/type");
      ibs_op_type.has_value()) {
    /// Event that is triggered by cycles.
    counter_definition.add("ibs_op", "ibs_op", CounterConfig{ ibs_op_type.value(), 0U });

    /// Event that is triggered by uops.
    auto ibs_op_uops_bit = std::optional<std::uint8_t>{ std::nullopt };
    if (const auto ibs_uops_bit_format = SystemSpecificEventProvider::parse_event_file_descriptor_format(
          "/sys/bus/event_source/devices/ibs_op/format/cnt_ctl");
        ibs_uops_bit_format.size() == 1UL) {
      ibs_op_uops_bit = std::get<0U>(std::get<1U>(ibs_uops_bit_format.front()));
    }

    if (ibs_op_uops_bit.has_value()) {
      counter_definition.add(
        "ibs_op", "ibs_op_uops", CounterConfig{ ibs_op_type.value(), 1ULL << ibs_op_uops_bit.value() });
    }

    /// Cycle and uops events with L3 miss filter.
    if (HardwareInfo::is_ibs_l3_filter_supported()) {
      if (const auto ibs_op_l3miss_bit_format = SystemSpecificEventProvider::parse_event_file_descriptor_format(
            "/sys/bus/event_source/devices/ibs_op/format/l3missonly");
          ibs_op_l3miss_bit_format.size() == 1UL) {
        const auto ibs_op_l3miss_bit = std::get<0U>(std::get<1U>(ibs_op_l3miss_bit_format.front()));

        /// Event that is triggered by cycles and applies the L3 miss only filter.
        counter_definition.add(
          "ibs_op", "ibs_op_l3missonly", CounterConfig{ ibs_op_type.value(), 1ULL << ibs_op_l3miss_bit });

        /// Event that is triggered by uops and applies the L3 miss only filter.
        if (ibs_op_uops_bit.has_value()) {
          counter_definition.add(
            "ibs_op",
            "ibs_op_uops_l3missonly",
            CounterConfig{ ibs_op_type.value(), (1ULL << ibs_op_uops_bit.value()) | (1ULL << ibs_op_l3miss_bit) });
        }
      }
    }
  }
}

void
perf::CsvFileEventProvider::add_events(perf::CounterDefinition& counter_definition)
{
  /// Read all counter values from the config file in the format
  ///     name,<config>[,<extended config>,<type>]
  /// where <config> and <extended config> are either integer or hex values.

  auto input_file = std::ifstream{ this->_file_name };
  if (!input_file.is_open()) {
    throw CannotOpenFileError{ this->_file_name };
  }

  std::string line;
  while (std::getline(input_file, line)) {
    /// Skip lines that start with '#' and are considered a comment.
    if (const auto is_comment = !line.empty() && line.front() == '#'; is_comment) {
      continue;
    }

    /// Lines containing a ',' (separator) are considered an event or an event.
    if (const auto is_event_line = line.find(',') != std::string::npos; is_event_line) {
      auto line_stream = std::istringstream{ line };

      std::string name;
      
      /// Read name.
      if (std::getline(line_stream, name, ','); !name.empty()) {

        auto extended_config = std::optional<std::uint64_t>{ std::nullopt };
        auto type = std::optional<std::uint32_t>{ std::nullopt };

        /// Read config-field and translate into integer.
        if (std::string config_or_metric_str; std::getline(line_stream, config_or_metric_str, ',')) {

          /// Try to translate config into number.
          if (const auto config = SystemSpecificEventProvider::parse_integer(config_or_metric_str);
              config.has_value()) {
            /// Read extended config-field and translate into integer.
            if (std::string extended_config_str; std::getline(line_stream, extended_config_str, ',')) {
              /// Translate extended config into number.
              extended_config = SystemSpecificEventProvider::parse_integer(extended_config_str);

              /// Read type-field and translate into integer.
              if (std::string type_str; std::getline(line_stream, type_str, ',')) {
                /// Translate type into number.
                type = SystemSpecificEventProvider::parse_integer(type_str);
              }
            }

            /// Add counter configuration.
            counter_definition.add(
              std::move(name),
              CounterConfig{ type.value_or(PERF_TYPE_RAW), config.value(), extended_config.value_or(0ULL) });
          }

          /// Try to translate config into metric.
          else if (!config_or_metric_str.empty()) {
            auto metric = std::make_unique<FormulaMetric>(std::move(name), std::move(config_or_metric_str));
            counter_definition.add(std::move(metric));
          }
        }
      }
    }
  }
}