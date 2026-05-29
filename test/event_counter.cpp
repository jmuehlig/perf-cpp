#include "access_benchmark.hpp"
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>
#include <optional>
#include <perfcpp/event_counter.hpp>
#include <perfcpp/exception.hpp>
#include <perfcpp/hardware_info.hpp>
#include <sched.h>
#include <string>
#include <unistd.h>

namespace {
/// Reads the cgroupv2 path of the calling process from /proc/self/cgroup.
[[nodiscard]] std::optional<std::string>
read_own_cgroup_path()
{
  auto file = std::ifstream{ "/proc/self/cgroup" };
  auto line = std::string{};
  while (std::getline(file, line)) {
    if (line.rfind("0::", 0) == 0) {
      return "/sys/fs/cgroup" + line.substr(3);
    }
  }
  return std::nullopt;
}
}

TEST_CASE("configuration", "[EventCounter]")
{
  auto readonly_benchmark = perf::test::AccessBenchmark{ /* is random */ true, 1024U /* MB */ };

  SECTION("non-existing counter")
  {
    auto event_counter = perf::EventCounter{};

    REQUIRE_THROWS(event_counter.add("non-existing"));
  }

  SECTION("limited counters")
  {
    auto config = perf::Config{};
    config.num_events_per_physical_counter(1U);
    config.num_physical_counters(2U);
    auto event_counter = perf::EventCounter{ config };

    event_counter.add(std::vector<std::string>{ "instructions", "cycles" });

    event_counter.start();
    readonly_benchmark.run();
    event_counter.stop();

    REQUIRE(event_counter.result().get("cycles").has_value());
    REQUIRE(event_counter.result().get("instructions").has_value());
  }

  SECTION("too many counters")
  {
    auto config = perf::Config{};
    config.num_events_per_physical_counter(1U);
    config.num_physical_counters(2U);
    auto event_counter = perf::EventCounter{ config };

    REQUIRE_THROWS(event_counter.add({ "branches", "branch-misses", "cache-misses" }));
  }

  SECTION("empty counter")
  {
    auto event_counter = perf::EventCounter{};
    event_counter.start();
    readonly_benchmark.run();
    event_counter.stop();

    REQUIRE_FALSE(event_counter.result().get("instructions").has_value());
  }
}

TEST_CASE("counter scheduling", "[EventCounter]")
{
  auto readonly_benchmark = perf::test::AccessBenchmark{ /* is random */ true, 1024U /* MB */ };

  SECTION("same hardware counter")
  {
    auto event_counter = perf::EventCounter{};

    event_counter.add(std::vector<std::string>{ "instructions", "cycles" }, perf::EventCounter::Schedule::Group);

    event_counter.start();
    readonly_benchmark.run();
    event_counter.stop();

    REQUIRE(event_counter.result().get("cycles").has_value());
    REQUIRE(event_counter.result().get("instructions").has_value());
    REQUIRE(event_counter.result().get("instructions").value() > 100000000.);
    REQUIRE(event_counter.result().get("instructions").value() < 140000000.);
  }

  SECTION("separate hardware counter")
  {
    auto event_counter = perf::EventCounter{};

    event_counter.add(std::vector<std::string>{ "instructions", "cycles" }, perf::EventCounter::Schedule::Separate);

    event_counter.start();
    readonly_benchmark.run();
    event_counter.stop();

    REQUIRE(event_counter.result().get("cycles").has_value());
    REQUIRE(event_counter.result().get("instructions").has_value());
    REQUIRE(event_counter.result().get("instructions").value() > 100000000.);
    REQUIRE(event_counter.result().get("instructions").value() < 140000000.);
  }

  SECTION("counter with pmu")
  {
    auto event_counter = perf::EventCounter{};

    event_counter.add(std::vector<std::string>{ "cpu/cycles" }, perf::EventCounter::Schedule::Separate);

    event_counter.start();
    readonly_benchmark.run();
    event_counter.stop();

    REQUIRE(event_counter.result().get("cycles").has_value());
  }

  SECTION("counter with specific pmu")
  {
    // Add some non-existing "cycles" counter for a non-existing PMU; we will only add cpu/cycles.
    auto counter_definition = perf::CounterDefinition{};
    counter_definition.add("non-existing", "cycles", 10000U, 10000U);

    auto event_counter = perf::EventCounter{ counter_definition };

    event_counter.add(std::vector<std::string>{ "cpu/cycles" }, perf::EventCounter::Schedule::Separate);

    event_counter.start();
    readonly_benchmark.run();
    event_counter.stop();

    REQUIRE(event_counter.result().get("cycles").has_value());
  }
}

