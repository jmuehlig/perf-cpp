#include "access_benchmark.hpp"
#include <algorithm>
#include <array>
#include <catch2/catch_test_macros.hpp>
#include <perfcpp/event_counter.hpp>
#include <perfcpp/exception.hpp>
#include <perfcpp/hardware_info.hpp>

TEST_CASE("configuration", "[LiveEventCounter]")
{
  if (!(perf::HardwareInfo::is_intel() || perf::HardwareInfo::is_amd())) {
    SKIP("LiveEventCounter uses rdpmc, which is only available on x86 hardware.");
  }
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
  if (!(perf::HardwareInfo::is_intel() || perf::HardwareInfo::is_amd())) {
    SKIP("LiveEventCounter uses rdpmc, which is only available on x86 hardware.");
  }

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
  if (!(perf::HardwareInfo::is_intel() || perf::HardwareInfo::is_amd())) {
    SKIP("LiveEventCounter uses rdpmc, which is only available on x86 hardware.");
  }

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

TEST_CASE("LiveEventCounter edge cases", "[LiveEventCounter]")
{
  if (!(perf::HardwareInfo::is_intel() || perf::HardwareInfo::is_amd())) {
    SKIP("LiveEventCounter uses rdpmc, which is only available on x86 hardware.");
  }

  auto benchmark = perf::test::AccessBenchmark{ /* is random */ true, 256U /* MB */ };

  SECTION("live_event_names is empty when no live events were added")
  {
    auto event_counter = perf::EventCounter{};
    REQUIRE(event_counter.live_event_names().empty());

    /// LiveEventCounter built on a counter with no live events must still answer queries with 0.
    const auto live_events = perf::LiveEventCounter{ event_counter };
    REQUIRE(live_events.get("instructions") == 0.);
  }

  SECTION("live_event_names ignores regular (non-live) events")
  {
    auto event_counter = perf::EventCounter{};
    event_counter.add("instructions");
    event_counter.add("cycles");

    REQUIRE(event_counter.live_event_names().empty());
  }

  SECTION("get before any start/stop returns 0")
  {
    /// Cached start/stop values are nullopt at construction; get must short-circuit to 0 instead of dereferencing.
    auto event_counter = perf::EventCounter{};
    event_counter.add_live("instructions");

    const auto live_events = perf::LiveEventCounter{ event_counter };
    REQUIRE(live_events.get("instructions") == 0.);
    REQUIRE(live_events.get("instructions", std::uint64_t{ 1000U }) == 0.);
  }

  SECTION("get after only start (no stop) returns 0")
  {
    /// stop_value is still nullopt; get must not subtract from an empty optional.
    auto event_counter = perf::EventCounter{};
    event_counter.add_live("instructions");

    auto live_events = perf::LiveEventCounter{ event_counter };

    event_counter.start();
    live_events.start();
    benchmark.run();
    /// Deliberately do NOT call live_events.stop().

    REQUIRE(live_events.get("instructions") == 0.);

    event_counter.stop();
  }

  SECTION("get for an unknown event name returns 0")
  {
    auto event_counter = perf::EventCounter{};
    event_counter.add_live("instructions");

    const auto live_events = perf::LiveEventCounter{ event_counter };
    REQUIRE(live_events.get("not-an-event") == 0.);
    REQUIRE(live_events.get("not-an-event", std::uint64_t{ 1000U }) == 0.);
  }

  SECTION("add_live(string&&) and add_live(vector&&) rvalue overloads register events")
  {
    auto event_counter = perf::EventCounter{};
    REQUIRE_NOTHROW(event_counter.add_live(std::string{ "instructions" }));
    REQUIRE_NOTHROW(event_counter.add_live(std::vector<std::string>{ "cache-misses", "branches" }));

    const auto names = event_counter.live_event_names();
    REQUIRE(names.size() == 3U);
  }
}

TEST_CASE("EventCounter::live_result vector with normalization", "[LiveEventCounter]")
{
  if (!(perf::HardwareInfo::is_intel() || perf::HardwareInfo::is_amd())) {
    SKIP("LiveEventCounter uses rdpmc, which is only available on x86 hardware.");
  }

  auto benchmark = perf::test::AccessBenchmark{ /* is random */ true, 512U /* MB */ };

  SECTION("normalization divides each entry")
  {
    auto event_counter = perf::EventCounter{};
    event_counter.add_live(std::vector<std::string>{ "instructions", "cache-misses" });

    event_counter.start();
    benchmark.run();

    auto raw = std::vector<double>(2U, 0.);
    event_counter.live_result(raw);

    auto normalized = std::vector<double>(2U, 0.);
    const auto normalization = std::uint64_t{ 1000U };
    event_counter.live_result(normalized, normalization);

    event_counter.stop();

    /// Live reads are independent in time, so allow 10% slack for the small drift between the two reads.
    const auto expected_instructions = raw[0] / static_cast<double>(normalization);
    REQUIRE(normalized[0] > expected_instructions * 0.9);
    REQUIRE(normalized[0] < expected_instructions * 1.1);
  }

  SECTION("size mismatch throws")
  {
    auto event_counter = perf::EventCounter{};
    event_counter.add_live(std::vector<std::string>{ "instructions", "cache-misses" });

    event_counter.start();
    benchmark.run();

    auto wrong_size = std::vector<double>(1U, 0.);
    REQUIRE_THROWS_AS(event_counter.live_result(wrong_size, std::uint64_t{ 1000U }),
                      perf::LiveEventCounterResultMismatchError);

    event_counter.stop();
  }
}

#if defined(__x86_64__) || defined(__i386__)
/// Reads the hardware performance counter with the given (zero-based) rdpmc index.
static std::uint64_t
read_pmc(const std::uint32_t counter)
{
  auto low = std::uint32_t{};
  auto high = std::uint32_t{};
  asm volatile("rdpmc" : "=a"(low), "=d"(high) : "c"(counter));
  return (static_cast<std::uint64_t>(high) << 32U) | low;
}
#endif

TEST_CASE("live info", "[LiveEventCounter]")
{
  if (!(perf::HardwareInfo::is_intel() || perf::HardwareInfo::is_amd())) {
    SKIP("LiveEventCounter uses rdpmc, which is only available on x86 hardware.");
  }

  SECTION("live_info out-of-bounds throws")
  {
    auto event_counter = perf::EventCounter{};
    event_counter.add_live("instructions");

    REQUIRE_THROWS_AS(event_counter.live_info(1U), perf::LiveEventCounterOutOfBoundsAccessError);
  }

  SECTION("live_info is empty before the counter is opened")
  {
    /// Without opening, there is no mmap-ed buffer to read from.
    auto event_counter = perf::EventCounter{};
    event_counter.add_live("instructions");

    REQUIRE_FALSE(event_counter.live_info(0U).has_value());
  }

  SECTION("live_info by index returns valid info while counting")
  {
    auto event_counter = perf::EventCounter{};
    event_counter.add_live("instructions");

    event_counter.start();
    const auto info = event_counter.live_info(0U);
    event_counter.stop();

    REQUIRE(info.has_value());

    /// The kernel reports index 0 for "no rdpmc possible", which must have been mapped to nullopt.
    REQUIRE(info.value().index() != 0U);

    /// Counter width on x86 is typically 48 bits, but never 0 and never more than 64.
    REQUIRE(info.value().width() > 0U);
    REQUIRE(info.value().width() <= 64U);

    /// An odd seqlock means the kernel was writing; such a snapshot must never be returned.
    REQUIRE((info.value().lock() & 1U) == 0U);
  }

  SECTION("live_info is empty after the counter is closed")
  {
    auto event_counter = perf::EventCounter{};
    event_counter.add_live("instructions");

    event_counter.start();
    event_counter.stop();
    event_counter.close();

    REQUIRE_FALSE(event_counter.live_info(0U).has_value());
  }

  SECTION("live_info vector overload is empty without live events")
  {
    auto event_counter = perf::EventCounter{};
    event_counter.add("instructions");

    event_counter.start();
    const auto infos = event_counter.live_info();
    event_counter.stop();

    REQUIRE(infos.empty());
  }

  SECTION("live_info vector overload has default info before the counter is opened")
  {
    auto event_counter = perf::EventCounter{};
    event_counter.add_live(std::vector<std::string>{ "instructions", "cycles" });

    const auto infos = event_counter.live_info();

    REQUIRE(infos.size() == 2U);
    REQUIRE(infos[0] == perf::LiveCounterInfo{});
    REQUIRE(infos[1] == perf::LiveCounterInfo{});
  }

  SECTION("live_info vector overload returns one info per live event")
  {
    auto event_counter = perf::EventCounter{};
    event_counter.add_live(std::vector<std::string>{ "instructions", "cycles" });

    event_counter.start();
    const auto infos = event_counter.live_info();
    const auto first_info = event_counter.live_info(0U);
    event_counter.stop();

    REQUIRE(infos.size() == 2U);
    REQUIRE(infos[0].index() != 0U);
    REQUIRE(infos[1].index() != 0U);

    /// Both events are scheduled at the same time, so they must occupy different hardware counters.
    REQUIRE(infos[0].index() != infos[1].index());

    /// The vector overload must report the same hardware counter as the single-index overload.
    REQUIRE(first_info.has_value());
    REQUIRE(first_info.value().index() == infos[0].index());
    REQUIRE(first_info.value().width() == infos[0].width());
  }

  SECTION("live_info in-place overload fills one info per live event")
  {
    auto event_counter = perf::EventCounter{};
    event_counter.add_live(std::vector<std::string>{ "instructions", "cycles" });

    auto infos = std::vector<perf::LiveCounterInfo>(2U);

    event_counter.start();
    event_counter.live_info(infos);
    const auto first_info = event_counter.live_info(0U);
    event_counter.stop();

    REQUIRE(infos[0].index() != 0U);
    REQUIRE(infos[1].index() != 0U);
    REQUIRE(infos[0].index() != infos[1].index());

    /// The in-place overload must report the same hardware counter as the single-index overload.
    REQUIRE(first_info.has_value());
    REQUIRE(first_info.value().index() == infos[0].index());
    REQUIRE(first_info.value().width() == infos[0].width());
  }

  SECTION("live_info in-place overload size mismatch throws")
  {
    auto event_counter = perf::EventCounter{};
    event_counter.add_live(std::vector<std::string>{ "instructions", "cycles" });

    auto wrong_size = std::vector<perf::LiveCounterInfo>(1U);
    REQUIRE_THROWS_AS(event_counter.live_info(wrong_size), perf::LiveEventCounterResultMismatchError);
  }

  SECTION("live_info in-place overload resets stale info")
  {
    auto event_counter = perf::EventCounter{};
    event_counter.add_live("instructions");

    auto infos = std::vector<perf::LiveCounterInfo>(1U);

    event_counter.start();
    event_counter.live_info(infos);
    event_counter.stop();
    REQUIRE(infos[0].index() != 0U);

    /// After closing, the counter is no longer readable; the previously read info must not survive.
    event_counter.close();
    event_counter.live_info(infos);
    REQUIRE(infos[0] == perf::LiveCounterInfo{});
  }

#if defined(__x86_64__) || defined(__i386__)
  SECTION("live_info enables reading the hardware counter via rdpmc")
  {
    auto event_counter = perf::EventCounter{};
    event_counter.add_live("instructions");

    event_counter.start();

    constexpr auto iterations = std::uint64_t{ 10000U };
    auto is_measured = false;
    auto instructions = std::uint64_t{ 0U };

    /// Raw rdpmc values are only comparable if the kernel did not reschedule the counter between both reads, which is
    /// signaled by an unchanged seqlock. Retry until one measurement is undisturbed.
    for (auto attempt = 0U; attempt < 100U && !is_measured; ++attempt) {
      const auto info_before = event_counter.live_info(0U);
      REQUIRE(info_before.has_value());

      /// The mmap-ed index is the hardware counter index + 1.
      const auto raw_before = read_pmc(info_before.value().index() - 1U);
      for (auto volatile i = std::uint64_t{ 0U }; i < iterations; i = i + 1U) {
      }
      const auto raw_after = read_pmc(info_before.value().index() - 1U);

      const auto info_after = event_counter.live_info(0U);
      if (info_after.has_value() && info_after.value() == info_before.value()) {
        const auto width = info_before.value().width();
        const auto mask = width >= 64U ? ~std::uint64_t{ 0U } : (std::uint64_t{ 1U } << width) - 1U;
        instructions = (raw_after - raw_before) & mask;
        is_measured = true;
      }
    }

    event_counter.stop();

    REQUIRE(is_measured);

    /// Each loop iteration retires multiple instructions; the upper bound catches reading an unrelated counter.
    REQUIRE(instructions >= iterations);
    REQUIRE(instructions < iterations * 100U);
  }

  SECTION("live_info snapshot can be reused across rdpmc measurements while unchanged")
  {
    auto event_counter = perf::EventCounter{};
    event_counter.add_live(std::vector<std::string>{ "instructions", "cycles" });

    event_counter.start();

    constexpr auto iterations = std::uint64_t{ 10000U };
    constexpr auto measurements = 1000U;

    auto snapshot = std::vector<perf::LiveCounterInfo>(2U);
    auto current = std::vector<perf::LiveCounterInfo>(2U);

    /// The rdpmc indices (ECX values) are derived once per snapshot, not per measurement.
    auto rdpmc_indices = std::array<std::uint32_t, 2U>{};
    auto masks = std::array<std::uint64_t, 2U>{};
    const auto take_snapshot = [&]() {
      event_counter.live_info(snapshot);
      for (auto counter = 0U; counter < 2U; ++counter) {
        const auto width = snapshot[counter].width();
        rdpmc_indices[counter] = snapshot[counter].index() - 1U;
        masks[counter] = width >= 64U ? ~std::uint64_t{ 0U } : (std::uint64_t{ 1U } << width) - 1U;
      }
    };

    take_snapshot();
    auto snapshots = 1U;

    auto kept_instructions = std::vector<std::uint64_t>{};
    auto kept_cycles = std::vector<std::uint64_t>{};
    kept_instructions.reserve(measurements);
    kept_cycles.reserve(measurements);

    for (auto measurement = 0U; measurement < measurements; ++measurement) {
      /// Without valid info (index 0), rdpmc would read an arbitrary counter; take a new snapshot instead.
      if (snapshot[0].index() == 0U || snapshot[1].index() == 0U) {
        take_snapshot();
        ++snapshots;
        continue;
      }

      /// Only bare rdpmc instructions surround the measured region.
      const auto instructions_before = read_pmc(rdpmc_indices[0]);
      const auto cycles_before = read_pmc(rdpmc_indices[1]);
      for (auto volatile i = std::uint64_t{ 0U }; i < iterations; i = i + 1U) {
      }
      const auto cycles_after = read_pmc(rdpmc_indices[1]);
      const auto instructions_after = read_pmc(rdpmc_indices[0]);

      /// The measurement is valid only if no counter was rescheduled since the (possibly old) snapshot.
      event_counter.live_info(current);
      if (current == snapshot) {
        kept_instructions.push_back((instructions_after - instructions_before) & masks[0]);
        kept_cycles.push_back((cycles_after - cycles_before) & masks[1]);
      } else {
        take_snapshot();
        ++snapshots;
      }
    }

    event_counter.stop();

    /// Most measurements must be kept and snapshots must actually be reused, i.e., far fewer snapshots than
    /// measurements were taken.
    REQUIRE(kept_instructions.size() >= measurements / 2U);
    REQUIRE(snapshots < kept_instructions.size());

    /// Every kept measurement must be plausible, including those that relied on an old snapshot.
    const auto [min_instructions, max_instructions] =
      std::minmax_element(kept_instructions.begin(), kept_instructions.end());
    REQUIRE(*min_instructions >= iterations);
    REQUIRE(*max_instructions < iterations * 100U);

    const auto [min_cycles, max_cycles] = std::minmax_element(kept_cycles.begin(), kept_cycles.end());
    REQUIRE(*min_cycles > 0U);
    REQUIRE(*max_cycles < iterations * 1000U);
  }
#endif
}
