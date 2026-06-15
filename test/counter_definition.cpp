#include <catch2/catch_test_macros.hpp>
#include <perfcpp/counter_definition.hpp>

TEST_CASE("supports", "[CounterDefinition]")
{
  const auto definition = perf::CounterDefinition{};

  SECTION("built-in hardware events")
  {
    REQUIRE(definition.supports("instructions"));
    REQUIRE(definition.supports("cycles"));
    REQUIRE(definition.supports("cache-misses"));
    REQUIRE(definition.supports("cache-references"));
    REQUIRE(definition.supports("branches"));
    REQUIRE(definition.supports("branch-misses"));
  }

  SECTION("built-in software events")
  {
    REQUIRE(definition.supports("cpu-clock"));
    REQUIRE(definition.supports("task-clock"));
    REQUIRE(definition.supports("page-faults"));
    REQUIRE(definition.supports("context-switches"));
  }

  SECTION("built-in time events")
  {
    REQUIRE(definition.supports("seconds"));
    REQUIRE(definition.supports("milliseconds"));
    REQUIRE(definition.supports("microseconds"));
    REQUIRE(definition.supports("nanoseconds"));
  }

  SECTION("built-in metrics")
  {
    REQUIRE(definition.supports("cycles-per-instruction"));
    REQUIRE(definition.supports("instructions-per-cycle"));
    REQUIRE(definition.supports("cache-hit-ratio"));
    REQUIRE(definition.supports("cache-miss-ratio"));
  }

  SECTION("unknown event")
  {
    REQUIRE_FALSE(definition.supports("does-not-exist"));
    REQUIRE_FALSE(definition.supports(""));
  }

  SECTION("user-added event")
  {
    auto extended = perf::CounterDefinition{};
    REQUIRE_FALSE(extended.supports("my-custom-event"));

    extended.add("my-custom-event", 0x1234);
    REQUIRE(extended.supports("my-custom-event"));
  }

  SECTION("user-added metric")
  {
    auto extended = perf::CounterDefinition{};
    REQUIRE_FALSE(extended.supports("my-custom-metric"));

    extended.add("my-custom-metric", "cycles / instructions");
    REQUIRE(extended.supports("my-custom-metric"));
  }

  SECTION("mutually dependent metrics do not stack-overflow")
  {
    auto extended = perf::CounterDefinition{};

    /// metric_a references metric_b and vice versa — a cycle.
    /// Metric names in formulas must not contain hyphens: the tokenizer only accepts [alnum, _, .].
    extended.add("metric_a", "metric_b + cycles");
    extended.add("metric_b", "metric_a + instructions");

    /// Must return false cleanly, not crash.
    REQUIRE_FALSE(extended.supports("metric_a"));
    REQUIRE_FALSE(extended.supports("metric_b"));
  }

  SECTION("diamond metric dependency is not mistaken for a cycle")
  {
    auto extended = perf::CounterDefinition{};

    /// base_metric is a dependency of composite via two independent paths — a diamond, not a cycle.
    extended.add("base_metric", "cycles + instructions");
    extended.add("composite", "base_metric + base_metric");

    /// The visited set must not block the second traversal of base_metric.
    REQUIRE(extended.supports("composite"));
  }
}