TEST_CASE("fixed counters bypass the generic PMC limit", "[EventCounter]")
{
  /// Regression test: a fixed-function event (e.g. instructions/cycles on Intel) added after the generic PMC budget
  /// is already exhausted must still be scheduled on its dedicated fixed counter, not rejected with
  /// MaxPhysicalCountersReachedError. Fixed events are extracted before generics within a single add(), so the bug
  /// only surfaces across add() calls, once the generic groups already fill num_physical_counters().
  if (!perf::HardwareInfo::is_intel() ||
      perf::HardwareInfo::physical_fixed_performance_counters_per_logical_core() == 0U) {
    SKIP("Requires an Intel CPU with fixed-function performance counters.");
  }

  auto readonly_benchmark = perf::test::AccessBenchmark{ /* is random */ true, 1024U /* MB */ };

  auto config = perf::Config{};
  config.num_events_per_physical_counter(1U);
  config.num_physical_counters(2U);
  auto event_counter = perf::EventCounter{ config };

  /// Fill both generic counters with generic (non-fixed) events.
  event_counter.add(std::vector<std::string>{ "branches", "cache-misses" });

  /// Adding a fixed event afterwards must not throw, even though the generic budget is exhausted.
  REQUIRE_NOTHROW(event_counter.add("instructions"));

  event_counter.start();
  readonly_benchmark.run();
  event_counter.stop();

  /// All three events must be present in the result, confirming the fixed event was scheduled and read.
  const auto result = event_counter.result();
  REQUIRE(result.get("branches").has_value());
  REQUIRE(result.get("cache-misses").has_value());
  REQUIRE(result.get("instructions").has_value());
  REQUIRE(result.get("instructions").value() > 0.);
}

