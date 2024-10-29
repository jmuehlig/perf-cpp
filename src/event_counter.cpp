#include <algorithm>
#include <numeric>
#include <perfcpp/perf.h>
#include <stdexcept>
bool
perf::EventCounter::add(std::string&& event_name)
{
  /// If the counter has no name, we interpret this as the user wants to "close" the group and add further counters to
  /// another one.
  if (event_name.empty()) {
    if (this->_groups.empty() || this->_groups.back().empty()) {
      return true;
    }

    if (this->_groups.size() < this->_config.max_groups()) {
      this->_groups.emplace_back();
      return true;
    }

    throw std::runtime_error{ std::string{ "Cannot add another group – number of groups: " }
                                .append(std::to_string(this->_groups.size()))
                                .append(", maximal number of groups: ")
                                .append(std::to_string(std::size_t{ this->_config.max_groups() })) };
  }

  /// If the given name references an existing counter, add it.
  if (auto counter_config = this->_counter_definitions.counter(event_name); counter_config.has_value()) {
    this->add(std::get<0>(counter_config.value()), std::get<1>(counter_config.value()), true);
    return true;
  }

  /// If the given name references an existing metric, add the metric and all its required counters.
  if (auto metric = this->_counter_definitions.metric(event_name); metric.has_value()) {
    /// Add all required counters.
    for (auto&& dependent_counter_name : std::get<1>(metric.value()).required_counter_names()) {
      if (auto dependent_counter_config = this->_counter_definitions.counter(dependent_counter_name);
          dependent_counter_config.has_value()) {
        this->add(std::get<0>(dependent_counter_config.value()), std::get<1>(dependent_counter_config.value()), false);
      } else {
        throw std::runtime_error{ std::string{ "Cannot find event '" }
                                    .append(dependent_counter_name)
                                    .append("' for metric '")
                                    .append(event_name)
                                    .append("'.") };
      }
    }

    /// If all of the metric's counters could be added (i.e., no exception was thrown), add the metric itself.
    this->_events.emplace_back(std::get<0>(metric.value()));
    return true;
  }

  throw std::runtime_error{ std::string{ "Cannot find event or metric with name '" }.append(event_name).append("'.") };
}

void
perf::EventCounter::add(std::string_view event_name, perf::CounterConfig counter, const bool is_shown_in_results)
{
  /// Check if the event is already added.
  if (auto iterator = std::find_if(this->_events.begin(),
                                   this->_events.end(),
                                   [&event_name](const auto& counter) { return counter.name() == event_name; });
      iterator != this->_events.end()) {
    /// If so, there is no need to add it again – but we need to check if the event was requested (this time) by the
    /// user to show it in the result set. One scenario could be, that the event was added earlier by a metric (i.e., it
    /// should not appear in the results), but now, the user requests it, too – switching the state to "show in
    /// results".
    iterator->is_shown_in_results(iterator->is_shown_in_results() || is_shown_in_results);
    return;
  }

  /// Check if space for more counters left: If the latest group is "full", check, if there is space for another group.
  if (this->_groups.size() == this->_config.max_groups() &&
      this->_groups.back().size() >= this->_config.max_counters_per_group()) {
    throw std::runtime_error{
      "Cannot add more events: Reached maximum number of groups and maximum number of events in the latest group."
    };
  }

  /// If the latest group is "full", add a new group. We already verified that there will be enough space.
  if (this->_groups.empty() || this->_groups.back().size() >= this->_config.max_counters_per_group()) {
    this->_groups.emplace_back();
  }

  /// Remember the group and the index within the group of the event.
  const auto group_id = std::uint8_t(this->_groups.size()) - 1U;
  const auto in_group_id = std::uint8_t(this->_groups.back().size());

  this->_events.emplace_back(event_name, is_shown_in_results, group_id, in_group_id);

  /// Add the event config to the last group.
  this->_groups.back().add(counter);
}

bool
perf::EventCounter::add(std::vector<std::string>&& event_names)
{
  /// Add all counter names. If one of them fails, add() will throw an exception.
  for (auto& name : event_names) {
    this->add(std::move(name));
  }

  /// If no exception was thrown, we are good to go.
  return true;
}

bool
perf::EventCounter::add(const std::vector<std::string>& event_names)
{
  return this->add(std::vector<std::string>(event_names));
}

