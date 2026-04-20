#include <catch2/catch_test_macros.hpp>
#include <perfcpp/counter/requested_event.hpp>
#include <perfcpp/counter/result.hpp>
#include <perfcpp/counter_definition.hpp>

TEST_CASE("empty RequestedEventSet", "[RequestedEventSet]")
{
  auto event_set = perf::RequestedEventSet{};
  auto counter_definition = perf::CounterDefinition{};

  SECTION("newly created event set is empty")
  {
    REQUIRE(event_set.empty());
    REQUIRE(event_set.size() == 0U);
    REQUIRE(event_set.begin() == event_set.end());
  }

  SECTION("event set with reserved capacity is still empty")
  {
    auto event_set_with_capacity = perf::RequestedEventSet{ 10U };

    REQUIRE(event_set_with_capacity.empty());
    REQUIRE(event_set_with_capacity.size() == 0U);
    REQUIRE(event_set_with_capacity.begin() == event_set_with_capacity.end());
  }

  SECTION("result with empty hardware_events_result should be empty")
  {
    auto empty_hardware_result = perf::CounterResult{};

    auto result = event_set.result(counter_definition, std::move(empty_hardware_result), 1U);

    REQUIRE(result.begin() == result.end());
    REQUIRE_FALSE(result.get("instructions").has_value());
    REQUIRE_FALSE(result.get("cycles").has_value());
  }

  SECTION("result with non-empty hardware_events_result but no requested events should be empty")
  {
    auto hardware_result =
      perf::CounterResult{ std::vector<std::pair<std::string_view, double>>{ std::make_pair("instructions", 1000.0) } };

    auto result = event_set.result(counter_definition, std::move(hardware_result), 1U);

    REQUIRE(result.begin() == result.end());
    REQUIRE_FALSE(result.get("instructions").has_value());
    REQUIRE_FALSE(result.get("cycles").has_value());
  }

  SECTION("result with reserved capacity - empty hardware_events_result should be empty")
  {
    auto event_set_with_capacity = perf::RequestedEventSet{ 10U };
    auto empty_hardware_result = perf::CounterResult{};

    auto result = event_set_with_capacity.result(counter_definition, std::move(empty_hardware_result), 1U);

    REQUIRE(result.begin() == result.end());
    REQUIRE_FALSE(result.get("instructions").has_value());
    REQUIRE_FALSE(result.get("cycles").has_value());
  }

  SECTION("result with reserved capacity - non-empty hardware_events_result but no requested events should be empty")
  {
    auto event_set_with_capacity = perf::RequestedEventSet{ 10U };
    auto hardware_result =
      perf::CounterResult{ std::vector<std::pair<std::string_view, double>>{ std::make_pair("instructions", 1000.0) } };

    auto result = event_set_with_capacity.result(counter_definition, std::move(hardware_result), 1U);

    REQUIRE(result.begin() == result.end());
    REQUIRE_FALSE(result.get("instructions").has_value());
    REQUIRE_FALSE(result.get("cycles").has_value());
  }
}

TEST_CASE("RequestedEventSet with requested events", "[RequestedEventSet]")
{
  auto counter_definition = perf::CounterDefinition{};

  SECTION("result contains requested event when present in hardware result")
  {
    auto event_set = perf::RequestedEventSet{};

    /// Add the "instructions" event as a hardware event
    REQUIRE(
      event_set.add(perf::RequestedEvent{ "cpu", "instructions", true, perf::RequestedEvent::Type::HardwareEvent }));

    /// Verify the event set is no longer empty
    REQUIRE_FALSE(event_set.empty());
    REQUIRE(event_set.size() == 1U);

    /// Create hardware result containing instructions
    auto hardware_result =
      perf::CounterResult{ std::vector<std::pair<std::string_view, double>>{ std::make_pair("instructions", 1000.0) } };

    auto result = event_set.result(counter_definition, std::move(hardware_result), 1U);

    /// The result should contain the requested instructions event
    REQUIRE(result.get("instructions").has_value());
    REQUIRE(result.get("instructions").value() == 1000.0);

    /// But should not contain unrequested events
    REQUIRE_FALSE(result.get("cycles").has_value());

    /// Verify iterator access
    REQUIRE(result.begin() != result.end());
    auto it = result.begin();
    REQUIRE(it->first == "instructions");
    REQUIRE(it->second == 1000.0);
    ++it;
    REQUIRE(it == result.end());
  }

  SECTION("result is empty when requested event not in hardware result")
  {
    auto event_set = perf::RequestedEventSet{};

    /// Add the "instructions" event as a hardware event
    REQUIRE(
      event_set.add(perf::RequestedEvent{ "cpu", "instructions", true, perf::RequestedEvent::Type::HardwareEvent }));

    /// Create hardware result NOT containing instructions
    auto hardware_result =
      perf::CounterResult{ std::vector<std::pair<std::string_view, double>>{ std::make_pair("cycles", 2000.0) } };

    auto result = event_set.result(counter_definition, std::move(hardware_result), 1U);

    /// The result should be empty since requested event is not in hardware result
    REQUIRE(result.begin() == result.end());
    REQUIRE_FALSE(result.get("instructions").has_value());
    REQUIRE_FALSE(result.get("cycles").has_value());
  }

  SECTION("result respects normalization for hardware events")
  {
    auto event_set = perf::RequestedEventSet{};

    /// Add the "instructions" event as a hardware event
    REQUIRE(
      event_set.add(perf::RequestedEvent{ "cpu", "instructions", true, perf::RequestedEvent::Type::HardwareEvent }));

    /// Create hardware result containing instructions
    auto hardware_result =
      perf::CounterResult{ std::vector<std::pair<std::string_view, double>>{ std::make_pair("instructions", 1000.0) } };

    auto result = event_set.result(counter_definition, std::move(hardware_result), 10U);

    /// The result should be normalized (1000.0 / 10 = 100.0)
    REQUIRE(result.get("instructions").has_value());
    REQUIRE(result.get("instructions").value() == 100.0);
  }
}

