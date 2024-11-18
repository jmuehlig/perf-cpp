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
perf::EventCounter::add(const std::string& event_name)
{
  /// If the counter has no name, we interpret this as the user wants to "close" the current group and add further
  /// counters to a fresh group.
  if (event_name.empty()) {
    /// Check if the current group is already empty.
    if (this->_groups.empty() || this->_groups.back().empty()) {
      return true;
    }

    /// Check if we have enough capacity for another group.
    if (this->size() < this->_config.max_groups()) {
      this->_groups.emplace_back();
      return true;
    }

    throw MaxCountersReachedError{ this->_config.max_groups() };
  }

  /// If the given name references an existing counter, add it.
  if (auto counter_config = this->_counter_definitions.counter(event_name); counter_config.has_value()) {
    this->add(std::get<0>(counter_config.value()), std::get<1>(counter_config.value()), true);
    return true;
  }

  /// If the given name references an existing metric, add the metric and all its required counters.
  if (auto metric = this->_counter_definitions.metric(event_name); metric.has_value()) {
    /// Add all hardware counters required by the metric..
    for (auto&& dependent_counter_name : std::get<1>(metric.value()).required_counter_names()) {
      if (auto dependent_counter_config = this->_counter_definitions.counter(dependent_counter_name);
          dependent_counter_config.has_value()) {
        this->add(std::get<0>(dependent_counter_config.value()), std::get<1>(dependent_counter_config.value()), false);
      } else {
        throw CannotFindEventForMetricError{ dependent_counter_name, event_name };
      }
    }

    /// If all of the metric's counters could be added (i.e., no exception was thrown), add the metric itself.
    this->_events.add(std::get<0>(metric.value()));
    return true;
  }

  throw CannotFindEventOrMetricError{ event_name };
}

bool
perf::EventCounter::add(const std::vector<std::string>& event_names)
{
  /// Add all counter names. If one of them fails, add() will throw an exception.
  for (const auto& event_name : event_names) {
    this->add(event_name);
  }

  /// If no exception was thrown, we are good to go. The bool is only returned for interface compatibility.
  return true;
}

void
perf::EventCounter::add(const std::string_view event_name,
                        perf::CounterConfig event_config,
                        const bool is_shown_in_results)
{
  /// If the event is already in the set, set the visibility to true (if is_shown_in_results == true), and return since
  /// we do not need to add the event twice.
  if (this->_events.adjust_visibility_if_present(event_name, is_shown_in_results)) {
    return;
  }

  /// Check if space for more counters left: If the latest group is "full", check, if there is space for another group.
  if (this->size() == this->_config.max_groups() &&
      this->_groups.back().size() >= this->_config.max_counters_per_group()) {
    throw MaxCountersReachedError{ this->_config.max_groups(), this->_config.max_counters_per_group() };
  }

  /// If the latest group is "full" (or no group was added, yet), add a new group. We already verified that there will
  /// be enough space.
  if (this->_groups.empty() || this->_groups.back().size() >= this->_config.max_counters_per_group()) {
    this->_groups.emplace_back();
  }

  /// The group the hardware counter is added to. If no groups exist or the latest group is already "full", we make sure
  /// to add another group before.
  auto& group = this->_groups.back();

  /// By organizing multiple hardware counters in groups (this->_groups), we need to remember the "true" order of events
  /// requested by the user (this->_events). To that end, we manage the user's requested events (this->_events) and
  /// associate each hardware event with two indices: the index of the group within the vector (group index) and the
  /// position of the hardware counter within that group (group.size()).
  const auto group_index = std::uint8_t(this->_groups.size() - 1U);
  if (this->_events.add(event_name, is_shown_in_results, group_index, std::uint8_t(group.size()))) {
    group.add(event_config);
  }
}