TEST_CASE("counting", "[EventCounter]")
{
  auto readonly_benchmark = perf::test::AccessBenchmark{ /* is random */ true, 1024U /* MB */ };

  SECTION("instructions only")
  {
    auto event_counter = perf::EventCounter{};
    event_counter.add("instructions");

    event_counter.start();
    readonly_benchmark.run();
    event_counter.stop();

    REQUIRE_FALSE(event_counter.result().get("cycles").has_value());
    REQUIRE(event_counter.result().get("instructions").has_value());
    REQUIRE(event_counter.result().get("instructions").value() > 100000000.);
    REQUIRE(event_counter.result().get("instructions").value() < 140000000.);
  }

  SECTION("re-open")
  {
    auto event_counter = perf::EventCounter{};
    event_counter.add("instructions");

    event_counter.start();
    readonly_benchmark.run();
    event_counter.stop();
    const auto result1 = event_counter.result();

    REQUIRE(result1.get("instructions").has_value());

    /// Restart the same counter without adding new events.
    event_counter.start();
    readonly_benchmark.run();
    event_counter.stop();
    const auto result2 = event_counter.result();

    REQUIRE(result2.get("instructions").has_value());

    const auto max_instructions = std::max(result1.get("instructions").value(), result2.get("instructions").value());
    const auto min_instructions = std::min(result1.get("instructions").value(), result2.get("instructions").value());
    REQUIRE((1. / max_instructions * min_instructions) < 1.1);
    REQUIRE((1. / max_instructions * min_instructions) > .9);
  }

  SECTION("re-open with close")
  {
    auto event_counter = perf::EventCounter{};
    event_counter.add("instructions");

    event_counter.start();
    readonly_benchmark.run();
    event_counter.stop();
    const auto result1 = event_counter.result();

    REQUIRE_FALSE(result1.get("cycles").has_value());
    REQUIRE(result1.get("instructions").has_value());

    /// Close before adding new events, then restart.
    event_counter.close();
    event_counter.add("cycles");
    event_counter.start();
    readonly_benchmark.run();
    event_counter.stop();
    const auto result2 = event_counter.result();

    REQUIRE(result2.get("cycles").has_value());
    REQUIRE(result2.get("instructions").has_value());

    const auto max_instructions = std::max(result1.get("instructions").value(), result2.get("instructions").value());
    const auto min_instructions = std::min(result1.get("instructions").value(), result2.get("instructions").value());
    REQUIRE((1. / max_instructions * min_instructions) < 1.1);
    REQUIRE((1. / max_instructions * min_instructions) > .9);
  }

  SECTION("add when opened")
  {
    /// Adding after start() (implicit open).
    {
      auto event_counter = perf::EventCounter{};
      event_counter.add("instructions");
      event_counter.start();
      REQUIRE_THROWS(event_counter.add("cycles"));
      event_counter.stop();
    }

    /// Adding after explicit open().
    {
      auto event_counter = perf::EventCounter{};
      event_counter.add("instructions");
      event_counter.open();
      REQUIRE_THROWS(event_counter.add("cycles"));
      event_counter.close();
    }
  }

  SECTION("instructions only")
  {
    auto event_counter = perf::EventCounter{};
    event_counter.add("instructions");

    event_counter.start();
    readonly_benchmark.run();
    event_counter.stop();

    REQUIRE_FALSE(event_counter.result().get("cycles").has_value());
    REQUIRE(event_counter.result().get("instructions").has_value());
    REQUIRE(event_counter.result().get("instructions").value() > 100000000.);
    REQUIRE(event_counter.result().get("instructions").value() < 140000000.);
  }

  SECTION("cache pattern increasing workload")
  {
    /// Create both benchmarks upfront so their pages are faulted in before any counter runs.
    auto readonly_sequential_benchmark = perf::test::AccessBenchmark{ /* is random */ false, 1024U /* MB */ };

    auto event_counter = perf::EventCounter{};
    event_counter.add({ "seconds", "instructions", "cycles", "cache-misses" });

    event_counter.start();
    readonly_benchmark.run();
    event_counter.stop();
    const auto random_result = event_counter.result();

    REQUIRE(random_result.get("seconds").has_value());
    REQUIRE(random_result.get("instructions").has_value());
    REQUIRE(random_result.get("cycles").has_value());
    REQUIRE(random_result.get("cache-misses").has_value());

    event_counter.start();
    readonly_sequential_benchmark.run();
    event_counter.stop();
    const auto sequential_result = event_counter.result();

    REQUIRE(sequential_result.get("seconds").has_value());
    REQUIRE(sequential_result.get("instructions").has_value());
    REQUIRE(sequential_result.get("cycles").has_value());
    REQUIRE(sequential_result.get("cache-misses").has_value());

    REQUIRE(random_result.get("cache-misses").value() > (sequential_result.get("cache-misses").value() * 2U));
    REQUIRE(random_result.get("cycles").value() > (sequential_result.get("cycles").value() * 2U));
    REQUIRE(random_result.get("seconds").value() > (sequential_result.get("seconds").value() * 2U));
  }

  SECTION("random access per cache line")
  {
    auto event_counter = perf::EventCounter{};
    event_counter.add({ "instructions", "cycles", "cache-misses", "branches", "dTLB-miss-ratio" });

    event_counter.start();
    readonly_benchmark.run();
    event_counter.stop();
    const auto random_result = event_counter.result(readonly_benchmark.size());

    REQUIRE(random_result.get("instructions").has_value());
    REQUIRE(random_result.get("instructions").value() > 6U);
    REQUIRE(random_result.get("instructions").value() < 9U);

    REQUIRE(random_result.get("cycles").has_value());
    REQUIRE(random_result.get("cycles").value() > 25U);
    REQUIRE(random_result.get("cycles").value() < 100U);

    REQUIRE(random_result.get("cache-misses").has_value());
    REQUIRE(random_result.get("cache-misses").value() > 1);
    REQUIRE(random_result.get("cache-misses").value() < 2);

    REQUIRE(random_result.get("branches").has_value());
    REQUIRE(random_result.get("branches").value() > .9);
    REQUIRE(random_result.get("branches").value() < 1.1);

    REQUIRE(random_result.get("dTLB-miss-ratio").has_value());
    REQUIRE(random_result.get("dTLB-miss-ratio").value() > .4);
    REQUIRE(random_result.get("dTLB-miss-ratio").value() < 1.1);
  }

  SECTION("time")
  {
    auto event_counter = perf::EventCounter{};
    event_counter.add(std::vector<std::string>{ "seconds", "milliseconds" });

    event_counter.start();
    readonly_benchmark.run();
    event_counter.stop();

    const auto result = event_counter.result();
    REQUIRE(result.get("seconds").has_value());
    REQUIRE(result.get("milliseconds").has_value());
    REQUIRE_FALSE(result.get("nanoseconds").has_value());

    REQUIRE((result.get("seconds").value() * 1100.) > (result.get("milliseconds").value()));
  }
}

