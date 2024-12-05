#include <algorithm>
#include <numeric>
#include <perfcpp/event_counter.h>
#include <perfcpp/exception.h>
#include <stdexcept>
#include <utility>

perf::EventCounter::~EventCounter()
{
  this->close();
}

bool
perf::EventCounter::add(const std::string& event_name, const Schedule schedule)
{
  auto events = std::vector<std::tuple<std::string_view, RequestedEvent::Type, std::optional<CounterConfig>, bool>>{};
  this->unfold(event_name, events);

  /// Schedule the events to hardware event counters.
  this->schedule(std::move(events), schedule);

  /// If no exception was thrown, we are good to go. The bool is only returned for interface compatibility.
  return true;
}

bool
perf::EventCounter::add(const std::vector<std::string>& event_names, const Schedule schedule)
{
  auto events = std::vector<std::tuple<std::string_view, RequestedEvent::Type, std::optional<CounterConfig>, bool>>{};

  /// Unfold all counters.
  for (const auto& event_name : event_names) {
    this->unfold(event_name, events);
  }

  /// Schedule the events to hardware event counters.
  this->schedule(std::move(events), schedule);

  /// If no exception was thrown, we are good to go. The bool is only returned for interface compatibility.
  return true;
}

void
perf::EventCounter::unfold(
  const std::string& event_name,
  std::vector<std::tuple<std::string_view, RequestedEvent::Type, std::optional<CounterConfig>, bool>>& result_vector)
  const
{
  if (const auto counter_config = this->_counter_definitions.counter(event_name); counter_config.has_value()) {
    EventCounter::add(std::get<0>(counter_config.value()),
                      std::get<1>(counter_config.value()),
                      /* requested hardware counters are visible */ true,
                      result_vector);
  }

  /// If the given name references an existing metric, add the metric and all its required counters.
  else if (const auto metric = this->_counter_definitions.metric(event_name); metric.has_value()) {
    /// Add all hardware counters required by the metric..
    for (auto&& dependent_counter_name : std::get<1>(metric.value()).required_counter_names()) {
      if (const auto dependent_counter_config = this->_counter_definitions.counter(dependent_counter_name);
          dependent_counter_config.has_value()) {
        EventCounter::add(std::get<0>(dependent_counter_config.value()),
                          std::get<1>(dependent_counter_config.value()),
                          /* hardware counters only used for metrics are not visible */ false,
                          result_vector);
      } else if (const auto dependent_time_event = this->_counter_definitions.time_event(dependent_counter_name);
                 dependent_time_event.has_value()) {
        result_vector.emplace_back(
          std::get<0>(dependent_time_event.value()), RequestedEvent::Type::TimeEvent, std::nullopt, false);
      } else {
        throw CannotFindEventForMetricError{ dependent_counter_name, event_name };
      }
    }

    /// If all of the metric's counters could be added (i.e., no exception was thrown), add the metric itself.
    result_vector.emplace_back(std::get<0>(metric.value()), RequestedEvent::Type::Metric, std::nullopt, true);
  }

  /// If the given name references an existing time event, add the time event.
  else if (const auto time_event = this->_counter_definitions.time_event(event_name); time_event.has_value()) {
    result_vector.emplace_back(std::get<0>(time_event.value()), RequestedEvent::Type::TimeEvent, std::nullopt, true);
  } else {
    throw CannotFindEventOrMetricError{ event_name };
  }
}

void
perf::EventCounter::add(
  const std::string_view event_name,
  const perf::CounterConfig& counter_config,
  const bool is_shown_in_results,
  std::vector<std::tuple<std::string_view, RequestedEvent::Type, std::optional<CounterConfig>, bool>>& result_vector)
{
  /// Find an event with the same name in the result vector.
  auto iterator = std::find_if(result_vector.begin(), result_vector.end(), [event_name](const auto& event) {
    return std::get<0>(event) == event_name;
  });
  if (iterator == result_vector.end()) {
    /// Append, if the event does not exist.
    result_vector.emplace_back(event_name, RequestedEvent::Type::HardwareEvent, counter_config, is_shown_in_results);
  } else if (is_shown_in_results) {
    /// Adjust visibility if the event should be shown.
    std::get<3>(*iterator) = true;
  }
}

