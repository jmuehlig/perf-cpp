#include "access_benchmark.h"
#include <catch2/catch_test_macros.hpp>
#include <perfcpp/sampler.h>

TEST_CASE("sampling", "[Sampler]")
{
  /// Benchmark used for all sampling tests.
  auto readonly_benchmark = perf::test::AccessBenchmark{ /* is random */ true, 1024U /* MB */ };

  /// Counter definitions used for all sampling tests.
  const auto counter_definition = perf::CounterDefinition{};

  SECTION("IP with cycles")
  {
    auto sampler = perf::Sampler{ counter_definition };
    REQUIRE_NOTHROW(sampler.trigger("cycles"));
    sampler.values().instruction_pointer(true);

    REQUIRE_NOTHROW(sampler.open());
    REQUIRE_NOTHROW(sampler.start());

    readonly_benchmark.run();

    REQUIRE_NOTHROW(sampler.stop());

    const auto samples = sampler.result();
    REQUIRE_FALSE(samples.empty());
    for (const auto& sample : samples) {
      REQUIRE_FALSE(sample.metadata().timestamp().has_value());
      REQUIRE(sample.instruction_execution().logical_instruction_pointer().has_value());
    }

    REQUIRE_NOTHROW(sampler.close());
  }

  SECTION("re-use")
  {
    auto sampler = perf::Sampler{ counter_definition };
    REQUIRE_NOTHROW(sampler.trigger("cycles"));
    sampler.values().instruction_pointer(true).time(true);

    REQUIRE_NOTHROW(sampler.open());

    REQUIRE_NOTHROW(sampler.start());
    readonly_benchmark.run();
    REQUIRE_NOTHROW(sampler.stop());

    const auto samples1 = sampler.result(true);
    REQUIRE_FALSE(samples1.empty());
    REQUIRE(samples1.back().metadata().timestamp().has_value());

    REQUIRE_NOTHROW(sampler.start());
    readonly_benchmark.run();
    REQUIRE_NOTHROW(sampler.stop());

    const auto samples2 = sampler.result(true);
    REQUIRE_FALSE(samples2.empty());

    REQUIRE(samples2.front().metadata().timestamp().has_value());
    REQUIRE(samples1.back().metadata().timestamp().value() < samples2.front().metadata().timestamp().value());

    REQUIRE_NOTHROW(sampler.close());
  }
}