TEST_CASE("open errors", "[EventCounter]")
{
  SECTION("invalid cpu core")
  {
    auto config = perf::Config{};
    config.cpu_core(9999U);

    auto event_counter = perf::EventCounter{ config };
    event_counter.add("instructions");

    REQUIRE_THROWS_AS(event_counter.open(), perf::CannotOpenCounterError);
  }

  SECTION("invalid pid")
  {
    auto config = perf::Config{};
    config.process(static_cast<pid_t>(999999));

    auto event_counter = perf::EventCounter{ config };
    event_counter.add("instructions");

    REQUIRE_THROWS_AS(event_counter.open(), perf::CannotOpenCounterError);
  }
}

TEST_CASE("EventCounter with explicit PID", "[EventCounter]")
{
  auto readonly_benchmark = perf::test::AccessBenchmark{ /* is random */ true, 1024U /* MB */ };

  SECTION("own process by getpid()")
  {
    auto config = perf::Config{};
    config.process(static_cast<pid_t>(::getpid()));

    auto event_counter = perf::EventCounter{ config };
    event_counter.add(std::vector<std::string>{ "instructions", "cycles" });

    event_counter.start();
    readonly_benchmark.run();
    event_counter.stop();

    const auto result = event_counter.result();
    REQUIRE(result.get("instructions").has_value());
    REQUIRE(result.get("instructions").value() > 0.);
    REQUIRE(result.get("cycles").has_value());
    REQUIRE(result.get("cycles").value() > 0.);
  }
}

TEST_CASE("EventCounter with cgroup", "[EventCounter]")
{
  const auto cgroup_path = read_own_cgroup_path();
  REQUIRE(cgroup_path.has_value());

  const auto cpu_id = ::sched_getcpu();
  REQUIRE(cpu_id >= 0);

  /// Pin this thread to the current CPU so the workload runs on the monitored CPU.
  auto saved_affinity = cpu_set_t{};
  ::sched_getaffinity(0, sizeof(cpu_set_t), &saved_affinity);

  auto pinned_set = cpu_set_t{};
  CPU_ZERO(&pinned_set);
  CPU_SET(cpu_id, &pinned_set);
  ::sched_setaffinity(0, sizeof(cpu_set_t), &pinned_set);

  auto config = perf::Config{};
  config.cgroup(perf::CGroupMonitor{ std::filesystem::path{ cgroup_path.value() } });
  config.cpu_core(static_cast<std::uint16_t>(cpu_id));

  auto event_counter = perf::EventCounter{ config };
  event_counter.add(std::vector<std::string>{ "instructions", "cycles" });

  auto readonly_benchmark = perf::test::AccessBenchmark{ /* is random */ true, 1024U /* MB */ };

  event_counter.start();
  readonly_benchmark.run();
  event_counter.stop();

  const auto result = event_counter.result();
  REQUIRE(result.get("instructions").has_value());
  REQUIRE(result.get("instructions").value() > 0.);
  REQUIRE(result.get("cycles").has_value());
  REQUIRE(result.get("cycles").value() > 0.);

  ::sched_setaffinity(0, sizeof(cpu_set_t), &saved_affinity);
}

TEST_CASE("lifecycle", "[EventCounter]")
{
  auto readonly_benchmark = perf::test::AccessBenchmark{ /* is random */ true, 1024U /* MB */ };

  SECTION("start without prior open")
  {
    /// start() implicitly calls open(); no exception expected.
    auto event_counter = perf::EventCounter{};
    event_counter.add("instructions");

    REQUIRE_NOTHROW(event_counter.start());
    readonly_benchmark.run();
    REQUIRE_NOTHROW(event_counter.stop());

    auto result = event_counter.result();
    const auto instructions = result.get("instructions");
    REQUIRE(instructions.has_value());
    REQUIRE(instructions.value() > 0U);

    event_counter.close();
  }

  SECTION("double start without stop")
  {
    /// Second start() is a no-op on the already-enabled counter; no exception expected.
    auto event_counter = perf::EventCounter{};
    event_counter.add("instructions");

    REQUIRE_NOTHROW(event_counter.start());
    REQUIRE_NOTHROW(event_counter.start());
    readonly_benchmark.run();
    REQUIRE_NOTHROW(event_counter.stop());

    auto result = event_counter.result();
    const auto instructions = result.get("instructions");
    REQUIRE(instructions.has_value());
    REQUIRE(instructions.value() > 0U);

    event_counter.close();
  }
}

