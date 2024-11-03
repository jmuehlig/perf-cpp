#pragma once

#include "config.h"
#include "counter.h"
#include "counter_definition.h"
#include "group.h"
#include <chrono>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace perf {
/**
 * The EventCounter allows to specify events that should be counted by hardware performance counters and start/stop
 * counting.
 */
class EventCounter
{
  friend class MultiEventCounterBase;

private:
  /**
   * The Event class stores information about events that will be recorded, e.g., the group the event is scheduled to,
   * the index within the group, the name, and a flag if the event should be shown within the results (which is not true
   * for events only needed for metrics). The EventCounter will have an ordered list of events, dictating the order the
   * user requested the events to output the events in exactly that order.
   */
  class Event
  {
  public:
    explicit Event(std::string_view name) noexcept
      : _name(name)
      , _is_shown_in_results(false)
      , _is_event(false)
      , _group_id(0U)
      , _in_group_id(0U)
    {
    }

    Event(std::string_view name,
          const bool is_hidden,
          const std::uint8_t group_id,
          const std::uint8_t in_group_id) noexcept
      : _name(name)
      , _is_shown_in_results(is_hidden)
      , _is_event(true)
      , _group_id(group_id)
      , _in_group_id(in_group_id)
    {
    }

    ~Event() = default;

    [[nodiscard]] std::string_view name() const noexcept { return _name; }
    [[nodiscard]] bool is_event() const noexcept { return _is_event; }
    [[nodiscard]] bool is_shown_in_results() const noexcept { return _is_shown_in_results; }
    [[nodiscard]] std::uint8_t group_id() const noexcept { return _group_id; }
    [[nodiscard]] std::uint8_t in_group_id() const noexcept { return _in_group_id; }

    void is_shown_in_results(const bool is_shown_in_results) noexcept { _is_shown_in_results = is_shown_in_results; }

  private:
    /// Name of the event (references a string in the CounterDefinition).
    std::string_view _name;

    /// Indicates that the event is a "real" hardware event, not a metric.
    bool _is_event;

    /// Indicates that the event is included into results. Some events are "only" requested by metrics and are only
    /// needed for calculating them but are not requested by the user.
    bool _is_shown_in_results;

    /// Id of the group the event is placed in.
    std::uint8_t _group_id{ 0U };

    /// Id within a group.
    std::uint8_t _in_group_id{ 0U };
  };

public:
  explicit EventCounter(const CounterDefinition& counter_definition, Config config = {})
    : _counter_definitions(counter_definition)
    , _config(config)
  {
  }
  EventCounter(EventCounter&&) noexcept = default;
  EventCounter(const EventCounter&) = default;

  ~EventCounter() = default;

  /**
   * Add the specified event to the list of countered performance events.
   * The event must exist within the counter definitions.
   *
   * @param event_name Name of the event.
   * @return True, if the event could be added.
   */
  bool add(std::string&& event_name) { return add(event_name); }

  /**
   * Add the specified event to the list of monitored countered events.
   * The event must exist within the counter definitions.
   *
   * @param event_name Name of the event.
   * @return True, if the event could be added.
   */
  bool add(const std::string& event_name);

  /**
   * Add the specified events to the list of countered events.
   * The events must exist within the counter definitions.
   *
   * @param event_names List of names of the events.
   * @return True, if the events could be added.
   */
  bool add(std::vector<std::string>&& event_names);

  /**
   * Add the specified event to the list of countered performance events.
   * The event can be read "live" without stopping the counter (only x86 hardware).
   * The event must exist within the counter definitions.
   *
   * @param event_name Name of the event.
   */
  void add_live(std::string&& event_name) { add_live(event_name); }

  /**
   * Add the specified event to the list of countered performance events.
   * The event can be read "live" without stopping the counter (only x86 hardware).
   * The event must exist within the counter definitions.
   *
   * @param event_name Name of the event.
   */
  void add_live(const std::string& event_name);

  /**
   * Add the specified events to the list of countered performance events.
   * The events can be read "live" without stopping the counter (only x86 hardware).
   * The events must exist within the counter definitions.
   *
   * @param event_names List of event names.
   */
  void add_live(std::vector<std::string>&& event_names);

  /**
   * Add the specified events to the list of countered performance events.
   * The events must exist within the counter definitions.
   *
   * @param event_names List of names of the counted events.
   * @return True, if the events could be added.
   */
  bool add(const std::vector<std::string>& event_names);

  /**
   * Opens hardware performance counters.
   */
  void open();

