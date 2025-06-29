#include <catch2/catch_test_macros.hpp>
#include <perfcpp/counter_definition.h>

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