void
perf::EventCounter::add_live(const std::string& event_name)
{
  if (this->size() == this->_config.max_groups()) {
    throw MaxCountersReachedError{ this->_config.max_groups() };
  }

  /// If the given name references an existing counter, add it.
  if (auto counter_config = this->_counter_definitions.counter(event_name); counter_config.has_value()) {
    this->_live_counters.emplace_back(std::get<1>(counter_config.value()));
    this->_live_events.add(std::get<0>(counter_config.value()), std::uint8_t(this->_live_counters.size() - 1U));
    return;
  }

  /// If the counter does not exist, check if it is a metric, which is not supported for live events. Let the user know.
  if (auto metric = this->_counter_definitions.metric(event_name); metric.has_value()) {
    throw MetricNotSupportedAsLiveEventError{ event_name };
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
  if (const auto is_open = std::exchange(this->_is_open, true); !is_open) {
    /// Open all groups. If one of them fails, group.open() will throw an exception.
    for (auto& group : this->_groups) {
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
    for (auto& live_counter : this->_live_counters) {
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
  for (auto& group : this->_groups) {
    group.start();
  }

  /// Start all live counters.
  for (auto& live_counter : this->_live_counters) {
    live_counter.enable();
  }

  /// If no exception was thrown, we are good to go. The bool is only returned for interface compatibility.
  return true;
}

void
perf::EventCounter::stop()
{
  /// Stop all counter groups.
  for (auto& group : this->_groups) {
    group.stop();
  }

  /// Stop all live counters.
  for (auto& live_counter : this->_live_counters) {
    live_counter.disable();
  }
}

void
perf::EventCounter::close()
{
  if (const auto is_open = std::exchange(this->_is_open, false)) {
    /// Close all counter groups.
    for (auto& group : this->_groups) {
      group.close();
    }

    /// Close all live counters.
    for (auto& live_counter : this->_live_counters) {
      live_counter.close();
    }
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
    if (event.is_hardware_event()) {
      const auto scheduled_group = event.scheduled_group().value();
      const auto value = this->_groups[scheduled_group.id()].get(scheduled_group.position()) / double(normalization);
      hardware_event_values.emplace_back(event.name(), value);
    }
  }

  /// Turn the result of only hardware events into a result containing requested hardware events and metrics (which are
  /// calculated from hardware events).
  return this->_events.result(this->_counter_definitions, CounterResult{ std::move(hardware_event_values) });
}

void
perf::EventCounter::live_result(std::vector<double>& result) const noexcept
{
  for (auto counter_id = 0U; counter_id < this->_live_counters.size(); ++counter_id) {
    result[counter_id] = this->live_result(counter_id);
  }
}

void
perf::EventCounter::live_result(std::vector<double>& result, std::uint64_t normalization) const noexcept
{
  for (auto counter_id = 0U; counter_id < this->_live_counters.size(); ++counter_id) {
    result[counter_id] = this->live_result(counter_id, normalization);
  }
}

double
perf::EventCounter::live_result(const std::uint64_t counter_index) const noexcept
{
  return double(this->_live_counters[counter_index].read_live());
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
  std::transform(this->_live_events.begin(),
                 this->_live_events.end(),
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
perf::MultiEventCounterBase::add(std::string&& event_name)
{
  /// Add the event to every event counter.
  for (auto& event_counter : this->event_counters()) {
    event_counter.add(event_name);
  }

  /// The bool is only returned for interface compatibility.
  return true;
}

bool
perf::MultiEventCounterBase::add(const std::vector<std::string>& event_names)
{
  /// Add the event to every sub event counter.
  for (auto& event_counter : this->event_counters()) {
    event_counter.add(event_names);
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

  /// Build one result of only hardware-event values over all EventCounters by aggregating their values.
  auto aggregated_hardware_event_values = std::vector<std::pair<std::string_view, double>>{};
  aggregated_hardware_event_values.reserve(reference_event_counter._events.size());

  /// Accumulate all hardware events from EventCounters.
  for (const auto& event : reference_event_counter._events) {
    if (event.is_hardware_event()) {

      /// Add up the values from all individual EventCounters in event_counters.
      const auto aggregated_value = std::accumulate(
        this->event_counters().cbegin(),
        this->event_counters().cend(),
        .0,
        [group_id = event.scheduled_group()->id(),
         in_group_position = event.scheduled_group()->position()](const auto sum, const auto& event_counter) {
          return sum + event_counter._groups[group_id].get(in_group_position);
        });

      /// Normalize the value (by the given normalization parameter) and add to the aggregated results.
      aggregated_hardware_event_values.emplace_back(event.name(), aggregated_value / double(normalization));
    }
  }

  /// Turn the result of only aggregated hardware events into a result containing requested hardware events and metrics
  /// (which are calculated from hardware events).
  return reference_event_counter._events.result(reference_event_counter._counter_definitions,
                                                CounterResult{ std::move(aggregated_hardware_event_values) });
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