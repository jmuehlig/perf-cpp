#include <catch2/catch_test_macros.hpp>
#include <perfcpp/metric.h>

TEST_CASE("calculating", "[CyclesPerInstruction]")
{

  auto cpi_metric = perf::CyclesPerInstruction{};

  SECTION("empty result")
  {
    auto counter_result = perf::CounterResult{};
    REQUIRE(cpi_metric.calculate(counter_result).has_value() == false);
  }

  SECTION("wrong/missing hardware counters")
  {
    auto counter_result = perf::CounterResult{ std::vector<std::pair<std::string_view, double>>{
      std::make_pair("cycles", 100U), std::make_pair("some-other-event", 500U) } };
    REQUIRE(cpi_metric.calculate(counter_result).has_value() == false);
  }

  SECTION("calculation")
  {
    auto counter_result = perf::CounterResult{ std::vector<std::pair<std::string_view, double>>{
      std::make_pair("cycles", 100U), std::make_pair("instructions", 5U), std::make_pair("some-other-event", 500U) } };
    REQUIRE(cpi_metric.calculate(counter_result).has_value());
    REQUIRE(cpi_metric.calculate(counter_result).value() == 20.);
  }
}

TEST_CASE("calculating", "[InstructionsPerCycle]")
{

  auto ipc_metric = perf::InstructionsPerCycle{};

  SECTION("empty result")
  {
    auto counter_result = perf::CounterResult{};
    REQUIRE(ipc_metric.calculate(counter_result).has_value() == false);
  }

  SECTION("wrong/missing hardware counters")
  {
    auto counter_result = perf::CounterResult{ std::vector<std::pair<std::string_view, double>>{
      std::make_pair("cycles", 100U), std::make_pair("some-other-event", 500U) } };
    REQUIRE(ipc_metric.calculate(counter_result).has_value() == false);
  }

  SECTION("calculation")
  {
    auto counter_result = perf::CounterResult{ std::vector<std::pair<std::string_view, double>>{
      std::make_pair("cycles", 100U), std::make_pair("instructions", 5U), std::make_pair("some-other-event", 500U) } };
    REQUIRE(ipc_metric.calculate(counter_result).has_value());
    REQUIRE(ipc_metric.calculate(counter_result).value() == .05);
  }
}

TEST_CASE("calculating", "[CacheHitRatio]")
{
  auto chr_metric = perf::CacheHitRatio{};

  SECTION("empty result")
  {
    auto counter_result = perf::CounterResult{};
    REQUIRE(chr_metric.calculate(counter_result).has_value() == false);
  }

  SECTION("wrong/missing hardware counters")
  {
    auto counter_result = perf::CounterResult{ std::vector<std::pair<std::string_view, double>>{
      std::make_pair("cache-misses", 100U), std::make_pair("some-other-event", 500U) } };
    REQUIRE(chr_metric.calculate(counter_result).has_value() == false);
  }

  SECTION("calculation")
  {
    auto counter_result = perf::CounterResult{ std::vector<std::pair<std::string_view, double>>{
      std::make_pair("cache-misses", 10U),
      std::make_pair("cache-references", 20U),
      std::make_pair("some-other-event", 500U) } };
    REQUIRE(chr_metric.calculate(counter_result).has_value());
    REQUIRE(chr_metric.calculate(counter_result).value() == 2.);
  }
}

TEST_CASE("calculating", "[CacheMissRatio]")
{
  auto chr_metric = perf::CacheMissRatio{};

  SECTION("empty result")
  {
    auto counter_result = perf::CounterResult{};
    REQUIRE(chr_metric.calculate(counter_result).has_value() == false);
  }

  SECTION("wrong/missing hardware counters")
  {
    auto counter_result = perf::CounterResult{ std::vector<std::pair<std::string_view, double>>{
      std::make_pair("cache-misses", 100U), std::make_pair("some-other-event", 500U) } };
    REQUIRE(chr_metric.calculate(counter_result).has_value() == false);
  }

  SECTION("calculation")
  {
    auto counter_result = perf::CounterResult{ std::vector<std::pair<std::string_view, double>>{
      std::make_pair("cache-misses", 10U),
      std::make_pair("cache-references", 20U),
      std::make_pair("some-other-event", 500U) } };
    REQUIRE(chr_metric.calculate(counter_result).has_value());
    REQUIRE(chr_metric.calculate(counter_result).value() == .5);
  }
}

TEST_CASE("calculating", "[DTLBMissRatio]")
{
  auto dtlbmr_metric = perf::DTLBMissRatio{};

  SECTION("empty result")
  {
    auto counter_result = perf::CounterResult{};
    REQUIRE(dtlbmr_metric.calculate(counter_result).has_value() == false);
  }

  SECTION("wrong/missing hardware counters")
  {
    auto counter_result = perf::CounterResult{ std::vector<std::pair<std::string_view, double>>{
      std::make_pair("dTLB-load-misses", 100U), std::make_pair("some-other-event", 500U) } };
    REQUIRE(dtlbmr_metric.calculate(counter_result).has_value() == false);
  }

  SECTION("calculation")
  {
    auto counter_result = perf::CounterResult{ std::vector<std::pair<std::string_view, double>>{
      std::make_pair("dTLB-load-misses", 10U),
      std::make_pair("dTLB-loads", 20U),
      std::make_pair("some-other-event", 500U) } };
    REQUIRE(dtlbmr_metric.calculate(counter_result).has_value());
    REQUIRE(dtlbmr_metric.calculate(counter_result).value() == .5);
  }
}

