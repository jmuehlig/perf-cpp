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

  SECTION("read csv")
  {
    const auto uops_issued_counter = std::string{ "UOPS_ISSUED.CORE_STALL_CYCLES" };
    const auto mem_load_counter = std::string{ "mem-load-lat-3" };

    REQUIRE(definition.counter(uops_issued_counter).empty());
    REQUIRE(definition.counter(mem_load_counter).empty());

    const auto definition_with_file = perf::CounterDefinition{ "test/counter.csv" };

    REQUIRE_FALSE(definition_with_file.counter(uops_issued_counter).empty());
    REQUIRE_FALSE(definition_with_file.counter(mem_load_counter).empty());

    REQUIRE(std::get<2>(definition_with_file.counter(uops_issued_counter).front()).event_id() == 0x1f3010e);
    REQUIRE(std::get<2>(definition_with_file.counter(uops_issued_counter).front()).event_id_extension()[0U] == 0U);
    REQUIRE(std::get<2>(definition_with_file.counter(uops_issued_counter).front()).event_id_extension()[1U] == 0U);

    REQUIRE(std::get<2>(definition_with_file.counter(mem_load_counter).front()).event_id() == 0x1CD);
    REQUIRE(std::get<2>(definition_with_file.counter(mem_load_counter).front()).event_id_extension()[0U] == 3U);
    REQUIRE(std::get<2>(definition_with_file.counter(mem_load_counter).front()).event_id_extension()[1U] == 0U);
  }
}