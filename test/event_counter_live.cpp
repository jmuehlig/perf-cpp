#include "access_benchmark.hpp"
#include <catch2/catch_test_macros.hpp>
#include <perfcpp/event_counter.hpp>
#include <perfcpp/exception.hpp>

TEST_CASE("configuration", "[LiveEventCounter]")
{
  SECTION("add single live event")
  {
    auto event_counter = perf::EventCounter{};
    event_counter.add_live("instructions");

    const auto names = event_counter.live_event_names();
    REQUIRE(names.size() == 1U);
    REQUIRE(names[0] == "instructions");
  }

  SECTION("add multiple live events")
  {
    auto event_counter = perf::EventCounter{};
    event_counter.add_live(std::vector<std::string>{ "cache-references", "cache-misses", "branches" });

    const auto names = event_counter.live_event_names();
    REQUIRE(names.size() == 3U);
    REQUIRE(names[0] == "cache-references");
    REQUIRE(names[1] == "cache-misses");
    REQUIRE(names[2] == "branches");
  }

  SECTION("metric not supported as live event")
  {
    auto event_counter = perf::EventCounter{};
    REQUIRE_THROWS_AS(event_counter.add_live("instructions-per-cycle"), perf::MetricNotSupportedAsLiveEventError);
  }

  SECTION("time event not supported as live event")
  {
    auto event_counter = perf::EventCounter{};
    REQUIRE_THROWS_AS(event_counter.add_live("seconds"), perf::TimeEventNotSupportedAsLiveEventError);
  }
}

TEST_CASE("live result", "[LiveEventCounter]")
{
  auto benchmark = perf::test::AccessBenchmark{ /* is random */ true, 512U /* MB */ };

  SECTION("live_result by index returns a value")
  {
    auto event_counter = perf::EventCounter{};
    event_counter.add_live("instructions");

    event_counter.start();
    benchmark.run();

    const auto value = event_counter.live_result(0U);
    event_counter.stop();

    REQUIRE(value.has_value());
    REQUIRE(value.value() > 0.);
  }

  SECTION("live_result vector overload")
  {
    auto event_counter = perf::EventCounter{};
    event_counter.add_live(std::vector<std::string>{ "instructions", "cache-misses" });

    event_counter.start();
    benchmark.run();

    auto results = std::vector<double>(2U, 0.);
    event_counter.live_result(results);
    event_counter.stop();

    REQUIRE(results[0] > 0.);
    REQUIRE(results[1] >= 0.);
  }

  SECTION("live_result out-of-bounds throws")
  {
    auto event_counter = perf::EventCounter{};
    event_counter.add_live("instructions");

    event_counter.start();
    benchmark.run();

    REQUIRE_THROWS_AS(event_counter.live_result(1U), perf::LiveEventCounterOutOfBoundsAccessError);
    event_counter.stop();
  }

  SECTION("live_result vector size mismatch throws")
  {
    auto event_counter = perf::EventCounter{};
    event_counter.add_live(std::vector<std::string>{ "instructions", "cache-misses" });

    event_counter.start();
    benchmark.run();

    auto wrong_size_results = std::vector<double>(1U, 0.);
    REQUIRE_THROWS_AS(event_counter.live_result(wrong_size_results), perf::LiveEventCounterResultMismatchError);
    event_counter.stop();
  }

  SECTION("live_result grows during execution")
  {
    auto event_counter = perf::EventCounter{};
    event_counter.add_live("instructions");

    event_counter.start();

    const auto first_read = event_counter.live_result(0U);
    benchmark.run();
    const auto second_read = event_counter.live_result(0U);

    event_counter.stop();

    REQUIRE(first_read.has_value());
    REQUIRE(second_read.has_value());
    REQUIRE(second_read.value() > first_read.value());
  }

  SECTION("live_result with normalization")
  {
    auto event_counter = perf::EventCounter{};
    event_counter.add_live("instructions");

    event_counter.start();
    benchmark.run();

    const auto raw_value = event_counter.live_result(0U);
    const auto normalization = std::uint64_t{ 1000U };
    const auto normalized_value = event_counter.live_result(0U, normalization);

    event_counter.stop();

    REQUIRE(raw_value.has_value());
    REQUIRE(normalized_value.has_value());

    /// The normalized value must be approximately raw / normalization.
    const auto expected = raw_value.value() / static_cast<double>(normalization);
    REQUIRE(normalized_value.value() > expected * 0.9);
    REQUIRE(normalized_value.value() < expected * 1.1);
  }
}

TEST_CASE("LiveEventCounter", "[LiveEventCounter]")
{
  auto benchmark = perf::test::AccessBenchmark{ /* is random */ true, 512U /* MB */ };

  SECTION("basic start/stop/get")
  {
    auto event_counter = perf::EventCounter{};
    event_counter.add_live(std::vector<std::string>{ "instructions", "cache-misses" });

    auto live_events = perf::LiveEventCounter{ event_counter };

    event_counter.start();
    live_events.start();
    benchmark.run();
    live_events.stop();
    event_counter.stop();

    REQUIRE(live_events.get("instructions") > 0.);
    REQUIRE(live_events.get("cache-misses") >= 0.);
  }

  SECTION("get unknown event returns zero")
  {
    auto event_counter = perf::EventCounter{};
    event_counter.add_live("instructions");

    auto live_events = perf::LiveEventCounter{ event_counter };

    event_counter.start();
    live_events.start();
    benchmark.run();
    live_events.stop();
    event_counter.stop();

    REQUIRE(live_events.get("non-existing-event") == 0.);
  }

  SECTION("get with normalization")
  {
    auto event_counter = perf::EventCounter{};
    event_counter.add_live("instructions");

    auto live_events = perf::LiveEventCounter{ event_counter };

    event_counter.start();
    live_events.start();
    benchmark.run();
    live_events.stop();
    event_counter.stop();

    const auto raw = live_events.get("instructions");
    const auto normalization = std::uint64_t{ 1000U };
    const auto normalized = live_events.get("instructions", normalization);

    const auto expected = raw / static_cast<double>(normalization);
    REQUIRE(normalized > expected * 0.9);
    REQUIRE(normalized < expected * 1.1);
  }

  SECTION("multiple iterations yield consistent results")
  {
    auto event_counter = perf::EventCounter{};
    event_counter.add_live("instructions");

    auto live_events = perf::LiveEventCounter{ event_counter };

    event_counter.start();

    auto iteration_results = std::vector<double>{};
    iteration_results.reserve(5U);

    for (auto i = 0U; i < 5U; ++i) {
      live_events.start();
      benchmark.run();
      live_events.stop();
      iteration_results.push_back(live_events.get("instructions"));
    }

    event_counter.stop();

    /// Every iteration must have a positive instruction count.
    for (const auto value : iteration_results) {
      REQUIRE(value > 0.);
    }

    /// No single iteration should deviate more than 2x from the first.
    const auto reference = iteration_results.front();
    for (const auto value : iteration_results) {
      REQUIRE(value > reference / 2.);
      REQUIRE(value < reference * 2.);
    }
  }

  SECTION("combined with regular events")
  {
    auto event_counter = perf::EventCounter{};
    event_counter.add("cycles");
    event_counter.add_live("instructions");

    auto live_events = perf::LiveEventCounter{ event_counter };

    event_counter.start();
    live_events.start();
    benchmark.run();
    live_events.stop();
    event_counter.stop();

    REQUIRE(live_events.get("instructions") > 0.);
    REQUIRE(event_counter.result().get("cycles").has_value());
    REQUIRE(event_counter.result().get("cycles").value() > 0.);
  }
}