TEST_CASE("metric expansion", "[EventCounter]")
{
  auto readonly_benchmark = perf::test::AccessBenchmark{ /* is random */ true, 1024U /* MB */ };

  SECTION("metric alone hides its required events")
  {
    /// Adding only the metric must schedule its hardware dependencies but hide them from the result.
    auto event_counter = perf::EventCounter{};
    event_counter.add("instructions-per-cycle");

    event_counter.start();
    readonly_benchmark.run();
    event_counter.stop();

    const auto result = event_counter.result();
    REQUIRE(result.get("instructions-per-cycle").has_value());
    REQUIRE(result.get("instructions-per-cycle").value() > 0.);
    REQUIRE_FALSE(result.get("instructions").has_value());
    REQUIRE_FALSE(result.get("cycles").has_value());
  }

  SECTION("required event added before metric stays visible")
  {
    /// Explicit event is visible; the metric's other dependency stays hidden.
    auto event_counter = perf::EventCounter{};
    event_counter.add("instructions");
    event_counter.add("instructions-per-cycle");

    event_counter.start();
    readonly_benchmark.run();
    event_counter.stop();

    const auto result = event_counter.result();
    REQUIRE(result.get("instructions").has_value());
    REQUIRE(result.get("instructions").value() > 0.);
    REQUIRE(result.get("instructions-per-cycle").has_value());
    REQUIRE(result.get("instructions-per-cycle").value() > 0.);
    REQUIRE_FALSE(result.get("cycles").has_value());
  }

  SECTION("required event added after metric upgrades visibility")
  {
    /// The dependency was scheduled hidden by the metric expansion; adding it explicitly must promote it.
    auto event_counter = perf::EventCounter{};
    event_counter.add("instructions-per-cycle");
    event_counter.add("instructions");

    event_counter.start();
    readonly_benchmark.run();
    event_counter.stop();

    const auto result = event_counter.result();
    REQUIRE(result.get("instructions").has_value());
    REQUIRE(result.get("instructions").value() > 0.);
    REQUIRE(result.get("instructions-per-cycle").has_value());
    REQUIRE(result.get("instructions-per-cycle").value() > 0.);
    REQUIRE_FALSE(result.get("cycles").has_value());
  }

  SECTION("metric appears only once when added twice")
  {
    /// Adding the same metric twice must not duplicate it in the result or re-schedule its dependencies.
    auto event_counter = perf::EventCounter{};
    event_counter.add("instructions-per-cycle");
    event_counter.add("instructions-per-cycle");

    event_counter.start();
    readonly_benchmark.run();
    event_counter.stop();

    const auto result = event_counter.result();
    REQUIRE(result.get("instructions-per-cycle").has_value());

    /// Count occurrences via iteration since CounterResult::get returns only the first match.
    auto metric_count = 0U;
    for (const auto& [name, _] : result) {
      if (name == "instructions-per-cycle") {
        ++metric_count;
      }
    }
    REQUIRE(metric_count == 1U);
  }

  SECTION("two metrics share hardware dependencies")
  {
    /// IPC and CPI both depend on instructions + cycles. The shared dependencies must be scheduled once: with a limit
    /// of two physical counters, both metrics must still fit and compute.
    auto config = perf::Config{};
    config.num_physical_counters(2U);
    config.num_events_per_physical_counter(1U);
    auto event_counter = perf::EventCounter{ config };

    event_counter.add(std::vector<std::string>{ "instructions-per-cycle", "cycles-per-instruction" });

    event_counter.start();
    readonly_benchmark.run();
    event_counter.stop();

    const auto result = event_counter.result();
    REQUIRE(result.get("instructions-per-cycle").has_value());
    REQUIRE(result.get("instructions-per-cycle").value() > 0.);
    REQUIRE(result.get("cycles-per-instruction").has_value());
    REQUIRE(result.get("cycles-per-instruction").value() > 0.);

    /// IPC and CPI must be reciprocals: IPC * CPI == 1 (within floating-point tolerance).
    const auto product = result.get("instructions-per-cycle").value() * result.get("cycles-per-instruction").value();
    REQUIRE(product > 0.99);
    REQUIRE(product < 1.01);

    /// Underlying hardware dependencies must remain hidden.
    REQUIRE_FALSE(result.get("instructions").has_value());
    REQUIRE_FALSE(result.get("cycles").has_value());
  }

  SECTION("metric ordering follows first appearance")
  {
    /// Result order is dictated by the order events enter the requested event set, including hidden ones.
    auto event_counter = perf::EventCounter{};
    event_counter.add("instructions");
    event_counter.add("instructions-per-cycle");
    event_counter.add("cycles");

    event_counter.start();
    readonly_benchmark.run();
    event_counter.stop();

    const auto result = event_counter.result();

    /// Expected order: instructions (added explicitly first), then cycles (added hidden by IPC, then upgraded by the
    /// explicit add), then instructions-per-cycle (the metric itself).
    auto names = std::vector<std::string_view>{};
    for (const auto& [name, _] : result) {
      names.push_back(name);
    }
    REQUIRE(names.size() == 3U);
    REQUIRE(names[0] == "instructions");
    REQUIRE(names[1] == "cycles");
    REQUIRE(names[2] == "instructions-per-cycle");
  }
}

