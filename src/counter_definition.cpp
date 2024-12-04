#include <fstream>
#include <linux/perf_event.h>
#include <perfcpp/counter_definition.h>
#include <perfcpp/exception.h>
#include <perfcpp/feature.h>
#include <perfcpp/hardware_info.h>
#include <sstream>
#include <string_view>
#include <utility>

perf::CounterDefinition::CounterDefinition()
{
  this->initialize_generalized_counters();
  this->initialize_amd_ibs_counters();
  this->initialize_intel_pebs_counters();
  this->initialize_time_events();
}

perf::CounterDefinition::CounterDefinition(const std::string& config_file)
  : CounterDefinition()
{
  this->read_counter_configuration(config_file);
}

std::optional<std::pair<std::string_view, perf::CounterConfig>>
perf::CounterDefinition::counter(const std::string& name) const noexcept
{
  if (auto iterator = this->_hardware_counter_configurations.find(name);
      iterator != this->_hardware_counter_configurations.end()) {
    return std::make_optional(std::make_pair(std::string_view(iterator->first), iterator->second));
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
perf::CounterDefinition::initialize_generalized_counters()
{
  this->_hardware_counter_configurations.reserve(128U);
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
perf::CounterDefinition::initialize_amd_ibs_counters()
{
  /// IBS OP.
  if (const auto ibs_op_type = HardwareInfo::amd_ibs_op_type(); ibs_op_type.has_value()) {
    this->add("ibs_op", CounterConfig{ ibs_op_type.value(), 0U });

    /// Read the bit to sample for micro ops from the perf format.
    auto uops_bit = HardwareInfo::amd_ibs_op_bit();
    if (uops_bit.has_value()) {
      this->add("ibs_op_uops", CounterConfig{ ibs_op_type.value(), 1ULL << uops_bit.value() });
    }

    if (HardwareInfo::is_ibs_l3_filter_supported()) {

      /// Read the bit to filter for l3 misses from the perf format.
      if (const auto l3missonly_bit = HardwareInfo::amd_ibs_op_l3miss_bit(); l3missonly_bit.has_value()) {
        this->add("ibs_op_l3missonly", CounterConfig{ ibs_op_type.value(), 1ULL << l3missonly_bit.value() });

        if (uops_bit.has_value()) {
          this->add(
            "ibs_op_uops_l3missonly",
            CounterConfig{ ibs_op_type.value(), (1ULL << uops_bit.value()) | (1ULL << l3missonly_bit.value()) });
        }
      }
    }
  }

  /// IBS Fetch.
  if (const auto ibs_fetch_type = HardwareInfo::amd_ibs_fetch_type(); ibs_fetch_type.has_value()) {

    /// Read the bit to sample for instruction fetches from the perf format.
    if (const auto fetch_bit = HardwareInfo::amd_ibs_fetch_bit(); fetch_bit.has_value()) {
      this->add("ibs_fetch", CounterConfig{ ibs_fetch_type.value(), 1ULL << fetch_bit.value() });

      if (HardwareInfo::is_ibs_l3_filter_supported()) {
        /// Read the bit to filter for l3 misses from the perf format.
        if (const auto l3missonly_bit = HardwareInfo::amd_ibs_fetch_l3miss_bit(); l3missonly_bit.has_value()) {
          this->add(
            "ibs_fetch_l3missonly",
            CounterConfig{ ibs_fetch_type.value(), (1ULL << fetch_bit.value()) | (1ULL << l3missonly_bit.value()) });
        }
      }
    }
  }
}

void
perf::CounterDefinition::initialize_intel_pebs_counters()
{
  if (HardwareInfo::is_intel()) {
    if (HardwareInfo::is_intel_aux_counter_required()) {
      /// Auxiliary event, needed on some Intel architectures.
      if (const auto mem_loads_aux_event_id = HardwareInfo::intel_pebs_mem_loads_aux_event_id();
          mem_loads_aux_event_id.has_value()) {
        this->add("mem-loads-aux", PERF_TYPE_RAW, mem_loads_aux_event_id.value());
      }
    }

    /// mem-loads event.
    if (const auto mem_loads_event_id = HardwareInfo::intel_pebs_mem_loads_event_id(); mem_loads_event_id.has_value()) {
      this->add("mem-loads", PERF_TYPE_RAW, mem_loads_event_id.value());
    }

    /// mem-loads event.
    if (const auto mem_stores_event_id = HardwareInfo::intel_pebs_mem_stores_event_id();
        mem_stores_event_id.has_value()) {
      this->add("mem-stores", PERF_TYPE_RAW, mem_stores_event_id.value());
    }
  }
}

void
perf::CounterDefinition::initialize_time_events()
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