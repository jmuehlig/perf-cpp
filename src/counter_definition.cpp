#include <perfcpp/counter_definition.h>

#include <sstream>
#include <fstream>

perf::CounterDefinition::CounterDefinition(const std::string &config_file)
{
    this->initialized_default_counters();
    this->read_counter_configs(config_file);
}

perf::CounterDefinition::CounterDefinition()
{
    this->initialized_default_counters();
}

std::optional<perf::CounterConfig> perf::CounterDefinition::counter(const std::string &name) const noexcept
{
    if (auto iterator = this->_counter_configs.find(name); iterator != this->_counter_configs.end())
    {
        return std::make_optional( iterator->second);
    }

    return std::nullopt;
}

void perf::CounterDefinition::initialized_default_counters()
{
    this->_counter_configs.reserve(128U);
    this->_metrics.reserve(64U);

    /// Pre-defined counters.
    this->add("instructions", CounterConfig{PERF_TYPE_HARDWARE, PERF_COUNT_HW_INSTRUCTIONS});

    this->add("cycles", CounterConfig{PERF_TYPE_HARDWARE, PERF_COUNT_HW_CPU_CYCLES});
    this->add("cpu-cycles", CounterConfig{PERF_TYPE_HARDWARE, PERF_COUNT_HW_CPU_CYCLES});
    this->add("bus-cycles", CounterConfig{PERF_TYPE_HARDWARE, PERF_COUNT_HW_BUS_CYCLES});

    this->add("cache-misses", CounterConfig{PERF_TYPE_HARDWARE, PERF_COUNT_HW_CACHE_MISSES});
    this->add("cache-references", CounterConfig{PERF_TYPE_HARDWARE, PERF_COUNT_HW_CACHE_REFERENCES});

    this->add("branches", CounterConfig{PERF_TYPE_HARDWARE, PERF_COUNT_HW_BRANCH_INSTRUCTIONS});
    this->add("branch-instructions", CounterConfig{PERF_TYPE_HARDWARE, PERF_COUNT_HW_BRANCH_INSTRUCTIONS});
    this->add("branch-misses", CounterConfig{PERF_TYPE_HARDWARE, PERF_COUNT_HW_BRANCH_MISSES});

    this->add("stalled-cycles-backend", CounterConfig{PERF_TYPE_HARDWARE, PERF_COUNT_HW_STALLED_CYCLES_BACKEND});
    this->add("idle-cycles-backend", CounterConfig{PERF_TYPE_HARDWARE, PERF_COUNT_HW_STALLED_CYCLES_BACKEND});
    this->add("stalled-cycles-frontend", CounterConfig{PERF_TYPE_HARDWARE, PERF_COUNT_HW_STALLED_CYCLES_FRONTEND});
    this->add("idle-cycles-frontend", CounterConfig{PERF_TYPE_HARDWARE, PERF_COUNT_HW_STALLED_CYCLES_FRONTEND});

    this->add("cpu-clock", CounterConfig{PERF_TYPE_SOFTWARE, PERF_COUNT_SW_CPU_CLOCK});
    this->add("task-clock", CounterConfig{PERF_TYPE_SOFTWARE, PERF_COUNT_SW_TASK_CLOCK});
    this->add("page-faults", CounterConfig{PERF_TYPE_SOFTWARE, PERF_COUNT_SW_PAGE_FAULTS});
    this->add("faults", CounterConfig{PERF_TYPE_SOFTWARE, PERF_COUNT_SW_PAGE_FAULTS});
    this->add("major-faults", CounterConfig{PERF_TYPE_SOFTWARE, PERF_COUNT_SW_PAGE_FAULTS_MAJ});
    this->add("minor-faults", CounterConfig{PERF_TYPE_SOFTWARE, PERF_COUNT_SW_PAGE_FAULTS_MIN});
    this->add("alignment-faults", CounterConfig{PERF_TYPE_SOFTWARE, PERF_COUNT_SW_ALIGNMENT_FAULTS});
    this->add("emulation-faults", CounterConfig{PERF_TYPE_SOFTWARE, PERF_COUNT_SW_EMULATION_FAULTS});
    this->add("context-switches", CounterConfig{PERF_TYPE_SOFTWARE, PERF_COUNT_SW_CONTEXT_SWITCHES});
    this->add("bpf-output", CounterConfig{PERF_TYPE_SOFTWARE, PERF_COUNT_SW_BPF_OUTPUT});
    this->add("cgroup-switches", CounterConfig{PERF_TYPE_SOFTWARE, PERF_COUNT_SW_CGROUP_SWITCHES});
    this->add("cpu-migrations", CounterConfig{PERF_TYPE_SOFTWARE, PERF_COUNT_SW_CPU_MIGRATIONS});
    this->add("migrations", CounterConfig{PERF_TYPE_SOFTWARE, PERF_COUNT_SW_CPU_MIGRATIONS});

    this->add("L1-dcache-loads", PERF_TYPE_HW_CACHE, PERF_COUNT_HW_CACHE_L1D | (PERF_COUNT_HW_CACHE_OP_READ << 8) | (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16));
    this->add("L1-dcache-load-misses", PERF_TYPE_HW_CACHE, PERF_COUNT_HW_CACHE_L1D | (PERF_COUNT_HW_CACHE_OP_READ << 8) | (PERF_COUNT_HW_CACHE_RESULT_MISS << 16));
    this->add("L1-icache-loads", PERF_TYPE_HW_CACHE, PERF_COUNT_HW_CACHE_L1I | (PERF_COUNT_HW_CACHE_OP_READ << 8) | (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16));
    this->add("L1-icache-load-misses", PERF_TYPE_HW_CACHE, PERF_COUNT_HW_CACHE_L1I | (PERF_COUNT_HW_CACHE_OP_READ << 8) | (PERF_COUNT_HW_CACHE_RESULT_MISS << 16));
    this->add("dTLB-loads", PERF_TYPE_HW_CACHE, PERF_COUNT_HW_CACHE_DTLB | (PERF_COUNT_HW_CACHE_OP_READ << 8) | (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16));
    this->add("dTLB-load-misses", PERF_TYPE_HW_CACHE, PERF_COUNT_HW_CACHE_DTLB | (PERF_COUNT_HW_CACHE_OP_READ << 8) | (PERF_COUNT_HW_CACHE_RESULT_MISS << 16));
    this->add("iTLB-loads", PERF_TYPE_HW_CACHE, PERF_COUNT_HW_CACHE_ITLB | (PERF_COUNT_HW_CACHE_OP_READ << 8) | (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16));
    this->add("iTLB-load-misses", PERF_TYPE_HW_CACHE, PERF_COUNT_HW_CACHE_ITLB | (PERF_COUNT_HW_CACHE_OP_READ << 8) | (PERF_COUNT_HW_CACHE_RESULT_MISS << 16));

    /// Pre-defined metrics.
    this->add(std::make_unique<CyclesPerInstruction>());
    this->add(std::make_unique<CacheHitRatio>());
    this->add(std::make_unique<DTLBMissRatio>());
    this->add(std::make_unique<ITLBMissRatio>());
    this->add(std::make_unique<L1DataMissRatio>());
}

void perf::CounterDefinition::read_counter_configs(const std::string &config_file)
{
    auto input_file = std::ifstream{config_file};
    if (input_file.is_open())
    {
        std::string line;
        while(std::getline(input_file, line))
        {
            auto line_stream = std::istringstream {line};

            std::string value;
            if (std::getline(line_stream, value, ','))
            {
                auto counter_name = std::move(value);
                if (std::getline(line_stream, value, ','))
                {
                    auto counter_raw_value = std::move(value);
                    std::uint64_t counter_event_id;

                    if (counter_raw_value.substr(0U, 2U) == "0x")
                    {
                        auto hex_stream = std::istringstream {counter_raw_value.substr(2U)};
                        hex_stream >> std::hex >> counter_event_id;
                    }
                    else
                    {
                        counter_event_id = std::stoull(counter_raw_value);
                    }

                    if (!counter_name.empty() && counter_event_id > 0U)
                    {
                        this->_counter_configs.insert(std::make_pair(std::move(counter_name), CounterConfig{PERF_TYPE_RAW, counter_event_id}));
                    }
                }
            }
        }
    }
}