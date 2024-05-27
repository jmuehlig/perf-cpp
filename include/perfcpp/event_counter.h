#pragma once

#include "config.h"
#include "counter.h"
#include "counter_definition.h"
#include "group.h"
#include <chrono>
#include <optional>
#include <string>
#include <vector>

namespace perf {
class EventCounter
{
  friend class EventCounterMT;

private:
  class Event
  {
  public:
    explicit Event(std::string&& name) noexcept
      : _name(std::move(name))
      , _is_hidden(false)
      , _is_counter(false)
      , _group_id(0U)
      , _in_group_id(0U)
    {
    }

    Event(std::string&& name,
          const bool is_hidden,
          const std::uint8_t group_id,
          const std::uint8_t in_group_id) noexcept
      : _name(std::move(name))
      , _is_hidden(is_hidden)
      , _is_counter(true)
      , _group_id(group_id)
      , _in_group_id(in_group_id)
    {
    }

    ~Event() = default;

    [[nodiscard]] const std::string& name() const noexcept { return _name; }
    [[nodiscard]] bool is_counter() const noexcept { return _is_counter; }
    [[nodiscard]] bool is_hidden() const noexcept { return _is_hidden; }
    [[nodiscard]] std::uint8_t group_id() const noexcept { return _group_id; }
    [[nodiscard]] std::uint8_t in_group_id() const noexcept { return _in_group_id; }

    void is_hidden(const bool is_hidden) noexcept { _is_hidden = is_hidden; }

  private:
    std::string _name;
    bool _is_counter;
    bool _is_hidden;
    std::uint8_t _group_id{ 0U };
    std::uint8_t _in_group_id{ 0U };
  };

public:
  explicit EventCounter(const CounterDefinition& counter_list, Config config = {})
    : _counter_definitions(counter_list)
    , _config(config)
  {
  }
  EventCounter(EventCounter&&) noexcept = default;
  EventCounter(const EventCounter&) = default;

  ~EventCounter() = default;

  /**
   * Add the specified counter to the list of monitored performance counters.
   * The counter must exist within the counter definitions.
   *
   * @param counter_name Name of the counter.
   * @return True, if the counter could be added.
   */
  bool add(std::string&& counter_name);

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
   * Opens and starts recording performance counters.
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
   * @return List of counter names and values.
   */
  [[nodiscard]] CounterResult result(std::uint64_t normalization = 1U) const;

private:
  const CounterDefinition& _counter_definitions;

  Config _config;

  /// List of requested counters and metrics.
  std::vector<Event> _counters;

  /// Real counters to measure.
  std::vector<Group> _groups;

  /**
   * Add the specified counter to the list of monitored performance counters.
   * The counters must exist within the counter definitions.
   *
   * @param counter_name Name of the counter.
   * @param counter Configuration of the counter.
   * @param is_hidden Indicates if the counter should be exposed in the results.
   * @return True, if the counter was added.
   */
  bool add(std::string&& counter_name, CounterConfig counter, bool is_hidden);
};

class EventCounterMT
{
public:
  EventCounterMT(EventCounter&& perf, std::uint16_t num_threads);

  EventCounterMT(const EventCounter& event_counter, const std::uint16_t num_threads)
    : EventCounterMT(perf::EventCounter{ event_counter }, num_threads)
  {
  }

  ~EventCounterMT() = default;

  /**
   * Opens and starts recording performance counters for the given thread.
   *
   * @param thread_id Id of the thread.
   * @return True, of the performance counters could be started.
   */
  bool start(std::uint16_t thread_id);

  /**
   * Stops and closes recording performance counters.
   *
   * @param thread_id Id of the thread.
   */
  void stop(std::uint16_t thread_id);

  /**
   * Returns the result of the performance measurement.
   *
   * @param normalization Normalization value, default = 1.
   * @return List of counter names and values.
   */
  [[nodiscard]] CounterResult result(std::uint64_t normalization = 1U) const;

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
};
}