TEST_CASE("ergonomics", "[EventCounter]")
{
  SECTION("config() getter returns the construction config")
  {
    auto config = perf::Config{};
    config.num_physical_counters(3U);
    config.num_events_per_physical_counter(2U);

    const auto event_counter = perf::EventCounter{ config };
    REQUIRE(event_counter.config().num_physical_counters() == 3U);
    REQUIRE(event_counter.config().num_events_per_physical_counter() == 2U);
  }

  SECTION("config() setter overrides the previous config")
  {
    auto initial = perf::Config{};
    initial.num_physical_counters(2U);
    auto event_counter = perf::EventCounter{ initial };
    REQUIRE(event_counter.config().num_physical_counters() == 2U);

    auto updated = perf::Config{};
    updated.num_physical_counters(5U);
    event_counter.config(updated);
    REQUIRE(event_counter.config().num_physical_counters() == 5U);
  }

  SECTION("config() setter takes effect for subsequent add()")
  {
    /// Updating the config after construction must change the scheduling limits used by add().
    auto event_counter = perf::EventCounter{};

    auto tight = perf::Config{};
    tight.num_physical_counters(1U);
    tight.num_events_per_physical_counter(1U);
    event_counter.config(tight);

    REQUIRE_THROWS(event_counter.add(std::vector<std::string>{ "instructions", "cycles" }));
  }

  SECTION("add(string&&) rvalue overload registers the event")
  {
    auto event_counter = perf::EventCounter{};
    REQUIRE_NOTHROW(event_counter.add(std::string{ "instructions" }));
  }

  SECTION("add(vector&&) rvalue overload registers the events")
  {
    auto event_counter = perf::EventCounter{};
    REQUIRE_NOTHROW(event_counter.add(std::vector<std::string>{ "instructions", "cycles" }));
  }

  SECTION("empty EventCounter has empty result and safe lifecycle")
  {
    /// With no events scheduled, open()/start()/stop()/close() must be safe no-ops and result() must be empty.
    auto event_counter = perf::EventCounter{};

    REQUIRE_NOTHROW(event_counter.open());
    REQUIRE_NOTHROW(event_counter.start());
    REQUIRE_NOTHROW(event_counter.stop());

    const auto result = event_counter.result();
    REQUIRE(result.empty());
    REQUIRE(result.size() == 0U);

    REQUIRE_NOTHROW(event_counter.close());
    /// Second close on an already-closed counter must remain safe.
    REQUIRE_NOTHROW(event_counter.close());
  }

  SECTION("close before any open is safe")
  {
    /// close() must be safe on a counter that was never opened, even after add().
    auto event_counter = perf::EventCounter{};
    event_counter.add("instructions");
    REQUIRE_NOTHROW(event_counter.close());
  }
}