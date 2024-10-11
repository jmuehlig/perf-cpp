#include <algorithm>
#include <numeric>
#include <perfcpp/perf.h>
#include <stdexcept>
bool
perf::EventCounter::add(std::string&& counter_name)
{
  /// "Close" the current group and add new counters to the next group (if possible).
  if (counter_name.empty()) {
    if (this->_groups.empty() || this->_groups.back().empty()) {
      return true;
    }

    if (this->_groups.size() < this->_config.max_groups()) {
      this->_groups.emplace_back();
      return true;
    }

    return false;
  }

  /// Try to add the counter, if the name is a counter.
  auto counter_config = this->_counter_definitions.counter(counter_name);
  if (counter_config.has_value()) {
    this->add(std::get<0>(counter_config.value()), std::get<1>(counter_config.value()), false);
    return true;
  }

  /// Try to add the metric, if the name is a metric.
  auto metric = this->_counter_definitions.metric(counter_name);
  if (metric.has_value()) {
    /// Add all required counters.
    for (auto&& dependent_counter_name : std::get<1>(metric.value()).required_counter_names()) {
      auto dependent_counter_config = this->_counter_definitions.counter(dependent_counter_name);
      if (dependent_counter_config.has_value()) {
        this->add(std::get<0>(dependent_counter_config.value()), std::get<1>(dependent_counter_config.value()), true);
      } else {
        throw std::runtime_error{ std::string{ "Cannot find counter '" }
                                    .append(dependent_counter_name)
                                    .append("' for metric '")
                                    .append(counter_name)
                                    .append("'.") };
      }
    }

    this->_counters.emplace_back(std::get<0>(metric.value()));
    return true;
  }

  throw std::runtime_error{
    std::string{ "Cannot find counter or metric with name '" }.append(counter_name).append("'.")
  };
}

void
perf::EventCounter::add(std::string_view counter_name, perf::CounterConfig counter, const bool is_hidden)
{
  /// If the counter is already added,
  if (auto iterator = std::find_if(this->_counters.begin(),
                                   this->_counters.end(),
                                   [&counter_name](const auto& counter) { return counter.name() == counter_name; });
      iterator != this->_counters.end()) {
    iterator->is_hidden(iterator->is_hidden() && is_hidden);
    return;
  }

  /// Check if space for more counters left.
  if (this->_groups.size() == this->_config.max_groups() &&
      this->_groups.back().size() >= this->_config.max_counters_per_group()) {
    throw std::runtime_error{ "No more space for counters left." };
  }

  /// Add a new group, if needed (no one available or last is full).
  if (this->_groups.empty() || this->_groups.back().size() >= this->_config.max_counters_per_group()) {
    this->_groups.emplace_back();
  }

  const auto group_id = std::uint8_t(this->_groups.size()) - 1U;
  const auto in_group_id = std::uint8_t(this->_groups.back().size());
  this->_counters.emplace_back(counter_name, is_hidden, group_id, in_group_id);
  this->_groups.back().add(counter);
}

bool
perf::EventCounter::add(std::vector<std::string>&& counter_names)
{
  auto is_all_added = true;

  for (auto& name : counter_names) {
    is_all_added &= this->add(std::move(name));
  }

  return is_all_added;
}

bool
perf::EventCounter::add(const std::vector<std::string>& counter_names)
{
  return this->add(std::vector<std::string>(counter_names));
}

bool
perf::EventCounter::start()
{
  auto is_every_counter_started = true;

  /// Open the counters.
  for (auto& group : this->_groups) {
    is_every_counter_started &= group.open(this->_config);
  }

  /// Start the counters.
  if (is_every_counter_started) {
    for (auto& group : this->_groups) {
      is_every_counter_started &= group.start();
    }
  }

  return is_every_counter_started;
}

void
perf::EventCounter::stop()
{
  /// Stop the counters.
  for (auto& group : this->_groups) {
    std::ignore = group.stop();
  }

  /// Close the counters.
  for (auto& group : this->_groups) {
    group.close();
  }
}