void
perf::EventCounter::schedule(
  std::vector<std::tuple<std::string_view, RequestedEvent::Type, std::optional<CounterConfig>, bool>>&& events,
  const perf::EventCounter::Schedule schedule)
{
  if (schedule == Schedule::Append) {
    for (const auto& [event_name, type, counter_config, is_shown_in_results] : events) {
      /// Metrics and time events (indicated by no hardware counter config) do not need to be scheduled to hardware
      /// counter groups; just add it to the event set.
      if (!counter_config.has_value()) {
        this->_requested_event_set.add(event_name, type, is_shown_in_results);
        continue;
      }

      /// If the event is already in the set, set the visibility to true (if is_shown_in_results == true), and return
      /// since we do not need to add the event twice.
      if (this->_requested_event_set.adjust_visibility_if_present(event_name, is_shown_in_results)) {
        continue;
      }

      /// Try to find a group where we can append the counter.
      if (this->append_to_any_hardware_counter(event_name, counter_config.value(), is_shown_in_results)) {
        continue;
      }

      /// If we did not find any group, try to create a new one. Raise an exception, if the maximal number of groups is
      /// reached.
      if (this->size() == this->_config.max_groups()) {
        throw MaxCountersReachedError{ this->_config.max_groups(), this->_config.max_counters_per_group() };
      }

      /// Create a new group and add the counter, if we did not raise an exception.
      auto& group_and_flag = this->_hardware_event_groups.emplace_back(
        Group{},
        /* instantly close, if we can only add one counter per group */ this->_config.max_counters_per_group() > 1U);
      std::get<0>(group_and_flag).add(counter_config.value());

      /// Add to the request set.
      const auto group_id = std::uint8_t(this->_hardware_event_groups.size() - 1U);
      this->_requested_event_set.add(
        event_name, is_shown_in_results, group_id, /* the counter is the first in the group */ 0U);
    }
  } else if (schedule == Schedule::Separate) {
    for (const auto& [event_name, type, counter_config, is_shown_in_results] : events) {
      /// Metrics and time events (indicated by no hardware counter config) do not need to be scheduled to hardware
      /// counter groups; just add it to the event set.
      if (!counter_config.has_value()) {
        this->_requested_event_set.add(event_name, type, is_shown_in_results);
        continue;
      }

      /// If the event is already in the set, set the visibility to true (if is_shown_in_results == true), and return
      /// since we do not need to add the event twice.
      if (this->_requested_event_set.adjust_visibility_if_present(event_name, is_shown_in_results)) {
        continue;
      }

      /// Test if we can add another group.
      if (this->size() == this->_config.max_groups()) {
        throw MaxGroupsReachedError{ this->_config.max_groups() };
      }

      /// Create a new group and add the counter, if we did not raise an exception.
      auto& group_and_flag = this->_hardware_event_groups.emplace_back(
        Group{}, /* the counter should be scheduled individually; close it */ false);
      std::get<0>(group_and_flag).add(counter_config.value());

      /// Add to the request set.
      const auto group_id = std::uint8_t(this->_hardware_event_groups.size() - 1U);
      this->_requested_event_set.add(
        event_name, is_shown_in_results, group_id, /* the counter is the first in the group */ 0U);
    }
  } else if (schedule == Schedule::Group) {
    /// Test if we can add another group.
    if (this->size() == this->_config.max_groups()) {
      throw MaxGroupsReachedError{ this->_config.max_groups() };
    }

    /// Test, if all the hardware counters fit into a single group.
    const auto count_hardware_counters =
      std::count_if(events.begin(), events.end(), [](const auto& event) { return std::get<2>(event).has_value(); });
    if (count_hardware_counters > this->_config.max_counters_per_group()) {
      throw CannotAddCountersToSingleGroupError{ std::uint64_t(count_hardware_counters),
                                                 this->_config.max_counters_per_group() };
    }

    /// Create a new group and add the counter, if we did not raise an exception.
    auto& [group, _] = this->_hardware_event_groups.emplace_back(
      Group{}, /* the counter should be scheduled individually; close it */ false);

    /// Add all events.
    const auto group_id = std::uint8_t(this->_hardware_event_groups.size() - 1U);
    for (const auto& [event_name, type, counter_config, is_shown_in_results] : events) {
      /// Metrics and time events (indicated by no hardware counter config) do not need to be scheduled to hardware
      /// counter groups; just add it to the event set.
      if (!counter_config.has_value()) {
        this->_requested_event_set.add(event_name, type, is_shown_in_results);
        continue;
      }

      /// If the event is already in the set, set the visibility to true (if is_shown_in_results == true), and return
      /// since we do not need to add the event twice.
      if (this->_requested_event_set.adjust_visibility_if_present(event_name, is_shown_in_results)) {
        continue;
      }

      /// Add the hardware counter to the group.
      const auto in_group_position = std::uint8_t(group.size());
      group.add(counter_config.value());

      /// Add to the request set.
      this->_requested_event_set.add(event_name, is_shown_in_results, group_id, in_group_position);
    }
  }
}

