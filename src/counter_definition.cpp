#include <perfcpp/counter_definition.h>

#include <cpuid.h>
#include <fstream>
#include <sstream>
#include <string_view>
#include <utility>

perf::CounterDefinition::CounterDefinition(const std::string& config_file)
{
  this->initialize_generalized_counters();
  this->initialize_amd_ibs_counters();
  this->initialize_intel_pebs_counters();

  this->read_counter_configuration(config_file);
}

perf::CounterDefinition::CounterDefinition()
{
  this->initialize_generalized_counters();
  this->initialize_amd_ibs_counters();
  this->initialize_intel_pebs_counters();
}

std::optional<std::pair<std::string_view, perf::CounterConfig>>
perf::CounterDefinition::counter(const std::string& name) const noexcept
{
  if (auto iterator = this->_counter_configs.find(name); iterator != this->_counter_configs.end()) {
    return std::make_optional(std::make_pair(std::string_view(iterator->first), iterator->second));
  }

  return std::nullopt;
}

std::optional<std::pair<std::string_view, perf::Metric&>>
perf::CounterDefinition::metric(const std::string& name) const noexcept
{
  if (auto iterator = _metrics.find(name); iterator != _metrics.end()) {
    return std::make_optional(std::make_pair(std::string_view(iterator->first), std::ref(*iterator->second)));
  }

  return std::nullopt;
}

void
perf::CounterDefinition::initialize_generalized_counters()
{
  this->_counter_configs.reserve(128U);
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
  this->add("bpf-output", PERF_TYPE_SOFTWARE, PERF_COUNT_SW_BPF_OUTPUT);
#ifndef NO_PERF_COUNT_SW_CGROUP_SWITCHES /// PERF_COUNT_SW_CGROUP_SWITCHES is provided since Linux Kernel 5.13
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
  this->add(std::make_unique<CacheHitRatio>());
  this->add(std::make_unique<DTLBMissRatio>());
  this->add(std::make_unique<ITLBMissRatio>());
  this->add(std::make_unique<L1DataMissRatio>());
}

void
perf::CounterDefinition::initialize_amd_ibs_counters()
{
  /// AMD specific counter
  if (__builtin_cpu_is("amd") > 0) {
    /// See https://github.com/jlgreathouse/AMD_IBS_Toolkit/blob/master/ibs_with_perf_events.txt

    std::uint32_t eax, ebx, ecx, edx;

    /// Read CPUID
    if (__get_cpuid_count(0x80000001, 0, &eax, &ebx, &ecx, &edx)) {
      const auto is_ibs_supported = static_cast<bool>(ecx & (std::uint32_t(1U) << 10U));

      /// Check if L3Miss filtering is provided.
      auto is_ibs_l3miss_filter_supported = false;
      if (__get_cpuid_count(0x8000001b, 0, &eax, &ebx, &ecx, &edx)) {
        is_ibs_l3miss_filter_supported = static_cast<bool>(eax & (std::uint32_t(1U) << 11U));
      }

      if (is_ibs_supported) {
        /// IBS OP.
        {
          auto ibs_op_stream = std::ifstream{ "/sys/bus/event_source/devices/ibs_op/type" };
          if (ibs_op_stream.is_open()) {
            std::uint32_t type;
            ibs_op_stream >> type;
            this->add("ibs_op", CounterConfig{ type, 0U });
            this->add("ibs_op_uops", CounterConfig{ type, 1ULL << 19U });

            if (is_ibs_l3miss_filter_supported) {
              this->add("ibs_op_l3missonly", CounterConfig{ type, 1ULL << 16U });
              this->add("ibs_op_uops_l3missonly", CounterConfig{ type, (1ULL << 19U) | (1ULL << 16U) });
            }
          }
        }

        /// IBS Fetch.
        {
          auto ibs_op_stream = std::ifstream{ "/sys/bus/event_source/devices/ibs_fetch/type" };
          if (ibs_op_stream.is_open()) {
            std::uint32_t type;
            ibs_op_stream >> type;
            this->add("ibs_fetch", CounterConfig{ type, 1ULL << 57U });

            if (is_ibs_l3miss_filter_supported) {
              this->add("ibs_fetch_l3missonly", CounterConfig{ type, (1ULL << 57U) | (1ULL << 16U) });
            }
          }
        }
      }
    }
  }
}

void
perf::CounterDefinition::initialize_intel_pebs_counters()
{
  if (__builtin_cpu_is("intel") > 0) {
    /// Auxiliary event, needed on some Intel architectures (starting from Sapphire Rapids).
    this->add("mem-loads-aux", PERF_TYPE_RAW, 0x8203);
  }
}

void
perf::CounterDefinition::read_counter_configuration(const std::string& csv_filename)
{
  /// Read all counter values from the config file in the format
  ///     name,<config>[,<extended config>,<type>]
  /// where <config> and <extended config> are either integer or hex values.

  auto input_file = std::ifstream{ csv_filename };
  if (input_file.is_open()) {
    std::string line;
    while (std::getline(input_file, line)) {
      auto line_stream = std::istringstream{ line };

      std::string name;
      std::uint64_t config;
      auto extended_config = 0ULL;
      auto type = std::uint32_t{ PERF_TYPE_RAW };
      if (std::getline(line_stream, name, ',')) {
        std::string config_str;
        if (std::getline(line_stream, config_str, ',')) {
          config = std::stoull(config_str, nullptr, 0);

          std::string extended_config_str;
          if (std::getline(line_stream, extended_config_str, ',')) {
            extended_config = std::stoull(extended_config_str, nullptr, 0);

            std::string type_str;
            if (std::getline(line_stream, type_str, ',')) {
              type = std::stoul(type_str, nullptr, 0);
            }
          }

          if (!name.empty()) {
            this->_counter_configs.insert(
              std::make_pair(std::move(name), CounterConfig{ type, config, extended_config }));
          }
        }
      }
    }
  }
}