TEST_CASE("calculating", "[ITLBMissRatio]")
{
  auto itlbmr_metric = perf::ITLBMissRatio{};

  SECTION("empty result")
  {
    auto counter_result = perf::CounterResult{};
    REQUIRE(itlbmr_metric.calculate(counter_result).has_value() == false);
  }

  SECTION("wrong/missing hardware counters")
  {
    auto counter_result = perf::CounterResult{ std::vector<std::pair<std::string_view, double>>{
      std::make_pair("iTLB-load-misses", 100U), std::make_pair("some-other-event", 500U) } };
    REQUIRE(itlbmr_metric.calculate(counter_result).has_value() == false);
  }

  SECTION("calculation")
  {
    auto counter_result = perf::CounterResult{ std::vector<std::pair<std::string_view, double>>{
      std::make_pair("iTLB-load-misses", 10U),
      std::make_pair("iTLB-loads", 20U),
      std::make_pair("some-other-event", 500U) } };
    REQUIRE(itlbmr_metric.calculate(counter_result).has_value());
    REQUIRE(itlbmr_metric.calculate(counter_result).value() == .5);
  }
}

TEST_CASE("calculating", "[L1DataMissRatio]")
{
  auto l1dmr_metric = perf::L1DataMissRatio{};

  SECTION("empty result")
  {
    auto counter_result = perf::CounterResult{};
    REQUIRE(l1dmr_metric.calculate(counter_result).has_value() == false);
  }

  SECTION("wrong/missing hardware counters")
  {
    auto counter_result = perf::CounterResult{ std::vector<std::pair<std::string_view, double>>{
      std::make_pair("L1-dcache-load-misses", 100U), std::make_pair("some-other-event", 500U) } };
    REQUIRE(l1dmr_metric.calculate(counter_result).has_value() == false);
  }

  SECTION("calculation")
  {
    auto counter_result = perf::CounterResult{ std::vector<std::pair<std::string_view, double>>{
      std::make_pair("L1-dcache-load-misses", 10U),
      std::make_pair("L1-dcache-loads", 20U),
      std::make_pair("some-other-event", 500U) } };
    REQUIRE(l1dmr_metric.calculate(counter_result).has_value());
    REQUIRE(l1dmr_metric.calculate(counter_result).value() == .5);
  }
}

TEST_CASE("calculating", "[BranchMissRatio]")
{
  auto bmr_metric = perf::BranchMissRatio{};

  SECTION("empty result")
  {
    auto counter_result = perf::CounterResult{};
    REQUIRE(bmr_metric.calculate(counter_result).has_value() == false);
  }

  SECTION("wrong/missing hardware counters")
  {
    auto counter_result = perf::CounterResult{ std::vector<std::pair<std::string_view, double>>{
      std::make_pair("branch-misses", 100U), std::make_pair("some-other-event", 500U) } };
    REQUIRE(bmr_metric.calculate(counter_result).has_value() == false);
  }

  SECTION("calculation")
  {
    auto counter_result = perf::CounterResult{ std::vector<std::pair<std::string_view, double>>{
      std::make_pair("branch-misses", 10U),
      std::make_pair("branches", 20U),
      std::make_pair("some-other-event", 500U) } };
    REQUIRE(bmr_metric.calculate(counter_result).has_value());
    REQUIRE(bmr_metric.calculate(counter_result).value() == .5);
  }
}

TEST_CASE("calculating", "[FormulaMetric]")
{
  auto formula_metric = perf::FormulaMetric{ "any-formula", "('event-a'*'event-b')/2+13.37" };

  SECTION("empty result")
  {
    auto counter_result = perf::CounterResult{};
    REQUIRE(formula_metric.calculate(counter_result).has_value() == false);
  }

  SECTION("wrong/missing hardware counters")
  {
    auto counter_result = perf::CounterResult{ std::vector<std::pair<std::string_view, double>>{
      std::make_pair("event-a", 100U), std::make_pair("some-other-event", 500U) } };
    REQUIRE(formula_metric.calculate(counter_result).has_value() == false);
  }

  SECTION("calculation")
  {
    auto counter_result = perf::CounterResult{ std::vector<std::pair<std::string_view, double>>{
      std::make_pair("event-a", 10U), std::make_pair("event-b", 20U), std::make_pair("some-other-event", 500U) } };
    REQUIRE(formula_metric.calculate(counter_result).has_value());
    REQUIRE(formula_metric.calculate(counter_result).value() == 113.37);
  }
}