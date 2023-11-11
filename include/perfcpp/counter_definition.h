#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include "counter.h"

namespace perf
{
class CounterDefinition
{
public:
    explicit CounterDefinition(const std::string& config_file);
    explicit CounterDefinition(std::string&& config_file) : CounterDefinition(config_file) { }
    CounterDefinition();

    ~CounterDefinition() = default;

    void add(std::string&& name, const std::uint32_t type, const std::uint64_t event_id)
    {
        add(std::move(name), CounterConfig{type, event_id});
    }

    void add(std::string&& name, CounterConfig config)
    {
        _counter_configs.insert(std::make_pair(std::move(name), config));
    }

    std::optional<Counter> get(std::string&& name) const noexcept { return get(name); }
    std::optional<Counter> get(const std::string& name) const noexcept;

private:
    std::unordered_map<std::string, CounterConfig> _counter_configs;

    void initialized_default_counters();
    void read_counter_configs(const std::string& config_file);
};
}