bool
perf::EventCounter::append_to_any_hardware_counter(const std::string_view event_name,
                                                   const perf::CounterConfig& counter_config,
                                                   const bool is_shown_in_results)
{
  for (auto group_id = 0U; group_id < this->_hardware_event_groups.size(); ++group_id) {
    const auto is_group_open = std::get<1>(this->_hardware_event_groups[group_id]);
    if (is_group_open) {
      /// We found a matching group that has space.
      auto& group = std::get<0>(this->_hardware_event_groups[group_id]);
      const auto in_group_position = std::uint8_t(group.size());

      /// Add to the hardware counter group.
      group.add(counter_config);

      /// Add to the request set.
      this->_requested_event_set.add(event_name, is_shown_in_results, std::uint8_t(group_id), in_group_position);

      /// Close the group if full.
      if (group.size() == this->_config.max_counters_per_group()) {
        std::get<1>(this->_hardware_event_groups[group_id]) = false;
      }

      return true;
    }
  }

  return false;
}

void
perf::EventCounter::add_live(const std::string& event_name)
{
  if (this->size() == this->_config.max_groups()) {
    throw MaxCountersReachedError{ this->_config.max_groups() };
  }

  /// If the given name references an existing counter, add it.
  if (const auto counter_config = this->_counter_definitions.counter(event_name); counter_config.has_value()) {
    this->_hardware_live_counters.emplace_back(std::get<1>(counter_config.value()));
    this->_requested_live_event_set.add(std::get<0>(counter_config.value()),
                                        std::uint8_t(this->_hardware_live_counters.size() - 1U));
    return;
  }

  /// If the counter does not exist, check if it is a metric, which is not supported for live events. Let the user know.
  if (this->_counter_definitions.is_metric(event_name)) {
    throw MetricNotSupportedAsLiveEventError{ event_name };
  }

  /// If the counter does not exist, check if it is a time event, which is not supported for live events. Let the user
  /// know.
  if (this->_counter_definitions.is_time_event(event_name)) {
    throw TimeEventNotSupportedAsLiveEventError{ event_name };
  }

  throw CannotFindEventError{ event_name };
}

void
perf::EventCounter::add_live(std::vector<std::string>&& event_names)
{
  for (const auto& event_name : event_names) {
    this->add_live(event_name);
  }
}

void
perf::EventCounter::open()
{
  /// Verify that the EventCounter is not already opened (_is_open == false) and set flag appropriately.
  if (const auto is_open = std::exchange(this->_is_opened, true); !is_open) {
    /// Open all groups. If one of them fails, group.open() will throw an exception.
    for (auto& [group, _] : this->_hardware_event_groups) {
      group.open(this->_config,
                 /* is read format */ true,
                 /* has auxiliary counter */ false,
                 /* buffer pages */ std::nullopt,
                 /* sample type */ std::nullopt,
                 /* branch type */ std::nullopt,
                 /* user registers */ std::nullopt,
                 /* kernel registers */ std::nullopt,
                 /* max user stack size */ std::nullopt,
                 /* max callstack size */ std::nullopt,
                 /* include context switches */ false,
                 /* include cgroup */ false);
    }

    /// Open all live counters. If one of them fails, counter.open() will throw an exception.
    for (auto& live_counter : this->_hardware_live_counters) {
      live_counter.open(this->_config,
                        /* is group leader */ true,
                        /* is secret group leader */ false,
                        /* group leader file descriptor */ -1,
                        /* is read format */ false,
                        /* buffer pages */ std::make_optional(1ULL),
                        /* sample type */ std::make_optional(PERF_SAMPLE_READ),
                        /* branch type */ std::nullopt,
                        /* user registers */ std::nullopt,
                        /* kernel registers */ std::nullopt,
                        /* max user stack size */ std::nullopt,
                        /* max callstack size */ std::nullopt,
                        /* include context switches */ false,
                        /* include cgroup */ false);
    }
  }
}

bool
perf::EventCounter::start()
{
  /// Opens the hardware performance counters, if not already done specifically by calling EventCounter::open().
  this->open();

  /// Start all counter groups. If one of them fails, group.start() will throw an exception.
  for (auto& [group, _] : this->_hardware_event_groups) {
    group.start();
  }

  /// Start all live counters.
  for (auto& live_counter : this->_hardware_live_counters) {
    live_counter.enable();
  }

  /// Start timer.
  std::get<0>(this->_start_and_end_time) = std::chrono::steady_clock::now();

  /// If no exception was thrown, we are good to go. The bool is only returned for interface compatibility.
  return true;
}

