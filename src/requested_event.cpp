#include <perfcpp/requested_event.h>

bool
perf::RequestedEventSet::add(const std::string_view event_name,
                             const bool is_shown_in_results,
                             RequestedEvent::Type type,
                             std::optional<RequestedEvent::ScheduledHardwareCounterGroup> scheduled_group)
{
  /// If the event is not already added (in that case adjust_visibility_if_present() will return false), add it.
  /// If the event is already in the set, adjust_visibility_if_present() will adjust the visibility to true, if
  /// is_shown_in_results is true.
  if (!this->adjust_visibility_if_present(event_name, is_shown_in_results)) {
    this->_requested_events.emplace_back(event_name, is_shown_in_results, type, scheduled_group);
    return true;
  }

  return false;
}

bool
perf::RequestedEventSet::adjust_visibility_if_present(const std::string_view event_name, const bool is_shown_in_results)
{
  auto iterator = std::find_if(this->_requested_events.begin(),
                               this->_requested_events.end(),
                               [event_name](const auto& event) { return event.name() == event_name; });

  /// If the event is not in the set, notify the caller that the event needs to be added.
  if (iterator == this->_requested_events.end()) {
    return false;
  }

  /// If the event should be included into the results, mark it accordingly.
  if (is_shown_in_results) {
    iterator->is_shown_in_results(true);
  }

  return true;
}

perf::CounterResult
perf::RequestedEventSet::result(const perf::CounterDefinition& counter_definition,
                                perf::CounterResult&& hardware_events_result, const std::uint64_t normalization) const
{
  auto counter_results = std::vector<std::pair<std::string_view, double>>{};

  /// Add all the events that are requested as visible in the results.
  for (const auto& requested_event : this->_requested_events) {
    if (requested_event.is_shown_in_results()) {
      /// Hardware events can be copied directly.
      if (requested_event.is_hardware_event() || requested_event.is_time_event()) {
        if (const auto hardware_event_value = hardware_events_result.get(requested_event.name());
            hardware_event_value.has_value()) {
          counter_results.emplace_back(requested_event.name(), hardware_event_value.value() / double(normalization));
        }
      }

      /// Metrics need to be calculated by multiple hardware events.
      else if (requested_event.is_metric()) {
        if (auto metric = counter_definition.metric(requested_event.name()); metric.has_value()) {
          if (const auto calculated_metric_value = std::get<1>(metric.value()).calculate(hardware_events_result);
              calculated_metric_value.has_value()) {
            counter_results.emplace_back(requested_event.name(), calculated_metric_value.value());
          }
        }
      }
    }
  }

  return CounterResult{ std::move(counter_results) };
}