TEST_CASE("RequestedEvent construction", "[RequestedEvent]")
{
  SECTION("pmu name and event name")
  {
    const auto event = perf::RequestedEvent{ "cpu", "instructions", true, perf::RequestedEvent::Type::HardwareEvent };

    REQUIRE(event.pmu_name().has_value());
    REQUIRE(event.pmu_name().value() == "cpu");
    REQUIRE(event.event_name() == "instructions");
  }

  SECTION("no pmu name")
  {
    const auto event = perf::RequestedEvent{ "instructions-per-cycle", true, perf::RequestedEvent::Type::Metric };

    REQUIRE_FALSE(event.pmu_name().has_value());
    REQUIRE(event.event_name() == "instructions-per-cycle");
  }

  SECTION("group id and position via constructor")
  {
    const auto event = perf::RequestedEvent{ "cpu", "cycles", true, std::uint8_t{ 1U }, std::uint8_t{ 2U } };

    REQUIRE(event.scheduled_group().has_value());
    REQUIRE(event.scheduled_group()->id() == 1U);
    REQUIRE(event.scheduled_group()->position() == 2U);
  }

  SECTION("group id and position short constructor defaults to visible")
  {
    const auto event = perf::RequestedEvent{ "cpu", "cycles", std::uint8_t{ 0U }, std::uint8_t{ 0U } };

    REQUIRE(event.is_shown_in_results());
  }
}

TEST_CASE("RequestedEvent type predicates", "[RequestedEvent]")
{
  SECTION("hardware event")
  {
    const auto event = perf::RequestedEvent{ "cpu", "instructions", true, perf::RequestedEvent::Type::HardwareEvent };

    REQUIRE(event.is_hardware_event());
    REQUIRE_FALSE(event.is_metric());
    REQUIRE_FALSE(event.is_time_event());
  }

  SECTION("metric")
  {
    const auto event = perf::RequestedEvent{ "instructions-per-cycle", true, perf::RequestedEvent::Type::Metric };

    REQUIRE(event.is_metric());
    REQUIRE_FALSE(event.is_hardware_event());
    REQUIRE_FALSE(event.is_time_event());
  }

  SECTION("time event")
  {
    const auto event = perf::RequestedEvent{ "seconds", true, perf::RequestedEvent::Type::TimeEvent };

    REQUIRE(event.is_time_event());
    REQUIRE_FALSE(event.is_hardware_event());
    REQUIRE_FALSE(event.is_metric());
  }
}

TEST_CASE("RequestedEvent visibility", "[RequestedEvent]")
{
  SECTION("visible on construction")
  {
    const auto event = perf::RequestedEvent{ "cpu", "instructions", true, perf::RequestedEvent::Type::HardwareEvent };
    REQUIRE(event.is_shown_in_results());
  }

  SECTION("hidden on construction")
  {
    const auto event = perf::RequestedEvent{ "cpu", "instructions", false, perf::RequestedEvent::Type::HardwareEvent };
    REQUIRE_FALSE(event.is_shown_in_results());
  }

  SECTION("shown_in_results mutator flips to true")
  {
    auto event = perf::RequestedEvent{ "cpu", "instructions", false, perf::RequestedEvent::Type::HardwareEvent };
    event.shown_in_results(true);
    REQUIRE(event.is_shown_in_results());
  }

  SECTION("shown_in_results mutator flips to false")
  {
    auto event = perf::RequestedEvent{ "cpu", "instructions", true, perf::RequestedEvent::Type::HardwareEvent };
    event.shown_in_results(false);
    REQUIRE_FALSE(event.is_shown_in_results());
  }
}