  /**
   * Opens (if not already done) and starts recording performance counters.
   *
   * @return True, of the performance counters could be started.
   */
  bool start();

  /**
   * Stops and closes recording performance counters.
   */
  void stop();

  /**
   * Returns the result of the performance measurement.
   *
   * @param normalization Normalization value, default = 1.
   * @return List of event names and values.
   */
  [[nodiscard]] CounterResult result(std::uint64_t normalization = 1U) const;

  /**
   * Performs a live read for every group without stopping the counter and writes it into the result input/output.
   * The reason for having an output parameter is to not allocate any memory during a lightweight read.
   *
   * @param result Output parameter to write the result without allocating any memory.
   * @param normalization  Normalization value, default = 1.
   */
  void live_result(std::vector<double>& result, std::uint64_t normalization = 1U) const;

  /**
   * Performs a live read for every group without stopping the counter.
   *
   * @param counter_index Index of the counter to be read live.
   * @param normalization Normalization value, default = 1.
   * @return The live value of the counter.
   */
  [[nodiscard]] double live_result(std::uint64_t counter_index, std::uint64_t normalization = 1U) const;

  /**
   * @return Configuration of the counter.
   */
  [[nodiscard]] Config config() const noexcept { return _config; }

  /**
   * Update the configuration of the counter.
   *
   * @param config New config.
   */
  void config(Config config) noexcept { _config = config; }

private:
  /// List of event names and codes.
  const CounterDefinition& _counter_definitions;

  /// The configuration of counters (include user, kernel, etc.).
  Config _config;

  /// List of requested counters and metrics that are added to groups. This list is only to track the order and
  /// configuration of the user's requested events.
  std::vector<Event> _events;

  /// Counter groups holding performance counters that are started, stopped, and read.
  std::vector<Group> _groups;

  /// List of counters that are marked to be read "live" (without stopping) using the "rdpmc" instruction (only
  /// implemented on x86 hardware).
  std::vector<Counter> _live_counters;

  /// Flag indicating if the EventCounter was opened. Opens automatically on startup at the latest.
  bool _is_open{ false };

  /**
   * @return The number of opened (or to open) counters (groups or group leaders and live counters).
   */
  [[nodiscard]] std::size_t size() const noexcept { return _groups.size() + _live_counters.size(); }

  /**
   * Add the specified event to the list of counted performance events.
   * The event must exist within the counter definitions.
   *
   * @param event_name Name of the event.
   * @param event_config Configuration of the event.
   * @param is_shown_in_results Indicates if the counter should be exposed in the results.
   * @return True, if the event was added.
   */
  void add(std::string_view event_name, CounterConfig event_config, bool is_shown_in_results);

  /**
   * Searches for an event within the event list.
   *
   * @param event_name Name of the event.
   * @return Iterator of the event list.
   */
  [[nodiscard]] std::vector<Event>::iterator find_event(std::string_view event_name) noexcept;
};

class MultiEventCounterBase
{
public:
  MultiEventCounterBase() noexcept = default;
  virtual ~MultiEventCounterBase() = default;

  /**
   * Add the specified event to the list of countered performance events.
   * The event must exist within the counter definitions.
   *
   * @param event_name Name of the event.
   * @return True, if the event could be added.
   */
  bool add(std::string&& event_name);

  /**
   * Add the specified counter to the list of monitored performance counters.
   * The counter must exist within the counter definitions.
   *
   * @param counter_name Name of the counter.
   * @return True, if the counter could be added.
   */
  bool add(const std::string& counter_name) { return add(std::string{ counter_name }); }

  /**
   * Add the specified counters to the list of monitored performance counters.
   * The counters must exist within the counter definitions.
   *
   * @param counter_names List of names of the counters.
   * @return True, if the counters could be added.
   */
  bool add(std::vector<std::string>&& counter_names);

  /**
   * Add the specified counters to the list of monitored performance counters.
   * The counters must exist within the counter definitions.
   *
   * @param counter_names List of names of the counters.
   * @return True, if the counters could be added.
   */
  bool add(const std::vector<std::string>& counter_names);

  /**
   * Stops and closes recording performance counters.
   */
  void stop();

  /**
   * Returns the result of the performance measurement.
   *
   * @param normalization Normalization value, default = 1.
   * @return List of counter names and values.
   */
  [[nodiscard]] CounterResult result(std::uint64_t normalization = 1U) const;

protected:
  [[nodiscard]] virtual std::vector<EventCounter>& event_counters() noexcept = 0;
  [[nodiscard]] virtual const std::vector<EventCounter>& event_counters() const noexcept = 0;
};