TEST_CASE("is_available", "[CounterDefinition]")
{
  const auto definition = perf::CounterDefinition{};

  SECTION("built-in hardware events")
  {
    REQUIRE(definition.is_available("instructions"));
    REQUIRE(definition.is_available("cycles"));
    REQUIRE(definition.is_available("cache-misses"));
    REQUIRE(definition.is_available("cache-references"));
    REQUIRE(definition.is_available("branches"));
    REQUIRE(definition.is_available("branch-misses"));
  }

  SECTION("built-in software events")
  {
    REQUIRE(definition.is_available("cpu-clock"));
    REQUIRE(definition.is_available("task-clock"));
    REQUIRE(definition.is_available("page-faults"));
    REQUIRE(definition.is_available("context-switches"));
  }

  SECTION("built-in time events")
  {
    REQUIRE(definition.is_available("seconds"));
    REQUIRE(definition.is_available("milliseconds"));
    REQUIRE(definition.is_available("microseconds"));
    REQUIRE(definition.is_available("nanoseconds"));
  }

  SECTION("built-in metrics")
  {
    REQUIRE(definition.is_available("cycles-per-instruction"));
    REQUIRE(definition.is_available("instructions-per-cycle"));
    REQUIRE(definition.is_available("cache-hit-ratio"));
    REQUIRE(definition.is_available("cache-miss-ratio"));
  }

  SECTION("unknown event")
  {
    REQUIRE_FALSE(definition.is_available("does-not-exist"));
    REQUIRE_FALSE(definition.is_available(""));
  }

  SECTION("event registered but not openable on current hardware")
  {
    auto extended = perf::CounterDefinition{};

    /// Type 0x7FFFFFFF is not a valid PMU type; perf_event_open will reject it.
    extended.add("bad-hw-event", /* type = */ 0x7FFFFFFFU, /* config = */ 0UL);
    REQUIRE(extended.supports("bad-hw-event"));
    REQUIRE_FALSE(extended.is_available("bad-hw-event"));
  }

  SECTION("user-defined metric with available dependencies")
  {
    auto extended = perf::CounterDefinition{};
    extended.add("ipc", "instructions / cycles");
    REQUIRE(extended.is_available("ipc"));
  }

  SECTION("user-defined metric with unavailable dependencies")
  {
    auto extended = perf::CounterDefinition{};
    extended.add("bad-hw-event", /* type = */ 0x7FFFFFFFU, /* config = */ 0UL);
    extended.add("bad_metric", "`bad-hw-event` + cycles");
    REQUIRE(extended.supports("bad_metric"));
    REQUIRE_FALSE(extended.is_available("bad_metric"));
  }

  SECTION("mutually dependent metrics do not stack-overflow")
  {
    auto extended = perf::CounterDefinition{};
    extended.add("metric_a", "metric_b + cycles");
    extended.add("metric_b", "metric_a + instructions");
    REQUIRE_FALSE(extended.is_available("metric_a"));
    REQUIRE_FALSE(extended.is_available("metric_b"));
  }

  SECTION("diamond metric dependency is not mistaken for a cycle")
  {
    auto extended = perf::CounterDefinition{};
    extended.add("base_metric", "cycles + instructions");
    extended.add("composite", "base_metric + base_metric");
    REQUIRE(extended.is_available("composite"));
  }
}