TEST_CASE("RequestedEvent scheduled group", "[RequestedEvent]")
{
  SECTION("not set by default")
  {
    const auto event = perf::RequestedEvent{ "cpu", "instructions", true, perf::RequestedEvent::Type::HardwareEvent };
    REQUIRE_FALSE(event.scheduled_group().has_value());
  }

  SECTION("set via mutator")
  {
    auto event = perf::RequestedEvent{ "cpu", "instructions", true, perf::RequestedEvent::Type::HardwareEvent };
    event.scheduled_group(std::uint8_t{ 2U }, std::uint8_t{ 3U });

    REQUIRE(event.scheduled_group().has_value());
    REQUIRE(event.scheduled_group()->id() == 2U);
    REQUIRE(event.scheduled_group()->position() == 3U);
  }

  SECTION("ScheduledHardwareCounterGroup two-arg constructor")
  {
    const auto group = perf::RequestedEvent::ScheduledHardwareCounterGroup{ std::uint8_t{ 4U }, std::uint8_t{ 5U } };

    REQUIRE(group.id() == 4U);
    REQUIRE(group.position() == 5U);
  }

  SECTION("ScheduledHardwareCounterGroup single-arg constructor defaults id to zero")
  {
    const auto group = perf::RequestedEvent::ScheduledHardwareCounterGroup{ std::uint8_t{ 7U } };

    REQUIRE(group.id() == 0U);
    REQUIRE(group.position() == 7U);
  }
}

TEST_CASE("RequestedEventSet add deduplication", "[RequestedEventSet]")
{
  SECTION("first add returns true")
  {
    auto event_set = perf::RequestedEventSet{};
    REQUIRE(event_set.add(perf::RequestedEvent{ "cpu", "instructions", true, perf::RequestedEvent::Type::HardwareEvent }));
  }

  SECTION("duplicate returns false and size stays at one")
  {
    auto event_set = perf::RequestedEventSet{};
    REQUIRE(event_set.add(perf::RequestedEvent{ "cpu", "instructions", true, perf::RequestedEvent::Type::HardwareEvent }));
    REQUIRE_FALSE(event_set.add(perf::RequestedEvent{ "cpu", "instructions", true, perf::RequestedEvent::Type::HardwareEvent }));
    REQUIRE(event_set.size() == 1U);
  }

  SECTION("same event name different pmu is not a duplicate")
  {
    auto event_set = perf::RequestedEventSet{};
    REQUIRE(event_set.add(perf::RequestedEvent{ "cpu", "instructions", true, perf::RequestedEvent::Type::HardwareEvent }));
    REQUIRE(event_set.add(perf::RequestedEvent{ "uncore_imc_0", "instructions", true, perf::RequestedEvent::Type::HardwareEvent }));
    REQUIRE(event_set.size() == 2U);
  }

  SECTION("add with group_id and position overload sets scheduled group on event")
  {
    auto event_set = perf::RequestedEventSet{};
    auto event = perf::RequestedEvent{ "cpu", "instructions", true, perf::RequestedEvent::Type::HardwareEvent };
    event_set.add(event, std::uint8_t{ 1U }, std::uint8_t{ 2U });

    REQUIRE(event.scheduled_group().has_value());
    REQUIRE(event.scheduled_group()->id() == 1U);
    REQUIRE(event.scheduled_group()->position() == 2U);
  }
}

TEST_CASE("RequestedEventSet adjust_visibility_if_present", "[RequestedEventSet]")
{
  auto counter_definition = perf::CounterDefinition{};

  SECTION("returns false when event not in set")
  {
    auto event_set = perf::RequestedEventSet{};
    REQUIRE_FALSE(event_set.adjust_visibility_if_present("cpu", "instructions", true));
  }

  SECTION("returns true when event is present")
  {
    auto event_set = perf::RequestedEventSet{};
    event_set.add(perf::RequestedEvent{ "cpu", "instructions", true, perf::RequestedEvent::Type::HardwareEvent });
    REQUIRE(event_set.adjust_visibility_if_present("cpu", "instructions", true));
  }

  SECTION("upgrades hidden event to visible")
  {
    auto event_set = perf::RequestedEventSet{};
    event_set.add(perf::RequestedEvent{ "cpu", "instructions", false, perf::RequestedEvent::Type::HardwareEvent });

    REQUIRE(event_set.adjust_visibility_if_present("cpu", "instructions", true));

    auto hardware_result =
      perf::CounterResult{ std::vector<std::pair<std::string_view, double>>{ { "instructions", 1000.0 } } };
    const auto result = event_set.result(counter_definition, std::move(hardware_result), 1U);

    REQUIRE(result.get("instructions").has_value());
    REQUIRE(result.get("instructions").value() == 1000.0);
  }

  SECTION("does not downgrade visible event to hidden")
  {
    auto event_set = perf::RequestedEventSet{};
    event_set.add(perf::RequestedEvent{ "cpu", "instructions", true, perf::RequestedEvent::Type::HardwareEvent });

    REQUIRE(event_set.adjust_visibility_if_present("cpu", "instructions", false));

    auto hardware_result =
      perf::CounterResult{ std::vector<std::pair<std::string_view, double>>{ { "instructions", 1000.0 } } };
    const auto result = event_set.result(counter_definition, std::move(hardware_result), 1U);

    REQUIRE(result.get("instructions").has_value());
  }
}

