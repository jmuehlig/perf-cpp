#pragma once

#include "counter.h"
#include "counter_definition.h"
#include "util/graph.h"
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace perf {
/**
 * The RequestedEvent class stores information about events that will be recorded by the EventCounter and Sampler
 * The information include the group an event is scheduled to, the index within the group, the name, and a flag if the
 * event should be shown within the results (which is not true for events only needed for metrics). The EventCounter and
 * Sampler will have an ordered list of events, dictating the order the user requested the events to output the events
 * in exactly that order.
 */
class RequestedEvent
{
public:
  class ScheduledHardwareCounterGroup
  {
  public:
    explicit ScheduledHardwareCounterGroup(const std::uint8_t position) noexcept
      : _position(position)
    {
    }
    ScheduledHardwareCounterGroup(const std::uint8_t id, const std::uint8_t position) noexcept
      : _id(id)
      , _position(position)
    {
    }
    ~ScheduledHardwareCounterGroup() noexcept = default;

    [[nodiscard]] std::uint8_t id() const noexcept { return _id; }
    [[nodiscard]] std::uint8_t position() const noexcept { return _position; }

  private:
    std::uint8_t _id{ 0U };
    std::uint8_t _position;
  };

  enum class Type : std::uint8_t
  {
    HardwareEvent,
    Metric,
    TimeEvent
  };

  RequestedEvent(const std::optional<std::string_view> pmu_name,
                 const std::string_view event_name,
                 const bool is_shown_in_results,
                 const Type type,
                 const std::optional<ScheduledHardwareCounterGroup> scheduled_group) noexcept
    : _pmu_name(pmu_name)
    , _event_name(event_name)
    , _is_shown_in_results(is_shown_in_results)
    , _type(type)
    , _scheduled_hardware_counter_group(scheduled_group)
  {
  }

  ~RequestedEvent() = default;

  [[nodiscard]] std::optional<std::string_view> pmu_name() const noexcept { return _pmu_name; }
  [[nodiscard]] std::string_view event_name() const noexcept { return _event_name; }
  [[nodiscard]] bool is_hardware_event() const noexcept { return _type == Type::HardwareEvent; }
  [[nodiscard]] bool is_metric() const noexcept { return _type == Type::Metric; }
  [[nodiscard]] bool is_time_event() const noexcept { return _type == Type::TimeEvent; }
  [[nodiscard]] bool is_shown_in_results() const noexcept { return _is_shown_in_results; }
  [[nodiscard]] std::optional<ScheduledHardwareCounterGroup> scheduled_group() const noexcept
  {
    return _scheduled_hardware_counter_group;
  }

  void is_shown_in_results(const bool is_shown_in_results) noexcept { _is_shown_in_results = is_shown_in_results; }

private:
  /// Name of the PMU for hardware events. May be nullopt for metrics and time events.
  std::optional<std::string_view> _pmu_name;

  /// Name of the event (references a string in the CounterDefinition).
  std::string_view _event_name;

  /// Indicates that the event is included into results. Some events are "only" requested by metrics and are only
  /// needed for calculating them but are not requested by the user.
  bool _is_shown_in_results;

  /// Type, e.g., hardware event, metric, or time event.
  Type _type;

  /// Position (hardware counter group id and position within that group) the event is scheduled to.
  std::optional<ScheduledHardwareCounterGroup> _scheduled_hardware_counter_group{ std::nullopt };
};

/**
 * The RequestedEventSet manages events that are requested by the user or metrics and takes care that no event is added
 * more than one time (e.g., by the user and a metric).
 */
class RequestedEventSet
{
public:
  RequestedEventSet() = default;
  RequestedEventSet(RequestedEventSet&&) noexcept = default;
  RequestedEventSet(const RequestedEventSet&) noexcept = default;
  explicit RequestedEventSet(const std::size_t capacity) { _requested_events.reserve(capacity); }

  ~RequestedEventSet() = default;

  /**
   * Appends an event to the event set, if not present.
   * The event will be interpreted as a hardware event (since it is scheduled to a group) and marked as shown in
   * results.
   *
   * @param pmu_name Name of the PMU.
   * @param event_name Name of the event.
   * @param in_group_position Position of the event within the group.
   * @return True, if the event was added. False, if the event was already in the event set.
   */
  bool add(const std::string_view pmu_name, const std::string_view event_name, const std::uint8_t in_group_position)
  {
    return add(pmu_name,
               event_name,
               true,
               RequestedEvent::Type::HardwareEvent,
               RequestedEvent::ScheduledHardwareCounterGroup{ in_group_position });
  }

  /**
   * Appends an event to the event set, if not present.
   * If present and the event was marked as hidden for the results, but is_shown_in_results is true, the visibility will
   * change to true. Since the event is scheduled to a hardware counter group, the event will be interpreted as a
   * hardware event.
   *
   * @param pmu_name Name of the PMU.
   * @param event_name Name of the event.
   * @param is_shown_in_results True, if the event should be visible in the results.
   * @param group_id Id of the group the event was scheduled to.
   * @param in_group_position Position of the event within the group.
   * @return True, if the event was added. False, if the event was already in the event set.
   */
  bool add(const std::string_view pmu_name,
           const std::string_view event_name,
           const bool is_shown_in_results,
           const std::uint8_t group_id,
           const std::uint8_t in_group_position)
  {
    return add(pmu_name,
               event_name,
               is_shown_in_results,
               RequestedEvent::Type::HardwareEvent,
               RequestedEvent::ScheduledHardwareCounterGroup{ group_id, in_group_position });
  }

  /**
   * Appends an event to the event set, if not present.
   * The event will be interpreted as a metric (since it is not scheduled to any hardware counter group).
   *
   * @param pmu_name Name of the PMU.
   * @param event_name Name of the event.
   * @param type Type of the event (e.g., metric or time)
   * @param is_shown_in_results True, if the event should be visible in the results.
   * @return True, if the event was added. False, if the event was already in the event set.
   */
  bool add(std::optional<std::string_view> pmu_name,
           const std::string_view event_name,
           const RequestedEvent::Type type,
           const bool is_shown_in_results)
  {
    return add(pmu_name, event_name, is_shown_in_results, type, std::nullopt);
  }

  /**
   * Appends an event to the event set, if not present.
   * The event will be interpreted as a metric (since it is not scheduled to any hardware counter group).
   *
   * @param event_name Name of the event.
   * @param type Type of the event (e.g., metric or time)
   * @param is_shown_in_results True, if the event should be visible in the results.
   * @return True, if the event was added. False, if the event was already in the event set.
   */
  bool add(const std::string_view event_name, const RequestedEvent::Type type, const bool is_shown_in_results)
  {
    return add(std::nullopt, event_name, is_shown_in_results, type, std::nullopt);
  }

  /**
   * Checks if the event is present in the requested set. If so, set the visibility.
   * Otherwise, return false, indicating that the event needs to be added.
   *
   * @param pmu_name Name of the PMU.
   * @param event_name Name of the event.
   * @param is_shown_in_results The visibility of the event.
   * @return True, if the event is present.
   */
  [[nodiscard]] bool adjust_visibility_if_present(std::optional<std::string_view> pmu_name,
                                                  std::string_view event_name,
                                                  bool is_shown_in_results);

  /**
   * Constructs a CounterResult for a given CounterResult that uses hardware events only.
   * The returned CounterResult will include metrics added to the requested event set.
   *
   * @param counter_definition Counter definition.
   * @param hardware_events_result Result containing only hardware events.
   * @param normalization Value to normalize the (non-metric) results to.
   * @return CounterResult including all events requested as visible; hardware events and metrics.
   */
  [[nodiscard]] CounterResult result(const CounterDefinition& counter_definition,
                                     CounterResult&& hardware_events_result,
                                     std::uint64_t normalization) const;

  /**
   * @return True, if no event was added.
   */
  [[nodiscard]] bool empty() const noexcept { return _requested_events.empty(); }

  /**
   * @return The size of the set.
   */
  [[nodiscard]] std::size_t size() const noexcept { return _requested_events.size(); }

  [[nodiscard]] std::vector<RequestedEvent>::iterator begin() noexcept { return _requested_events.begin(); }
  [[nodiscard]] std::vector<RequestedEvent>::const_iterator begin() const noexcept
  {
    return _requested_events.cbegin();
  }

  [[nodiscard]] std::vector<RequestedEvent>::iterator end() noexcept { return _requested_events.end(); }
  [[nodiscard]] std::vector<RequestedEvent>::const_iterator end() const noexcept { return _requested_events.cend(); }

private:
  /// List dictating the order of events in the set.
  std::vector<RequestedEvent> _requested_events;

  /**
   * Appends an event to the event set, if not present.
   * If present and the event was marked as hidden for the results, but is_shown_in_results is true, the visibility will
   * change to true.
   *
   * @param pmu_name Name of the PMU.
   * @param event_name Name of the event.
   * @param is_shown_in_results True, if the event should be visible in the results.
   * @param type Type, e.g., hardware event, metric, or time event.
   * @param scheduled_group Group id and position within the group the hardware event is scheduled to (if the event is a
   * hardware event).
   * @return True, if the event was added. False, if the event was already in the event set.
   */
  bool add(std::optional<std::string_view> pmu_name,
           std::string_view event_name,
           bool is_shown_in_results,
           RequestedEvent::Type type,
           std::optional<RequestedEvent::ScheduledHardwareCounterGroup> scheduled_group);

  /**
   * Build a directed dependency graph for all metrics available in the requested event set.
   *
   * @param counter_definition Counter definition to lookup metrics.
   * @return A directed graph, connecting dependent metrics.
   */
  [[nodiscard]] util::DirectedGraph<std::string_view> build_metric_graph(const CounterDefinition& counter_definition) const;
};
}