class StartableMultiEventCounterBase : public MultiEventCounterBase
{
public:
  StartableMultiEventCounterBase() noexcept = default;
  ~StartableMultiEventCounterBase() override = default;

  /**
   * Opens and starts all event counters.
   *
   * @return True, of the event counters could be started.
   */
  bool start();
};

/**
 * Wrapper for EventCounter to record counters on different user-level threads.
 * Each thread can start/stop its own counter.
 * The results can be aggregated or queried for a specific thread.
 */
class MultiThreadEventCounter final : public MultiEventCounterBase
{
public:
  MultiThreadEventCounter(const CounterDefinition& counter_definition, std::uint16_t num_threads, Config config = {});

  MultiThreadEventCounter(EventCounter&& perf, std::uint16_t num_threads);

  MultiThreadEventCounter(const EventCounter& event_counter, const std::uint16_t num_threads)
    : MultiThreadEventCounter(perf::EventCounter{ event_counter }, num_threads)
  {
  }

  ~MultiThreadEventCounter() override = default;

  /**
   * Opens and starts recording performance counters for the given thread.
   *
   * @param thread_id Id of the thread.
   * @return True, of the performance counters could be started.
   */
  bool start(std::uint16_t thread_id) { return this->_thread_local_counter[thread_id].start(); }

  /**
   * Stops and closes recording performance counters.
   *
   * @param thread_id Id of the thread.
   */
  void stop(std::uint16_t thread_id) { this->_thread_local_counter[thread_id].stop(); }

  /**
   * Returns the result of the performance measurement for a given thread.
   *
   * @param thread_id Id of the thread.
   * @param normalization Normalization value, default = 1.
   * @return List of counter names and values.
   */
  [[nodiscard]] CounterResult result_of_thread(const std::uint16_t thread_id, std::uint64_t normalization = 1U) const
  {
    return _thread_local_counter[thread_id].result(normalization);
  }

private:
  std::vector<perf::EventCounter> _thread_local_counter;

  [[nodiscard]] std::vector<EventCounter>& event_counters() noexcept override { return _thread_local_counter; }
  [[nodiscard]] const std::vector<EventCounter>& event_counters() const noexcept override
  {
    return _thread_local_counter;
  }
};

/**
 * Wrapper for EventCounter to record counters on different process ids (i.e., linux thread ids).
 * ProcessIds / ThreadIds have to be specified. The counter can be started/stopped at once.
 * The results will be aggregated.
 */
class MultiProcessEventCounter final : public StartableMultiEventCounterBase
{
public:
  MultiProcessEventCounter(const CounterDefinition& counter_list, std::vector<pid_t>&& process_ids, Config config = {});

  MultiProcessEventCounter(EventCounter&& perf, std::vector<pid_t>&& process_ids);

  MultiProcessEventCounter(const EventCounter& event_counter, std::vector<pid_t>&& process_ids)
    : MultiProcessEventCounter(perf::EventCounter{ event_counter }, std::move(process_ids))
  {
  }

  ~MultiProcessEventCounter() override = default;

private:
  std::vector<perf::EventCounter> _process_local_counter;

  [[nodiscard]] std::vector<EventCounter>& event_counters() noexcept override { return _process_local_counter; }
  [[nodiscard]] const std::vector<EventCounter>& event_counters() const noexcept override
  {
    return _process_local_counter;
  }
};

/**
 * Wrapper for EventCounter to record counters on different CPU cores.
 * CPU ids have to be specified. The counter can be started/stopped at once.
 * The results will be aggregated.
 */
class MultiCoreEventCounter final : public StartableMultiEventCounterBase
{
public:
  MultiCoreEventCounter(const CounterDefinition& counter_definition,
                        std::vector<std::uint16_t>&& cpu_ids,
                        Config config = {});

  MultiCoreEventCounter(EventCounter&& perf, std::vector<std::uint16_t>&& cpu_ids);

  MultiCoreEventCounter(const EventCounter& event_counter, std::vector<std::uint16_t>&& cpu_ids)
    : MultiCoreEventCounter(perf::EventCounter{ event_counter }, std::move(cpu_ids))
  {
  }

  ~MultiCoreEventCounter() override = default;

private:
  std::vector<perf::EventCounter> _cpu_local_counter;

  [[nodiscard]] std::vector<EventCounter>& event_counters() noexcept override { return _cpu_local_counter; }
  [[nodiscard]] const std::vector<EventCounter>& event_counters() const noexcept override { return _cpu_local_counter; }
};
}