TEST_CASE("RequestedEventSet result ordering and visibility", "[RequestedEventSet]")
{
  auto counter_definition = perf::CounterDefinition{};

  SECTION("hidden event excluded from result")
  {
    auto event_set = perf::RequestedEventSet{};
    event_set.add(perf::RequestedEvent{ "cpu", "instructions", false, perf::RequestedEvent::Type::HardwareEvent });

    auto hardware_result =
      perf::CounterResult{ std::vector<std::pair<std::string_view, double>>{ { "instructions", 1000.0 } } };
    const auto result = event_set.result(counter_definition, std::move(hardware_result), 1U);

    REQUIRE_FALSE(result.get("instructions").has_value());
  }

  SECTION("result order matches add order not hardware result order")
  {
    auto event_set = perf::RequestedEventSet{};
    event_set.add(perf::RequestedEvent{ "cpu", "cycles", true, perf::RequestedEvent::Type::HardwareEvent });
    event_set.add(perf::RequestedEvent{ "cpu", "instructions", true, perf::RequestedEvent::Type::HardwareEvent });
    event_set.add(perf::RequestedEvent{ "cpu", "cache-misses", true, perf::RequestedEvent::Type::HardwareEvent });

    /// Hardware result has the events in reverse order of how they were added.
    auto hardware_result = perf::CounterResult{
      std::vector<std::pair<std::string_view, double>>{ { "cache-misses", 3.0 }, { "instructions", 2.0 }, { "cycles", 1.0 } }
    };
    const auto result = event_set.result(counter_definition, std::move(hardware_result), 1U);

    REQUIRE(result.size() == 3U);
    auto it = result.begin();
    REQUIRE(it->first == "cycles");
    ++it;
    REQUIRE(it->first == "instructions");
    ++it;
    REQUIRE(it->first == "cache-misses");
  }
}

TEST_CASE("RequestedEventSet result normalization", "[RequestedEventSet]")
{
  auto counter_definition = perf::CounterDefinition{};

  SECTION("hardware event is normalized")
  {
    auto event_set = perf::RequestedEventSet{};
    event_set.add(perf::RequestedEvent{ "cpu", "instructions", true, perf::RequestedEvent::Type::HardwareEvent });

    auto hardware_result =
      perf::CounterResult{ std::vector<std::pair<std::string_view, double>>{ { "instructions", 1000.0 } } };
    const auto result = event_set.result(counter_definition, std::move(hardware_result), 10U);

    REQUIRE(result.get("instructions").has_value());
    REQUIRE(result.get("instructions").value() == 100.0);
  }

  SECTION("time event is normalized")
  {
    auto event_set = perf::RequestedEventSet{};
    event_set.add(perf::RequestedEvent{ "nanoseconds", true, perf::RequestedEvent::Type::TimeEvent });

    auto hardware_result =
      perf::CounterResult{ std::vector<std::pair<std::string_view, double>>{ { "nanoseconds", 5000.0 } } };
    const auto result = event_set.result(counter_definition, std::move(hardware_result), 10U);

    REQUIRE(result.get("nanoseconds").has_value());
    REQUIRE(result.get("nanoseconds").value() == 500.0);
  }

  SECTION("metric is not normalized")
  {
    auto event_set = perf::RequestedEventSet{};
    event_set.add(perf::RequestedEvent{ "instructions-per-cycle", true, perf::RequestedEvent::Type::Metric });

    /// IPC = 2000 / 1000 = 2.0; normalization must not be applied to the metric result.
    auto hardware_result = perf::CounterResult{
      std::vector<std::pair<std::string_view, double>>{ { "instructions", 2000.0 }, { "cycles", 1000.0 } }
    };
    const auto result = event_set.result(counter_definition, std::move(hardware_result), 10U);

    REQUIRE(result.get("instructions-per-cycle").has_value());
    REQUIRE(result.get("instructions-per-cycle").value() == 2.0);
  }
}