TEST_CASE("adding new events and metrics", "[CounterDefinition]")
{
  auto definition = perf::CounterDefinition{};

  auto test_counter = std::string{ "some-test-counter-name" };

  SECTION("events do not exist")
  {
    REQUIRE(definition.counter(test_counter).empty());
    REQUIRE(definition.is_metric(test_counter) == false);
    REQUIRE(definition.metric(test_counter).has_value() == false);
  }

  SECTION("add hardware counter")
  {
    definition.add(std::string{ test_counter }, 100U, 0x1234);
    REQUIRE(definition.counter(test_counter).size() == 1U);
    REQUIRE(std::get<1>(definition.counter(test_counter).front()) == test_counter);
    REQUIRE(std::get<2>(definition.counter(test_counter).front()).configs()[0U] == 0x1234);
    REQUIRE(std::get<2>(definition.counter(test_counter).front()).type() == 100U);
    REQUIRE(definition.is_metric(test_counter) == false);
    REQUIRE(definition.metric(test_counter).has_value() == false);
  }

  SECTION("add metric")
  {
    auto test_metric = std::string{ "some-test-metric-name" };
    definition.add(std::string{ test_metric }, "cycles/instructions");
    REQUIRE(definition.counter(test_metric).empty());
    REQUIRE(definition.is_metric(test_metric));
    REQUIRE(definition.metric(test_metric).has_value());
    REQUIRE(std::get<0>(definition.metric(test_metric).value()) == test_metric);
  }

  SECTION("read csv counter-only")
  {
    const auto event0 = std::string{ "EVENT.TEST0" };
    const auto event1 = std::string{ "event-test-1" };

    REQUIRE(definition.counter(event0).empty());
    REQUIRE(definition.counter(event1).empty());

    const auto definition_with_file = perf::CounterDefinition{ "test/events.csv" };

    REQUIRE_FALSE(definition_with_file.counter(event0).empty());
    REQUIRE_FALSE(definition_with_file.counter(event1).empty());

    REQUIRE(std::get<2>(definition_with_file.counter(event0).front()).configs()[0U] == 0x1f3010e);
    REQUIRE(std::get<2>(definition_with_file.counter(event0).front()).configs()[1U] == 0U);
    REQUIRE(std::get<2>(definition_with_file.counter(event0).front()).configs()[2U] == 0U);

    REQUIRE(std::get<2>(definition_with_file.counter(event1).front()).configs()[0U] == 0x1CD);
    REQUIRE(std::get<2>(definition_with_file.counter(event1).front()).configs()[1U] == 3U);
    REQUIRE(std::get<2>(definition_with_file.counter(event1).front()).configs()[2U] == 0U);
  }

  SECTION("read csv with metric")
  {
    const auto event0 = std::string{ "EVENT.TEST0" };
    const auto event1 = std::string{ "event-test-1" };
    const auto test_metric = std::string{ "test-metric" };

    REQUIRE(definition.counter(event0).empty());
    REQUIRE(definition.counter(event1).empty());
    REQUIRE(definition.counter(test_metric).empty());

    REQUIRE_FALSE(definition.is_metric(test_metric));

    const auto definition_with_file = perf::CounterDefinition{ "test/events-and-metrics.csv" };

    REQUIRE_FALSE(definition_with_file.counter(event0).empty());
    REQUIRE_FALSE(definition_with_file.counter(event1).empty());
    REQUIRE(definition.counter(test_metric).empty());

    REQUIRE(definition_with_file.is_metric(test_metric));

    auto metric = definition_with_file.metric(test_metric);
    REQUIRE(metric.has_value());

    auto counter_result = perf::CounterResult{ std::vector<std::pair<std::string_view, double>>{
      std::make_pair("EVENT.TEST0", 100U), std::make_pair("event-test-1", 500U) } };
    const auto metric_result = metric->second.calculate(counter_result);
    REQUIRE(metric_result.has_value());
    REQUIRE(metric_result.value() == 1500U);
  }
}

TEST_CASE("child overrides parent", "[CounterDefinition]")
{
  SECTION("counter: child entry takes priority over parent for the same PMU")
  {
    auto child = perf::CounterDefinition{};

    /// "cycles" is registered in the global parent under "cpu"; override it with a custom config.
    child.add(std::string{ "cycles" }, /* type = */ 99U, /* config = */ 0x9999UL);

    const auto results = child.counter(std::string{ "cycles" });

    /// Exactly one result — no duplicate from the parent.
    REQUIRE(results.size() == 1U);

    /// The child's config is returned, not the parent's.
    REQUIRE(std::get<2>(results.front()).type() == 99U);
    REQUIRE(std::get<2>(results.front()).configs()[0U] == 0x9999UL);
  }

  SECTION("pmu_names: no duplicates when child and parent share a PMU name")
  {
    auto child = perf::CounterDefinition{};

    /// Adding any event under "cpu" creates a child-level entry; the global parent also has "cpu".
    child.add(std::string{ "my-event" }, /* type = */ 4U, /* config = */ 0x1234UL);

    const auto names = child.pmu_names();
    const auto cpu_count = std::count(names.begin(), names.end(), "cpu");

    REQUIRE(cpu_count == 1);
  }

  SECTION("pmu: child event overrides parent event with the same name in the same PMU")
  {
    auto child = perf::CounterDefinition{};

    /// Override "cycles" (which exists in the global parent under "cpu") with a custom config.
    child.add(std::string{ "cycles" }, /* type = */ 99U, /* config = */ 0x9999UL);

    const auto events = child.pmu("cpu");

    /// "cycles" must appear exactly once.
    const auto cycles_count =
      std::count_if(events.begin(), events.end(), [](const auto& event) { return std::get<0>(event) == "cycles"; });
    REQUIRE(cycles_count == 1);

    /// The child's config must be the one returned.
    const auto cycles_it =
      std::find_if(events.begin(), events.end(), [](const auto& event) { return std::get<0>(event) == "cycles"; });
    REQUIRE(cycles_it != events.end());
    REQUIRE(std::get<1>(*cycles_it).type() == 99U);
    REQUIRE(std::get<1>(*cycles_it).configs()[0U] == 0x9999UL);
  }
}