void
perf::EventCounter::stop()
{
  /// Stop timer.
  std::get<1>(this->_start_and_end_time) = std::chrono::steady_clock::now();

  /// Stop all counter groups.
  for (auto& [group, _] : this->_hardware_event_groups) {
    group.stop();
  }

  /// Stop all live counters.
  for (auto& live_counter : this->_hardware_live_counters) {
    live_counter.disable();
  }
}

void
perf::EventCounter::close()
{
  if (const auto is_open = std::exchange(this->_is_opened, false); is_open) {
    /// Close all counter groups.
    for (auto& [group, _] : this->_hardware_event_groups) {
      group.close();
    }

    /// Close all live counters.
    for (auto& live_counter : this->_hardware_live_counters) {
      live_counter.close();
    }
  }
}

perf::CounterResult
perf::EventCounter::result(const std::uint64_t normalization) const
{
  /// Build result with all counters, including hidden ones.
  auto event_values = std::vector<std::pair<std::string_view, double>>{};
  event_values.reserve(this->_requested_event_set.size());

  /// Copy only the hardware- and time-event values.
  for (const auto& event : this->_requested_event_set) {
    /// Hardware events are read from the hardware counter (groups).
    if (event.is_hardware_event()) {
      const auto scheduled_group = event.scheduled_group().value();
      const auto& group = std::get<0>(this->_hardware_event_groups[scheduled_group.id()]);
      event_values.emplace_back(event.name(), group.get(scheduled_group.position()));
    }

    /// Time events are read using the event counter's start and stop time.
    else if (event.is_time_event()) {
      if (const auto& time_calculator = this->_counter_definitions.time_event(event.name());
          time_calculator.has_value()) {
        const auto start_timestamp = std::get<0>(this->_start_and_end_time);
        const auto stop_timestamp = std::get<1>(this->_start_and_end_time);
        const auto time = std::get<1>(time_calculator.value()).calculate(start_timestamp, stop_timestamp);
        event_values.emplace_back(event.name(), time);
      }
    }
  }

  /// Turn the result of only hardware events into a result containing requested hardware events and metrics (which are
  /// calculated from hardware events).
  return this->_requested_event_set.result(this->_counter_definitions, CounterResult{ std::move(event_values) }, normalization);
}

void
perf::EventCounter::live_result(std::vector<double>& result) const noexcept
{
  for (auto counter_id = 0U; counter_id < this->_hardware_live_counters.size(); ++counter_id) {
    result[counter_id] = this->live_result(counter_id);
  }
}

void
perf::EventCounter::live_result(std::vector<double>& result, std::uint64_t normalization) const noexcept
{
  for (auto counter_id = 0U; counter_id < this->_hardware_live_counters.size(); ++counter_id) {
    result[counter_id] = this->live_result(counter_id, normalization);
  }
}

double
perf::EventCounter::live_result(const std::uint64_t counter_index) const noexcept
{
  return double(this->_hardware_live_counters[counter_index].read_live());
}

double
perf::EventCounter::live_result(const std::uint64_t counter_index, const std::uint64_t normalization) const noexcept
{
  return this->live_result(counter_index) / double(normalization);
}

std::vector<std::string_view>
perf::EventCounter::live_event_names() const
{
  auto names = std::vector<std::string_view>{};
  std::transform(this->_requested_live_event_set.begin(),
                 this->_requested_live_event_set.end(),
                 std::back_inserter(names),
                 [](const auto& event) { return event.name(); });
  return names;
}

perf::LiveEventCounter::LiveEventCounter(const perf::EventCounter& event_counter)
  : _event_counter(event_counter)
  , _event_names(event_counter.live_event_names())
{
  this->_counter_values.resize(this->_event_names.size(), { .0, .0 });
}

void
perf::LiveEventCounter::start() noexcept
{
  for (auto counter_id = 0U; counter_id < this->_counter_values.size(); ++counter_id) {
    /// Set <start value> to current value.
    this->_counter_values[counter_id].first = this->_event_counter.live_result(counter_id);
  }
}

void
perf::LiveEventCounter::stop() noexcept
{
  for (auto counter_id = 0U; counter_id < this->_counter_values.size(); ++counter_id) {
    /// Set <stop value> to current value.
    this->_counter_values[counter_id].second = this->_event_counter.live_result(counter_id);
  }
}

