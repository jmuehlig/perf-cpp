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
    REQUIRE(std::get<2>(definition.counter(test_counter).front()).event_id() == 0x1234);
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
}