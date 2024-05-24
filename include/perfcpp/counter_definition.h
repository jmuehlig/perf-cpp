#pragma once

#include "counter.h"
#include "metric.h"
#include <algorithm>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

namespace perf {
class CounterDefinition
{
public:
  explicit CounterDefinition(const std::string& config_file);
  explicit CounterDefinition(std::string&& config_file)
    : CounterDefinition(config_file)
  {
  }
  CounterDefinition();

  CounterDefinition(CounterDefinition&&) noexcept = default;
  CounterDefinition& operator=(CounterDefinition&&) noexcept = default;

  ~CounterDefinition() = default;

  void add(std::string&& name, const std::uint32_t type, const std::uint64_t event_id)
  {
    add(std::move(name), CounterConfig{ type, event_id });
  }

  void add(std::string&& name, CounterConfig config)
  {
    _counter_configs.insert(std::make_pair(std::move(name), config));
  }

  void add(std::string&& name, std::unique_ptr<Metric>&& metric)
  {
    _metrics.insert(std::make_pair(std::move(name), std::move(metric)));
  }

  void add(std::unique_ptr<Metric>&& metric) { _metrics.insert(std::make_pair(metric->name(), std::move(metric))); }

  [[nodiscard]] std::optional<CounterConfig> counter(std::string&& name) const noexcept { return counter(name); }
  [[nodiscard]] std::optional<CounterConfig> counter(const std::string& name) const noexcept;
  [[nodiscard]] bool is_metric(const std::string& name) const noexcept { return _metrics.find(name) != _metrics.end(); }
  [[nodiscard]] Metric* metric(const std::string& name) const noexcept
  {
    if (auto iterator = _metrics.find(name); iterator != _metrics.end()) {
      return iterator->second.get();
    }

    return nullptr;
  }

  [[nodiscard]] std::vector<std::string> names() const
  {
    auto names = std::vector<std::string>{};
    std::transform(_counter_configs.begin(), _counter_configs.end(), std::back_inserter(names), [](const auto& config) {
      return config.first;
    });
    return names;
  }

  void read_counter_configuration(const std::string& config_file);

private:
  std::unordered_map<std::string, CounterConfig> _counter_configs;
  std::unordered_map<std::string, std::unique_ptr<Metric>> _metrics;

  void initialized_default_counters();
};
}