bool
perf::EventCounter::start()
{
  /// Open all counters. If one of them fails, group.open() will throw an exception.
  for (auto& group : this->_groups) {
    group.open(this->_config);
  }

  /// Start all counters. If one of them fails, group.start() will throw an exception.
  for (auto& group : this->_groups) {
    group.start();
  }

  /// If no exception was thrown, we are good to go.
  return true;
}

void
perf::EventCounter::stop()
{
  /// Stop all counters. If one of them fails, group.stop() will throw an exception.
  for (auto& group : this->_groups) {
    group.stop();
  }

  /// Close all counters. If one of them fails, group.close() will throw an exception.
  for (auto& group : this->_groups) {
    group.close();
  }
}

perf::CounterResult
perf::EventCounter::result(std::uint64_t normalization) const
{
  /// Build result with all counters, including hidden ones.
  auto hardware_event_values = std::vector<std::pair<std::string_view, double>>{};
  hardware_event_values.reserve(this->_events.size());

  /// Copy only the hardware-event values.
  for (const auto& event : this->_events) {
    if (event.is_event()) {
      const auto value = this->_groups[event.group_id()].get(event.in_group_id()) / double(normalization);
      hardware_event_values.emplace_back(event.name(), value);
    }
  }

  /// This result only contains hardware-event values to either copy the value (if the event is requested) or use the
  /// value for calculating a metric.
  auto hardware_events_result = CounterResult{ std::move(hardware_event_values) };

  /// List of all requested values (hardware-events and metrics)
  auto result = std::vector<std::pair<std::string_view, double>>{};
  result.reserve(this->_events.size());

  for (const auto& event : this->_events) {
    /// First, add all hardware events that were requested to be shown: event.is_shown_in_results() indicates that
    /// the event was requested by the user and not only required by a metric.
    if (event.is_event()) {
      if (event.is_shown_in_results()) {
        if (const auto value = hardware_events_result.get(event.name()); value.has_value()) {
          result.emplace_back(event.name(), value.value());
        }
      }
    }

    /// If the event is a metric (not a hardware event), calculate the value of the metric and add it to the result.
    else if (auto metric = this->_counter_definitions.metric(event.name()); metric.has_value()) {
      if (const auto value = std::get<1>(metric.value()).calculate(hardware_events_result); value.has_value()) {
        result.emplace_back(std::get<0>(metric.value()), value.value());
      }
    }
  }

  return CounterResult{ std::move(result) };
}

bool
perf::MultiEventCounterBase::add(std::string&& event_name)
{
  /// Add the event to every event counter.
  for (auto i = 0U; i < this->event_counters().size() - 1U; ++i) {
    this->event_counters()[i].add(std::string{ event_name });
  }

  /// Re-use the event name for the last event counter.
  return this->event_counters().back().add(std::move(event_name));
}

bool
perf::MultiEventCounterBase::add(std::vector<std::string>&& event_names)
{
  /// Add the events to every counter.
  for (auto i = 0U; i < this->event_counters().size() - 1U; ++i) {
    this->event_counters()[i].add(std::vector<std::string>{ event_names });
  }

  /// Re-use the list of event names for the last event counter.
  return this->event_counters().back().add(std::move(event_names));
}

bool
perf::MultiEventCounterBase::add(const std::vector<std::string>& event_names)
{
  for (auto& event_counter : this->event_counters()) {
    event_counter.add(event_names);
  }

  return true;
}

void
perf::MultiEventCounterBase::stop()
{
  for (auto& event_counter : this->event_counters()) {
    event_counter.stop();
  }
}