double
perf::LiveEventCounter::get(const std::string_view event_name) const noexcept
{
  for (auto event_index = 0U; event_index < this->_event_names.size(); ++event_index) {
    /// Find the event matching the given name.
    if (this->_event_names[event_index] == event_name) {
      /// Calculate the difference by <stop value> - <start value>.
      return this->_counter_values[event_index].second - this->_counter_values[event_index].first;
    }
  }

  return .0;
}

double
perf::LiveEventCounter::get(const std::string_view event_name, const std::uint64_t normalization) const noexcept
{
  return this->get(event_name) / double(normalization);
}

bool
perf::MultiEventCounterBase::add(std::string&& event_name, const perf::EventCounter::Schedule schedule)
{
  /// Add the event to every event counter.
  for (auto& event_counter : this->event_counters()) {
    event_counter.add(event_name, schedule);
  }

  /// The bool is only returned for interface compatibility.
  return true;
}

bool
perf::MultiEventCounterBase::add(const std::vector<std::string>& event_names,
                                 const perf::EventCounter::Schedule schedule)
{
  /// Add the event to every sub event counter.
  for (auto& event_counter : this->event_counters()) {
    event_counter.add(event_names, schedule);
  }

  /// The bool is only returned for interface compatibility.
  return true;
}

void
perf::MultiEventCounterBase::stop()
{
  /// Stop every sub event counter.
  for (auto& event_counter : this->event_counters()) {
    event_counter.stop();
  }
}

void
perf::MultiEventCounterBase::close()
{
  /// Close every sub event counter.
  for (auto& event_counter : this->event_counters()) {
    event_counter.close();
  }
}

perf::CounterResult
perf::MultiEventCounterBase::result(const std::uint64_t normalization) const
{
  /// Aggregate hardware events from all individual EventCounters and turn into a list of requested values.

  /// The reference_event_counter is used to access counters (all EventCounters from the list are required to have the
  /// same events but different values).
  const auto& reference_event_counter = this->event_counters().front();
  const auto& reference_event_set = reference_event_counter._requested_event_set;

  /// Build one result of only hardware-event values over all EventCounters by aggregating their values.
  auto aggregated_event_values = std::vector<std::pair<std::string_view, double>>{};
  aggregated_event_values.reserve(reference_event_set.size());

  /// Accumulate all hardware and time events from EventCounters.
  for (const auto& event : reference_event_set) {
    /// Hardware events are read via hardware counter (groups).
    if (event.is_hardware_event()) {

      /// Add up the values from all individual EventCounters in event_counters.
      const auto aggregated_value = std::accumulate(
        this->event_counters().cbegin(),
        this->event_counters().cend(),
        .0,
        [group_id = event.scheduled_group()->id(),
         in_group_position = event.scheduled_group()->position()](const auto sum, const auto& event_counter) {
          const auto& group = std::get<0>(event_counter._hardware_event_groups[group_id]);
          return sum + group.get(in_group_position);
        });

      /// Add to the aggregated results.
      aggregated_event_values.emplace_back(event.name(), aggregated_value);
    }

    /// Time events are read via event counter's start and stop timestamps.
    else if (event.is_time_event()) {
      if (const auto time_event = reference_event_counter._counter_definitions.time_event(event.name());
          time_event.has_value()) {
        /// Aggregate the values from all individual EventCounters in event_counters.
        const auto aggregated_value = std::accumulate(
          this->event_counters().cbegin(),
          this->event_counters().cend(),
          .0,
          [&time_calculator = std::get<1>(time_event.value())](const auto sum, const auto& event_counter) {
            const auto start_timestamp = std::get<0>(event_counter._start_and_end_time);
            const auto stop_timestamp = std::get<1>(event_counter._start_and_end_time);
            return sum + time_calculator.calculate(start_timestamp, stop_timestamp);
          });

        /// Add to the aggregated results.
        aggregated_event_values.emplace_back(event.name(), aggregated_value);
      }
    }
  }

  /// Turn the result of only aggregated hardware events into a result containing requested hardware events and metrics
  /// (which are calculated from hardware events).
  return reference_event_set.result(reference_event_counter._counter_definitions,
                                                             CounterResult{ std::move(aggregated_event_values) }, normalization);
}

bool
perf::StartableMultiEventCounterBase::start()
{
  /// Start every sub event counter.
  for (auto& event_counter : this->event_counters()) {
    event_counter.start();
  }

  /// The bool is only returned for interface compatibility.
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