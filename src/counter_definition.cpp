#include <perfcpp/counter_definition.h>

#include <fstream>
#include <sstream>

perf::CounterDefinition::CounterDefinition(const std::string& config_file)
{
  this->initialized_default_counters();
  this->read_counter_configuration(config_file);
}

perf::CounterDefinition::CounterDefinition()
{
  this->initialized_default_counters();
}

std::optional<perf::CounterConfig>
perf::CounterDefinition::counter(const std::string& name) const noexcept
{
  if (auto iterator = this->_counter_configs.find(name); iterator != this->_counter_configs.end()) {
    return std::make_optional(iterator->second);
  }

  return std::nullopt;
}

void
perf::CounterDefinition::initialized_default_counters()
{
  this->_counter_configs.reserve(128U);
  this->_metrics.reserve(64U);

  /// Pre-defined counters.
  this->add("instructions", CounterConfig{ PERF_TYPE_HARDWARE, PERF_COUNT_HW_INSTRUCTIONS });

  this->add("cycles", CounterConfig{ PERF_TYPE_HARDWARE, PERF_COUNT_HW_CPU_CYCLES });
  this->add("cpu-cycles", CounterConfig{ PERF_TYPE_HARDWARE, PERF_COUNT_HW_CPU_CYCLES });
  this->add("bus-cycles", CounterConfig{ PERF_TYPE_HARDWARE, PERF_COUNT_HW_BUS_CYCLES });

  this->add("cache-misses", CounterConfig{ PERF_TYPE_HARDWARE, PERF_COUNT_HW_CACHE_MISSES });
  this->add("cache-references", CounterConfig{ PERF_TYPE_HARDWARE, PERF_COUNT_HW_CACHE_REFERENCES });

  this->add("branches", CounterConfig{ PERF_TYPE_HARDWARE, PERF_COUNT_HW_BRANCH_INSTRUCTIONS });
  this->add("branch-instructions", CounterConfig{ PERF_TYPE_HARDWARE, PERF_COUNT_HW_BRANCH_INSTRUCTIONS });
  this->add("branch-misses", CounterConfig{ PERF_TYPE_HARDWARE, PERF_COUNT_HW_BRANCH_MISSES });

  this->add("stalled-cycles-backend", CounterConfig{ PERF_TYPE_HARDWARE, PERF_COUNT_HW_STALLED_CYCLES_BACKEND });
  this->add("idle-cycles-backend", CounterConfig{ PERF_TYPE_HARDWARE, PERF_COUNT_HW_STALLED_CYCLES_BACKEND });
  this->add("stalled-cycles-frontend", CounterConfig{ PERF_TYPE_HARDWARE, PERF_COUNT_HW_STALLED_CYCLES_FRONTEND });
  this->add("idle-cycles-frontend", CounterConfig{ PERF_TYPE_HARDWARE, PERF_COUNT_HW_STALLED_CYCLES_FRONTEND });

  this->add("cpu-clock", CounterConfig{ PERF_TYPE_SOFTWARE, PERF_COUNT_SW_CPU_CLOCK });
  this->add("task-clock", CounterConfig{ PERF_TYPE_SOFTWARE, PERF_COUNT_SW_TASK_CLOCK });
  this->add("page-faults", CounterConfig{ PERF_TYPE_SOFTWARE, PERF_COUNT_SW_PAGE_FAULTS });
  this->add("faults", CounterConfig{ PERF_TYPE_SOFTWARE, PERF_COUNT_SW_PAGE_FAULTS });
  this->add("major-faults", CounterConfig{ PERF_TYPE_SOFTWARE, PERF_COUNT_SW_PAGE_FAULTS_MAJ });
  this->add("minor-faults", CounterConfig{ PERF_TYPE_SOFTWARE, PERF_COUNT_SW_PAGE_FAULTS_MIN });
  this->add("alignment-faults", CounterConfig{ PERF_TYPE_SOFTWARE, PERF_COUNT_SW_ALIGNMENT_FAULTS });
  this->add("emulation-faults", CounterConfig{ PERF_TYPE_SOFTWARE, PERF_COUNT_SW_EMULATION_FAULTS });
  this->add("context-switches", CounterConfig{ PERF_TYPE_SOFTWARE, PERF_COUNT_SW_CONTEXT_SWITCHES });
  this->add("bpf-output", CounterConfig{ PERF_TYPE_SOFTWARE, PERF_COUNT_SW_BPF_OUTPUT });
#ifndef NO_PERF_COUNT_SW_CGROUP_SWITCHES /// PERF_COUNT_SW_CGROUP_SWITCHES is provided since Linux Kernel 5.13
  this->add("cgroup-switches", CounterConfig{ PERF_TYPE_SOFTWARE, PERF_COUNT_SW_CGROUP_SWITCHES });
#endif
  this->add("cpu-migrations", CounterConfig{ PERF_TYPE_SOFTWARE, PERF_COUNT_SW_CPU_MIGRATIONS });
  this->add("migrations", CounterConfig{ PERF_TYPE_SOFTWARE, PERF_COUNT_SW_CPU_MIGRATIONS });

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

  /// Auxiliary event, needed on some Intel architectures (starting from Sapphire Rapids).
  this->add("mem-loads-aux", PERF_TYPE_RAW, 0x8203);

  /// Pre-defined metrics.
  this->add(std::make_unique<CyclesPerInstruction>());
  this->add(std::make_unique<CacheHitRatio>());
  this->add(std::make_unique<DTLBMissRatio>());
  this->add(std::make_unique<ITLBMissRatio>());
  this->add(std::make_unique<L1DataMissRatio>());
}

void
perf::CounterDefinition::read_counter_configuration(const std::string& config_file)
{
  /// Read all counter values from the config file in the format
  ///     name,<config>[,<extended config>,<type>]
  /// where <config> and <extended config> are either integer or hex values.

  auto input_file = std::ifstream{ config_file };
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