#include "access_benchmark.hpp"
#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <cstdint>
#include <perfcpp/event_counter.hpp>
#include <pthread.h>
#include <sched.h>
#include <thread>
#include <vector>

namespace {

/// Returns the first `max_count` CPU core ids that exist on this system (clamped to actual core count).
[[nodiscard]] std::vector<std::uint16_t>
available_cores(const std::size_t max_count)
{
  const auto hw = std::max(1U, std::thread::hardware_concurrency());
  const auto count = std::min(static_cast<std::size_t>(hw), max_count);
  auto cores = std::vector<std::uint16_t>{};
  cores.reserve(count);
  for (auto i = 0U; i < count; ++i) {
    cores.push_back(static_cast<std::uint16_t>(i));
  }
  return cores;
}

/// Runs `fn` on a worker thread pinned to the given CPU core, joining before returning.
template<typename F>
void
run_pinned(const std::uint16_t cpu_id, F&& fn)
{
  auto worker = std::thread{ [cpu_id, fn = std::forward<F>(fn)]() {
    auto pinned = cpu_set_t{};
    CPU_ZERO(&pinned);
    CPU_SET(cpu_id, &pinned);
    ::pthread_setaffinity_np(::pthread_self(), sizeof(cpu_set_t), &pinned);
    fn();
  } };
  worker.join();
}

} /// namespace

TEST_CASE("multi_core_event_counter_counting", "[MultiCoreEventCounter]")
{
  auto benchmark = perf::test::AccessBenchmark{ /* is_random= */ true, 256U };
  const auto cores = available_cores(4U);
  REQUIRE_FALSE(cores.empty());

  SECTION("monitored core captures pinned workload")
  {
    /// Workload runs on a thread pinned to the monitored core; the counter must see it even though the workload
    /// thread's TID was never registered (MultiCoreEventCounter forces Process::Any).
    auto counter = perf::MultiCoreEventCounter{ std::vector<std::uint16_t>{ cores.front() } };
    counter.add("instructions");

    counter.start();
    run_pinned(cores.front(), [&]() { benchmark.run(); });
    counter.stop();

    const auto result = counter.result_of_core(cores.front());
    REQUIRE(result.has_value());
    REQUIRE(result->get("instructions").has_value());
    REQUIRE(result->get("instructions").value() > 0.);
  }

  SECTION("result_of_core returns nullopt for an unmonitored core")
  {
    auto counter = perf::MultiCoreEventCounter{ std::vector<std::uint16_t>{ cores.front() } };
    counter.add("instructions");

    counter.start();
    counter.stop();

    REQUIRE_FALSE(counter.result_of_core(static_cast<std::uint16_t>(9999U)).has_value());
  }

  SECTION("aggregated result equals sum of per-core results")
  {
    if (cores.size() < 2U) {
      SUCCEED("system has fewer than two cores; skipping multi-core aggregation");
      return;
    }

    auto counter = perf::MultiCoreEventCounter{ cores };
    counter.add("instructions");

    counter.start();
    for (const auto cpu_id : cores) {
      run_pinned(cpu_id, [&]() { benchmark.run(); });
    }
    counter.stop();

    const auto aggregated = counter.result().get("instructions");
    REQUIRE(aggregated.has_value());
    REQUIRE(aggregated.value() > 0.);

    /// Per-core results must sum to the aggregated result (within 1% for measurement noise).
    auto sum = double{ 0. };
    for (const auto cpu_id : cores) {
      const auto per_core = counter.result_of_core(cpu_id);
      REQUIRE(per_core.has_value());
      const auto value = per_core->get("instructions");
      REQUIRE(value.has_value());
      sum += value.value();
    }
    REQUIRE(std::abs(aggregated.value() - sum) < sum * 0.01);
  }

  SECTION("normalization divides per-core values")
  {
    auto counter = perf::MultiCoreEventCounter{ std::vector<std::uint16_t>{ cores.front() } };
    counter.add("instructions");

    counter.start();
    run_pinned(cores.front(), [&]() { benchmark.run(); });
    counter.stop();

    const auto raw = counter.result_of_core(cores.front());
    const auto normalized = counter.result_of_core(cores.front(), 1000U);
    REQUIRE(raw.has_value());
    REQUIRE(normalized.has_value());

    const auto raw_value = raw->get("instructions").value();
    const auto normalized_value = normalized->get("instructions").value();
    const auto expected = raw_value / 1000.;
    REQUIRE(normalized_value > expected * 0.9);
    REQUIRE(normalized_value < expected * 1.1);
  }
}

TEST_CASE("multi_core_event_counter_template", "[MultiCoreEventCounter]")
{
  auto benchmark = perf::test::AccessBenchmark{ /* is_random= */ true, 256U };
  const auto cores = available_cores(2U);
  REQUIRE_FALSE(cores.empty());

  SECTION("template constructor copies events from the source EventCounter")
  {
    auto template_counter = perf::EventCounter{};
    template_counter.add(std::vector<std::string>{ "instructions", "cycles" });

    auto counter = perf::MultiCoreEventCounter{ template_counter, cores };

    counter.start();
    for (const auto cpu_id : cores) {
      run_pinned(cpu_id, [&]() { benchmark.run(); });
    }
    counter.stop();

    const auto result = counter.result();
    REQUIRE(result.get("instructions").has_value());
    REQUIRE(result.get("instructions").value() > 0.);
    REQUIRE(result.get("cycles").has_value());
    REQUIRE(result.get("cycles").value() > 0.);
  }
}

TEST_CASE("multi_core_event_counter_lifecycle", "[MultiCoreEventCounter]")
{
  const auto cores = available_cores(2U);
  REQUIRE_FALSE(cores.empty());

  SECTION("close is idempotent")
  {
    auto counter = perf::MultiCoreEventCounter{ cores };
    counter.add("instructions");

    counter.start();
    counter.stop();
    REQUIRE_NOTHROW(counter.close());
    REQUIRE_NOTHROW(counter.close());
  }

  SECTION("empty cpu list produces empty result")
  {
    /// With no CPU ids the counter has no underlying EventCounters; result() must be empty and lifecycle calls
    /// must be safe no-ops.
    auto counter = perf::MultiCoreEventCounter{ std::vector<std::uint16_t>{} };

    REQUIRE_NOTHROW(counter.add("instructions"));
    REQUIRE_NOTHROW(counter.start());
    REQUIRE_NOTHROW(counter.stop());
    REQUIRE_NOTHROW(counter.close());
    REQUIRE(counter.result().empty());
    REQUIRE_FALSE(counter.result_of_core(0U).has_value());
  }
}