perf::CounterResult
perf::MultiEventCounterBase::result(const std::uint64_t normalization) const
{
  /// The reference_event_counter is used to access counters (all EventCounters from the list are required to have the
  /// same counters but different values).
  const auto& reference_event_counter = this->event_counters().front();

  /// Build one result of only hardware-event values over all EventCounters by aggregating their values.
  auto aggregated_hardware_event_values = std::vector<std::pair<std::string_view, double>>{};
  aggregated_hardware_event_values.reserve(reference_event_counter._events.size());

  /// For every (hardware) event, accumulate the results for every EventCounter from event_counters.
  for (const auto& event : reference_event_counter._events) {
    if (event.is_event()) {

      /// Add up the values from all individual EventCounters in event_counters.
      const auto value = std::accumulate(
        this->event_counters().begin(),
        this->event_counters().end(),
        .0,
        [id = event.group_id(), in_group_id = event.in_group_id()](const double sum, const auto& event_counter) {
          return sum + event_counter._groups[id].get(in_group_id);
        });

      /// Normalize the value (by the given normalization parameter) and add to the aggregated results.
      aggregated_hardware_event_values.emplace_back(event.name(), value / double(normalization));
    }
  }

  /// This result only contains hardware-event values to either copy the value (if the event is requested) or use the
  /// value for calculating a metric.
  auto hardware_event_results = CounterResult{ std::move(aggregated_hardware_event_values) };

  /// List of all requested values (hardware-events and metrics)
  auto result = std::vector<std::pair<std::string_view, double>>{};
  result.reserve(reference_event_counter._events.size());

  for (const auto& event : reference_event_counter._events) {
    /// First, add all hardware events that were requested to be shown: event.is_shown_in_results() indicates that
    /// the event was requested by the user and not only required by a metric.
    if (event.is_event()) {
      if (event.is_shown_in_results()) {
        if (const auto value = hardware_event_results.get(event.name()); value.has_value()) {
          result.emplace_back(event.name(), value.value());
        }
      }
    }

    /// If the event is a metric (not a hardware event), calculate the value of the metric and add it to the result.
    else if (auto metric = reference_event_counter._counter_definitions.metric(event.name()); metric.has_value()) {
      if (auto value = std::get<1>(metric.value()).calculate(hardware_event_results); value.has_value()) {
        result.emplace_back(std::get<0>(metric.value()), value.value());
      }
    }
  }

  return CounterResult{ std::move(result) };
}

bool
perf::StartableMultiEventCounterBase::start()
{
  for (auto& event_counter : this->event_counters()) {
    event_counter.start();
  }

  return true;
}

perf::MultiThreadEventCounter::MultiThreadEventCounter(const perf::CounterDefinition& counter_definition,
                                                       const std::uint16_t num_threads,
                                                       const perf::Config config)
{
  this->_thread_local_counter.reserve(num_threads);
  for (auto i = 0U; i < num_threads; ++i) {
    this->_thread_local_counter.emplace_back(counter_definition, config);
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

    /// Create one counter for every process: Copy the config for every process and bind the EventCounter to that
    /// process.
    config.process_id(process_ids[i]);
    auto process_local_counter = perf::EventCounter{ event_counter };
    process_local_counter.config(config);

    this->_process_local_counter.emplace_back(std::move(process_local_counter));
  }

  /// Re-use the given EventCounter for the last process in the list.
  config.process_id(process_ids.back());
  event_counter.config(config);
  this->_process_local_counter.emplace_back(std::move(event_counter));
}

perf::MultiCoreEventCounter::MultiCoreEventCounter(const perf::CounterDefinition& counter_definition,
                                                   std::vector<std::uint16_t>&& cpu_ids,
                                                   perf::Config config)
{
  config.process_id(-1); /// Record every thread/process on the given CPUs.

  this->_cpu_local_counter.reserve(cpu_ids.size());

  for (const auto cpu_id : cpu_ids) {
    config.cpu_id(cpu_id);
    this->_cpu_local_counter.emplace_back(counter_definition, config);
  }
}

perf::MultiCoreEventCounter::MultiCoreEventCounter(perf::EventCounter&& event_counter,
                                                   std::vector<std::uint16_t>&& cpu_ids)
{
  this->_cpu_local_counter.reserve(cpu_ids.size());
  auto config = event_counter.config();
  config.process_id(-1); /// Record every thread/process on the given CPUs.

  for (auto i = 0U; i < cpu_ids.size() - 1U; ++i) {

    /// Create one EventCounter for every CPU core from the list via config.
    config.cpu_id(cpu_ids[i]);
    auto process_local_counter = perf::EventCounter{ event_counter };
    process_local_counter.config(config);

    this->_cpu_local_counter.push_back(std::move(process_local_counter));
  }

  /// Re-use the given EventCounter for the last CPU Id in the list.
  config.cpu_id(cpu_ids.back());
  event_counter.config(config);
  this->_cpu_local_counter.push_back(std::move(event_counter));
}