perf::CounterResult
perf::EventCounter::result(std::uint64_t normalization) const
{
  /// Build result with all counters, including hidden ones.
  auto temporary_result = std::vector<std::pair<std::string_view, double>>{};
  temporary_result.reserve(this->_counters.size());

  for (const auto& event : this->_counters) {
    if (event.is_counter()) {
      const auto value = this->_groups[event.group_id()].get(event.in_group_id()) / double(normalization);
      temporary_result.emplace_back(event.name(), value);
    }
  }

  /// Calculate metrics and copy not-hidden counters.
  auto counter_result = CounterResult{ std::move(temporary_result) };
  auto result = std::vector<std::pair<std::string_view, double>>{};
  result.reserve(this->_counters.size());

  for (const auto& event : this->_counters) {
    /// Add all not hidden counters...
    if (event.is_counter()) {
      if (!event.is_hidden()) {
        const auto value = counter_result.get(event.name());
        if (value.has_value()) {
          result.emplace_back(event.name(), value.value());
        }
      }
    }

    /// ... and all metrics.
    else {
      auto metric = this->_counter_definitions.metric(event.name());
      if (metric.has_value()) {
        const auto value = std::get<1>(metric.value()).calculate(counter_result);
        if (value.has_value()) {
          result.emplace_back(std::get<0>(metric.value()), value.value());
        }
      }
    }
  }

  return CounterResult{ std::move(result) };
}

bool
perf::MultiEventCounterBase::add(std::vector<EventCounter>& event_counter, std::string&& counter_name)
{
  for (auto i = 0U; i < event_counter.size() - 1U; ++i) {
    if (!event_counter[i].add(std::string{ counter_name })) {
      return false;
    }
  }

  return event_counter.back().add(std::move(counter_name));
}

bool
perf::MultiEventCounterBase::add(std::vector<EventCounter>& event_counter, std::vector<std::string>&& counter_names)
{
  for (auto i = 0U; i < event_counter.size() - 1U; ++i) {
    if (!event_counter[i].add(std::vector<std::string>{ counter_names })) {
      return false;
    }
  }

  return event_counter.back().add(std::move(counter_names));
}

bool
perf::MultiEventCounterBase::add(std::vector<EventCounter>& event_counter,
                                 const std::vector<std::string>& counter_names)
{
  for (auto& thread_local_counter : event_counter) {
    if (!thread_local_counter.add(counter_names)) {
      return false;
    }
  }

  return true;
}

perf::CounterResult
perf::MultiEventCounterBase::result(const std::vector<perf::EventCounter>& event_counters,
                                    const std::uint64_t normalization)
{
  /// Build result with all counters, including hidden ones.
  const auto& main_perf = event_counters.front();
  auto temporary_result = std::vector<std::pair<std::string_view, double>>{};
  temporary_result.reserve(main_perf._counters.size());

  for (const auto& event : main_perf._counters) {
    if (event.is_counter()) {
      auto value = .0;
      for (const auto& thread_local_counter : event_counters) {
        value += thread_local_counter._groups[event.group_id()].get(event.in_group_id());
      }
      const auto normalized_value = value / double(normalization);
      temporary_result.emplace_back(event.name(), normalized_value);
    }
  }

  /// Calculate metrics and copy not-hidden counters.
  auto counter_result = CounterResult{ std::move(temporary_result) };
  auto result = std::vector<std::pair<std::string_view, double>>{};
  result.reserve(main_perf._counters.size());

  for (const auto& event : main_perf._counters) {
    /// Add all not hidden counters...
    if (event.is_counter()) {
      if (!event.is_hidden()) {
        const auto value = counter_result.get(event.name());
        if (value.has_value()) {
          result.emplace_back(event.name(), value.value());
        }
      }
    }

    /// ... and all metrics.
    else {
      auto metric = main_perf._counter_definitions.metric(event.name());
      if (metric.has_value()) {
        auto value = std::get<1>(metric.value()).calculate(counter_result);
        if (value.has_value()) {
          result.emplace_back(std::get<0>(metric.value()), value.value());
        }
      }
    }
  }

  return CounterResult{ std::move(result) };
}

