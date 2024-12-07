#include <catch2/catch_test_macros.hpp>
#include <perfcpp/event_counter.h>

void
execute_workload()
{
  auto constexpr length = 1000000ULL;
  auto* data = new std::uint64_t[length];
  auto sum = 0ULL;
  for (auto i = 0U; i < length; ++i) {
    sum += data[i];
  }
  delete[] data;

  asm volatile("" : "+r,m"(sum) : : "memory");
}

TEST_CASE("counting", "[EventCounter]")
{
  SECTION("empty counter")
  {
    auto counter_definition = perf::CounterDefinition{};
    auto event_counter = perf::EventCounter{ counter_definition };
    event_counter.start();
    execute_workload();
    event_counter.stop();

    REQUIRE_FALSE(event_counter.result().get("instructions").has_value());
  }

  SECTION("non-existing counter")
  {
    auto counter_definition = perf::CounterDefinition{};
    auto event_counter = perf::EventCounter{ counter_definition };

    REQUIRE_THROWS(event_counter.add("non-existing"));
  }

  SECTION("limited counters")
  {
    auto counter_definition = perf::CounterDefinition{};
    auto config = perf::Config{};
    config.max_counters_per_group(1U);
    config.max_groups(2U);
    auto event_counter = perf::EventCounter{ counter_definition, config };

    event_counter.add(std::vector<std::string>{"instructions", "cycles"});

    event_counter.start();
    execute_workload();
    event_counter.stop();

    REQUIRE(event_counter.result().get("cycles").has_value());
    REQUIRE(event_counter.result().get("instructions").has_value());
    REQUIRE(event_counter.result().get("instructions").value() > 1000000.);
    REQUIRE(event_counter.result().get("instructions").value() < 6000000.);
  }

  SECTION("too many counters")
  {
    auto counter_definition = perf::CounterDefinition{};
    auto config = perf::Config{};
    config.max_counters_per_group(1U);
    config.max_groups(2U);
    auto event_counter = perf::EventCounter{ counter_definition, config };

    REQUIRE_THROWS(event_counter.add({"instructions", "cycles", "branches"}));
  }

  SECTION("same hardware counter")
  {
    auto counter_definition = perf::CounterDefinition{};
    auto event_counter = perf::EventCounter{ counter_definition };

    event_counter.add(std::vector<std::string>{"instructions", "cycles"}, perf::EventCounter::Schedule::Group);

    event_counter.start();
    execute_workload();
    event_counter.stop();

    REQUIRE(event_counter.result().get("cycles").has_value());
    REQUIRE(event_counter.result().get("instructions").has_value());
    REQUIRE(event_counter.result().get("instructions").value() > 1000000.);
    REQUIRE(event_counter.result().get("instructions").value() < 6000000.);
  }

  SECTION("separate hardware counter")
  {
    auto counter_definition = perf::CounterDefinition{};
    auto event_counter = perf::EventCounter{ counter_definition };

    event_counter.add(std::vector<std::string>{"instructions", "cycles"}, perf::EventCounter::Schedule::Separate);

    event_counter.start();
    execute_workload();
    event_counter.stop();

    REQUIRE(event_counter.result().get("cycles").has_value());
    REQUIRE(event_counter.result().get("instructions").has_value());
    REQUIRE(event_counter.result().get("instructions").value() > 1000000.);
    REQUIRE(event_counter.result().get("instructions").value() < 6000000.);
  }

  SECTION("instructions only")
  {
    auto counter_definition = perf::CounterDefinition{};
    auto event_counter = perf::EventCounter{ counter_definition };
    event_counter.add("instructions");

    event_counter.start();
    execute_workload();
    event_counter.stop();

    REQUIRE_FALSE(event_counter.result().get("cycles").has_value());
    REQUIRE(event_counter.result().get("instructions").has_value());
    REQUIRE(event_counter.result().get("instructions").value() > 1000000.);
    REQUIRE(event_counter.result().get("instructions").value() < 6000000.);
  }

  SECTION("re-open")
  {
    auto counter_definition = perf::CounterDefinition{};
    auto event_counter = perf::EventCounter{ counter_definition };
    event_counter.add("instructions");

    event_counter.start();
    execute_workload();
    event_counter.stop();

    REQUIRE_FALSE(event_counter.result().get("cycles").has_value());
    REQUIRE(event_counter.result().get("instructions").has_value());
    REQUIRE(event_counter.result().get("instructions").value() > 1000000.);
    REQUIRE(event_counter.result().get("instructions").value() < 6000000.);

    event_counter.add("cycles");
    event_counter.start();
    execute_workload();
    event_counter.stop();

    REQUIRE(event_counter.result().get("cycles").has_value());
    REQUIRE(event_counter.result().get("instructions").has_value());
    REQUIRE(event_counter.result().get("instructions").value() > 1000000.);
    REQUIRE(event_counter.result().get("instructions").value() < 6000000.);
  }
}