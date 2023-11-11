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

std::optional<perf::Counter> perf::CounterDefinition::get(const std::string &name) const noexcept
{
    if (auto iterator = this->_counter_configs.find(name); iterator != this->_counter_configs.end())
    {
        return std::make_optional(Counter{iterator->first, iterator->second});
    }

    return std::nullopt;
}

void perf::CounterDefinition::initialized_default_counters()
{
    this->_counter_configs.reserve(128U);

    this->_counter_configs.insert(std::make_pair("instructions", CounterConfig{PERF_TYPE_HARDWARE, PERF_COUNT_HW_INSTRUCTIONS}));
    this->_counter_configs.insert(std::make_pair("cycles", CounterConfig{PERF_TYPE_HARDWARE, PERF_COUNT_HW_CPU_CYCLES}));
    this->_counter_configs.insert(std::make_pair("cpu-cycles", CounterConfig{PERF_TYPE_HARDWARE, PERF_COUNT_HW_CPU_CYCLES}));
    this->_counter_configs.insert(std::make_pair("bus-cycles", CounterConfig{PERF_TYPE_HARDWARE, PERF_COUNT_HW_BUS_CYCLES}));

    this->_counter_configs.insert(std::make_pair("cache-misses", CounterConfig{PERF_TYPE_HARDWARE, PERF_COUNT_HW_CACHE_MISSES}));
    this->_counter_configs.insert(std::make_pair("cache-references", CounterConfig{PERF_TYPE_HARDWARE, PERF_COUNT_HW_CACHE_REFERENCES}));

    this->_counter_configs.insert(std::make_pair("branches", CounterConfig{PERF_TYPE_HARDWARE, PERF_COUNT_HW_BRANCH_INSTRUCTIONS}));
    this->_counter_configs.insert(std::make_pair("branch-instructions", CounterConfig{PERF_TYPE_HARDWARE, PERF_COUNT_HW_BRANCH_INSTRUCTIONS}));
    this->_counter_configs.insert(std::make_pair("branch-misses", CounterConfig{PERF_TYPE_HARDWARE, PERF_COUNT_HW_BRANCH_MISSES}));

    this->_counter_configs.insert(std::make_pair("stalled-cycles-backend", CounterConfig{PERF_TYPE_HARDWARE, PERF_COUNT_HW_STALLED_CYCLES_BACKEND}));
    this->_counter_configs.insert(std::make_pair("idle-cycles-backend", CounterConfig{PERF_TYPE_HARDWARE, PERF_COUNT_HW_STALLED_CYCLES_BACKEND}));
    this->_counter_configs.insert(std::make_pair("stalled-cycles-frontend", CounterConfig{PERF_TYPE_HARDWARE, PERF_COUNT_HW_STALLED_CYCLES_FRONTEND}));
    this->_counter_configs.insert(std::make_pair("idle-cycles-frontend", CounterConfig{PERF_TYPE_HARDWARE, PERF_COUNT_HW_STALLED_CYCLES_FRONTEND}));

    this->_counter_configs.insert(std::make_pair("cpu-clock", CounterConfig{PERF_TYPE_SOFTWARE, PERF_COUNT_SW_CPU_CLOCK}));
    this->_counter_configs.insert(std::make_pair("task-clock", CounterConfig{PERF_TYPE_SOFTWARE, PERF_COUNT_SW_TASK_CLOCK}));
    this->_counter_configs.insert(std::make_pair("page-faults", CounterConfig{PERF_TYPE_SOFTWARE, PERF_COUNT_SW_PAGE_FAULTS}));
    this->_counter_configs.insert(std::make_pair("faults", CounterConfig{PERF_TYPE_SOFTWARE, PERF_COUNT_SW_PAGE_FAULTS}));
    this->_counter_configs.insert(std::make_pair("major-faults", CounterConfig{PERF_TYPE_SOFTWARE, PERF_COUNT_SW_PAGE_FAULTS_MAJ}));
    this->_counter_configs.insert(std::make_pair("minor-faults", CounterConfig{PERF_TYPE_SOFTWARE, PERF_COUNT_SW_PAGE_FAULTS_MIN}));
    this->_counter_configs.insert(std::make_pair("alignment-faults", CounterConfig{PERF_TYPE_SOFTWARE, PERF_COUNT_SW_ALIGNMENT_FAULTS}));
    this->_counter_configs.insert(std::make_pair("emulation-faults", CounterConfig{PERF_TYPE_SOFTWARE, PERF_COUNT_SW_EMULATION_FAULTS}));
    this->_counter_configs.insert(std::make_pair("context-switches", CounterConfig{PERF_TYPE_SOFTWARE, PERF_COUNT_SW_CONTEXT_SWITCHES}));
    this->_counter_configs.insert(std::make_pair("bpf-output", CounterConfig{PERF_TYPE_SOFTWARE, PERF_COUNT_SW_BPF_OUTPUT}));
    this->_counter_configs.insert(std::make_pair("cgroup-switches", CounterConfig{PERF_TYPE_SOFTWARE, PERF_COUNT_SW_CGROUP_SWITCHES}));
    this->_counter_configs.insert(std::make_pair("cpu-migrations", CounterConfig{PERF_TYPE_SOFTWARE, PERF_COUNT_SW_CPU_MIGRATIONS}));
    this->_counter_configs.insert(std::make_pair("migrations", CounterConfig{PERF_TYPE_SOFTWARE, PERF_COUNT_SW_CPU_MIGRATIONS}));
}

#include <iostream>
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