perf::MultiThreadEventCounter::MultiThreadEventCounter(const perf::CounterDefinition& counter_list,
                                                       const std::uint16_t num_threads,
                                                       const perf::Config config)
{
  this->_thread_local_counter.reserve(num_threads);
  for (auto i = 0U; i < num_threads; ++i) {
    this->_thread_local_counter.emplace_back(counter_list, config);
  }
}

perf::MultiThreadEventCounter::MultiThreadEventCounter(perf::EventCounter&& event_counter,
                                                       const std::uint16_t num_threads)
{
  this->_thread_local_counter.reserve(num_threads);
  for (auto i = 0U; i < num_threads - 1U; ++i) {
    this->_thread_local_counter.push_back(event_counter);
  }
  this->_thread_local_counter.emplace_back(std::move(event_counter));
}

perf::MultiProcessEventCounter::MultiProcessEventCounter(const perf::CounterDefinition& counter_list,
                                                         std::vector<pid_t>&& process_ids,
                                                         perf::Config config)
{
  this->_process_local_counter.reserve(process_ids.size());

  for (const auto process_id : process_ids) {
    config.process_id(process_id);
    this->_process_local_counter.emplace_back(counter_list, config);
  }
}

perf::MultiProcessEventCounter::MultiProcessEventCounter(perf::EventCounter&& event_counter,
                                                         std::vector<pid_t>&& process_ids)
{
  this->_process_local_counter.reserve(process_ids.size());
  auto config = event_counter.config();

  for (auto i = 0U; i < process_ids.size() - 1U; ++i) {
    config.process_id(process_ids[i]);
    auto process_local_counter = perf::EventCounter{ event_counter };
    process_local_counter.config(config);

    this->_process_local_counter.emplace_back(std::move(process_local_counter));
  }

  config.process_id(process_ids.back());
  event_counter.config(config);
  this->_process_local_counter.emplace_back(std::move(event_counter));
}

bool
perf::MultiProcessEventCounter::start()
{
  auto is_all_started = true;
  for (auto& event_counter : this->_process_local_counter) {
    is_all_started &= event_counter.start();
  }

  return is_all_started;
}

void
perf::MultiProcessEventCounter::stop()
{
  for (auto& event_counter : this->_process_local_counter) {
    event_counter.stop();
  }
}

perf::MultiCoreEventCounter::MultiCoreEventCounter(const perf::CounterDefinition& counter_list,
                                                   std::vector<std::uint16_t>&& cpu_ids,
                                                   perf::Config config)
{
  config.process_id(-1); /// Record every thread/process on the given CPUs.

  this->_cpu_local_counter.reserve(cpu_ids.size());

  for (const auto cpu_id : cpu_ids) {
    config.cpu_id(cpu_id);
    this->_cpu_local_counter.emplace_back(counter_list, config);
  }
}

perf::MultiCoreEventCounter::MultiCoreEventCounter(perf::EventCounter&& event_counter,
                                                   std::vector<std::uint16_t>&& cpu_ids)
{
  this->_cpu_local_counter.reserve(cpu_ids.size());
  auto config = event_counter.config();
  config.process_id(-1); /// Record every thread/process on the given CPUs.

  for (auto i = 0U; i < cpu_ids.size() - 1U; ++i) {
    config.cpu_id(cpu_ids[i]);
    auto process_local_counter = perf::EventCounter{ event_counter };
    process_local_counter.config(config);

    this->_cpu_local_counter.push_back(std::move(process_local_counter));
  }

  config.cpu_id(cpu_ids.back());
  event_counter.config(config);
  this->_cpu_local_counter.push_back(std::move(event_counter));
}

bool
perf::MultiCoreEventCounter::start()
{
  auto is_all_started = true;
  for (auto& event_counter : this->_cpu_local_counter) {
    is_all_started &= event_counter.start();
  }

  return is_all_started;
}

void
perf::MultiCoreEventCounter::stop()
{
  for (auto& event_counter : this->_cpu_local_counter) {
    event_counter.stop();
  }
}