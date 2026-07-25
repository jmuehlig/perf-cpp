#include "access_benchmark.hpp"
#include <catch2/catch_test_macros.hpp>
#include <perfcpp/hardware_info.hpp>
#include <perfcpp/sampler.hpp>

TEST_CASE("sample fields", "[SampleFields]")
{
  if (!(perf::HardwareInfo::is_intel() || perf::HardwareInfo::is_amd())) {
    SKIP("Sampler is not implemented for non-x86 hardware.");
  }

  auto benchmark = perf::test::AccessBenchmark{ /* is random */ true, 512U /* MB */ };

  SECTION("logical instruction pointer")
  {
    auto sampler = perf::Sampler{};
    REQUIRE_NOTHROW(sampler.trigger(perf::Cycles{}, perf::Period{ 100000U }));
    sampler.values().logical_instruction_pointer(true);

    REQUIRE_NOTHROW(sampler.open());
    REQUIRE_NOTHROW(sampler.start());
    benchmark.run();
    REQUIRE_NOTHROW(sampler.stop());

    const auto samples = sampler.result();
    REQUIRE_FALSE(samples.empty());

    for (const auto& sample : samples) {
      CHECK(sample.instruction_execution().logical_instruction_pointer().has_value());
      if (sample.instruction_execution().logical_instruction_pointer().has_value()) {
        CHECK(sample.instruction_execution().logical_instruction_pointer().value() != 0U);
      }

      /// Other fields must not be populated.
      CHECK_FALSE(sample.metadata().timestamp().has_value());
      CHECK_FALSE(sample.metadata().cpu_id().has_value());
      CHECK_FALSE(sample.metadata().process_id().has_value());
      CHECK_FALSE(sample.metadata().thread_id().has_value());
      CHECK_FALSE(sample.metadata().period().has_value());
      CHECK_FALSE(sample.metadata().sample_id().has_value());
      CHECK_FALSE(sample.metadata().stream_id().has_value());
      CHECK_FALSE(sample.instruction_execution().callchain().has_value());
      CHECK_FALSE(sample.user_registers().has_value());
      CHECK_FALSE(sample.kernel_registers().has_value());
      CHECK_FALSE(sample.branch_stack().has_value());
      CHECK_FALSE(sample.user_stack().has_value());
    }

    REQUIRE_NOTHROW(sampler.close());
  }

  SECTION("timestamp")
  {
    auto sampler = perf::Sampler{};
    REQUIRE_NOTHROW(sampler.trigger(perf::Cycles{}, perf::Period{ 100000U }));
    sampler.values().timestamp(true);

    REQUIRE_NOTHROW(sampler.open());
    REQUIRE_NOTHROW(sampler.start());
    benchmark.run();
    REQUIRE_NOTHROW(sampler.stop());

    const auto samples = sampler.result();
    REQUIRE_FALSE(samples.empty());

    for (const auto& sample : samples) {
      CHECK(sample.metadata().timestamp().has_value());
      if (sample.metadata().timestamp().has_value()) {
        CHECK(sample.metadata().timestamp().value() > 0U);
      }

      /// Other fields must not be populated.
      CHECK_FALSE(sample.instruction_execution().logical_instruction_pointer().has_value());
      CHECK_FALSE(sample.metadata().cpu_id().has_value());
      CHECK_FALSE(sample.metadata().process_id().has_value());
      CHECK_FALSE(sample.metadata().thread_id().has_value());
      CHECK_FALSE(sample.metadata().period().has_value());
      CHECK_FALSE(sample.metadata().sample_id().has_value());
      CHECK_FALSE(sample.metadata().stream_id().has_value());
      CHECK_FALSE(sample.instruction_execution().callchain().has_value());
      CHECK_FALSE(sample.user_registers().has_value());
      CHECK_FALSE(sample.kernel_registers().has_value());
      CHECK_FALSE(sample.branch_stack().has_value());
      CHECK_FALSE(sample.user_stack().has_value());
    }

    REQUIRE_NOTHROW(sampler.close());
  }

  SECTION("cpu id")
  {
    auto sampler = perf::Sampler{};
    REQUIRE_NOTHROW(sampler.trigger(perf::Cycles{}, perf::Period{ 100000U }));
    sampler.values().cpu_id(true);

    REQUIRE_NOTHROW(sampler.open());
    REQUIRE_NOTHROW(sampler.start());
    benchmark.run();
    REQUIRE_NOTHROW(sampler.stop());

    const auto samples = sampler.result();
    REQUIRE_FALSE(samples.empty());

    for (const auto& sample : samples) {
      CHECK(sample.metadata().cpu_id().has_value());

      /// Other fields must not be populated.
      CHECK_FALSE(sample.instruction_execution().logical_instruction_pointer().has_value());
      CHECK_FALSE(sample.metadata().timestamp().has_value());
      CHECK_FALSE(sample.metadata().process_id().has_value());
      CHECK_FALSE(sample.metadata().thread_id().has_value());
      CHECK_FALSE(sample.metadata().period().has_value());
      CHECK_FALSE(sample.metadata().sample_id().has_value());
      CHECK_FALSE(sample.metadata().stream_id().has_value());
      CHECK_FALSE(sample.instruction_execution().callchain().has_value());
      CHECK_FALSE(sample.user_registers().has_value());
      CHECK_FALSE(sample.kernel_registers().has_value());
      CHECK_FALSE(sample.branch_stack().has_value());
      CHECK_FALSE(sample.user_stack().has_value());
    }

    REQUIRE_NOTHROW(sampler.close());
  }

  SECTION("thread id")
  {
    auto sampler = perf::Sampler{};
    REQUIRE_NOTHROW(sampler.trigger(perf::Cycles{}, perf::Period{ 100000U }));
    sampler.values().thread_id(true);

    REQUIRE_NOTHROW(sampler.open());
    REQUIRE_NOTHROW(sampler.start());
    benchmark.run();
    REQUIRE_NOTHROW(sampler.stop());

    const auto samples = sampler.result();
    REQUIRE_FALSE(samples.empty());

    for (const auto& sample : samples) {
      /// Both process and thread ID come from the same PERF_SAMPLE_TID field.
      CHECK(sample.metadata().process_id().has_value());
      CHECK(sample.metadata().thread_id().has_value());
      if (sample.metadata().process_id().has_value()) {
        CHECK(sample.metadata().process_id().value() > 0U);
      }
      if (sample.metadata().thread_id().has_value()) {
        CHECK(sample.metadata().thread_id().value() > 0U);
      }

      /// Other fields must not be populated.
      CHECK_FALSE(sample.instruction_execution().logical_instruction_pointer().has_value());
      CHECK_FALSE(sample.metadata().timestamp().has_value());
      CHECK_FALSE(sample.metadata().cpu_id().has_value());
      CHECK_FALSE(sample.metadata().period().has_value());
      CHECK_FALSE(sample.metadata().sample_id().has_value());
      CHECK_FALSE(sample.metadata().stream_id().has_value());
      CHECK_FALSE(sample.instruction_execution().callchain().has_value());
      CHECK_FALSE(sample.user_registers().has_value());
      CHECK_FALSE(sample.kernel_registers().has_value());
      CHECK_FALSE(sample.branch_stack().has_value());
      CHECK_FALSE(sample.user_stack().has_value());
    }

    REQUIRE_NOTHROW(sampler.close());
  }

  SECTION("period")
  {
    auto sampler = perf::Sampler{};
    REQUIRE_NOTHROW(sampler.trigger(perf::Cycles{}, perf::Period{ 100000U }));
    sampler.values().period(true);

    REQUIRE_NOTHROW(sampler.open());
    REQUIRE_NOTHROW(sampler.start());
    benchmark.run();
    REQUIRE_NOTHROW(sampler.stop());

    const auto samples = sampler.result();
    REQUIRE_FALSE(samples.empty());

    for (const auto& sample : samples) {
      CHECK(sample.metadata().period().has_value());
      if (sample.metadata().period().has_value()) {
        CHECK(sample.metadata().period().value() > 0U);
      }

      /// Other fields must not be populated.
      CHECK_FALSE(sample.instruction_execution().logical_instruction_pointer().has_value());
      CHECK_FALSE(sample.metadata().timestamp().has_value());
      CHECK_FALSE(sample.metadata().cpu_id().has_value());
      CHECK_FALSE(sample.metadata().process_id().has_value());
      CHECK_FALSE(sample.metadata().thread_id().has_value());
      CHECK_FALSE(sample.metadata().sample_id().has_value());
      CHECK_FALSE(sample.metadata().stream_id().has_value());
      CHECK_FALSE(sample.instruction_execution().callchain().has_value());
      CHECK_FALSE(sample.user_registers().has_value());
      CHECK_FALSE(sample.kernel_registers().has_value());
      CHECK_FALSE(sample.branch_stack().has_value());
      CHECK_FALSE(sample.user_stack().has_value());
    }

    REQUIRE_NOTHROW(sampler.close());
  }

  SECTION("id")
  {
    auto sampler = perf::Sampler{};
    REQUIRE_NOTHROW(sampler.trigger(perf::Cycles{}, perf::Period{ 100000U }));
    sampler.values().sample_id(true);

    REQUIRE_NOTHROW(sampler.open());
    REQUIRE_NOTHROW(sampler.start());
    benchmark.run();
    REQUIRE_NOTHROW(sampler.stop());

    const auto samples = sampler.result();
    REQUIRE_FALSE(samples.empty());

    for (const auto& sample : samples) {
      CHECK(sample.metadata().sample_id().has_value());

      /// Other fields must not be populated.
      CHECK_FALSE(sample.instruction_execution().logical_instruction_pointer().has_value());
      CHECK_FALSE(sample.metadata().timestamp().has_value());
      CHECK_FALSE(sample.metadata().cpu_id().has_value());
      CHECK_FALSE(sample.metadata().process_id().has_value());
      CHECK_FALSE(sample.metadata().thread_id().has_value());
      CHECK_FALSE(sample.metadata().period().has_value());
      CHECK_FALSE(sample.metadata().stream_id().has_value());
      CHECK_FALSE(sample.instruction_execution().callchain().has_value());
      CHECK_FALSE(sample.user_registers().has_value());
      CHECK_FALSE(sample.kernel_registers().has_value());
      CHECK_FALSE(sample.branch_stack().has_value());
      CHECK_FALSE(sample.user_stack().has_value());
    }

    REQUIRE_NOTHROW(sampler.close());
  }

  SECTION("stream id")
  {
    auto sampler = perf::Sampler{};
    REQUIRE_NOTHROW(sampler.trigger(perf::Cycles{}, perf::Period{ 100000U }));
    sampler.values().stream_id(true);

    REQUIRE_NOTHROW(sampler.open());
    REQUIRE_NOTHROW(sampler.start());
    benchmark.run();
    REQUIRE_NOTHROW(sampler.stop());

    const auto samples = sampler.result();
    REQUIRE_FALSE(samples.empty());

    for (const auto& sample : samples) {
      CHECK(sample.metadata().stream_id().has_value());

      /// Other fields must not be populated.
      CHECK_FALSE(sample.instruction_execution().logical_instruction_pointer().has_value());
      CHECK_FALSE(sample.metadata().timestamp().has_value());
      CHECK_FALSE(sample.metadata().cpu_id().has_value());
      CHECK_FALSE(sample.metadata().process_id().has_value());
      CHECK_FALSE(sample.metadata().thread_id().has_value());
      CHECK_FALSE(sample.metadata().period().has_value());
      CHECK_FALSE(sample.metadata().sample_id().has_value());
      CHECK_FALSE(sample.instruction_execution().callchain().has_value());
      CHECK_FALSE(sample.user_registers().has_value());
      CHECK_FALSE(sample.kernel_registers().has_value());
      CHECK_FALSE(sample.branch_stack().has_value());
      CHECK_FALSE(sample.user_stack().has_value());
    }

    REQUIRE_NOTHROW(sampler.close());
  }

  SECTION("branch stack")
  {
    auto sampler = perf::Sampler{};
    REQUIRE_NOTHROW(sampler.trigger(perf::Cycles{}, perf::Precision::AllowArbitrarySkid, perf::Period{ 1000000U }));
    sampler.values().branch_stack({ perf::BranchType::User, perf::BranchType::Conditional });

    REQUIRE_NOTHROW(sampler.open());
    REQUIRE_NOTHROW(sampler.start());
    benchmark.run();
    REQUIRE_NOTHROW(sampler.stop());

    const auto samples = sampler.result();
    REQUIRE_FALSE(samples.empty());

    for (const auto& sample : samples) {
      CHECK(sample.branch_stack().has_value());
      if (sample.branch_stack().has_value()) {
        CHECK_FALSE(sample.branch_stack()->empty());
      }

      /// Other fields must not be populated.
      CHECK_FALSE(sample.instruction_execution().logical_instruction_pointer().has_value());
      CHECK_FALSE(sample.metadata().timestamp().has_value());
      CHECK_FALSE(sample.metadata().cpu_id().has_value());
      CHECK_FALSE(sample.metadata().process_id().has_value());
      CHECK_FALSE(sample.metadata().thread_id().has_value());
      CHECK_FALSE(sample.metadata().period().has_value());
      CHECK_FALSE(sample.metadata().sample_id().has_value());
      CHECK_FALSE(sample.metadata().stream_id().has_value());
      CHECK_FALSE(sample.instruction_execution().callchain().has_value());
      CHECK_FALSE(sample.user_registers().has_value());
      CHECK_FALSE(sample.kernel_registers().has_value());
      CHECK_FALSE(sample.user_stack().has_value());
    }

    REQUIRE_NOTHROW(sampler.close());
  }

  SECTION("user registers")
  {
    auto sampler = perf::Sampler{};
    REQUIRE_NOTHROW(sampler.trigger(perf::Cycles{}, perf::Period{ 100000U }));
    sampler.values().user_registers(perf::Registers{ std::vector<perf::Registers::x86>{ perf::Registers::x86::IP } });

    REQUIRE_NOTHROW(sampler.open());
    REQUIRE_NOTHROW(sampler.start());
    benchmark.run();
    REQUIRE_NOTHROW(sampler.stop());

    const auto samples = sampler.result();
    REQUIRE_FALSE(samples.empty());

    for (const auto& sample : samples) {
      /// User registers are always decoded (ABI may be None for kernel-mode samples).
      CHECK(sample.user_registers().has_value());

      if (sample.user_registers().has_value() && sample.metadata().mode() == perf::Metadata::Mode::User &&
          sample.user_registers()->abi() != perf::ABI::None) {
        CHECK(sample.user_registers()->get(perf::Registers::x86::IP).has_value());
        if (sample.user_registers()->get(perf::Registers::x86::IP).has_value()) {
          CHECK(sample.user_registers()->get(perf::Registers::x86::IP).value() != 0);
        }
      }

      /// Other fields must not be populated.
      CHECK_FALSE(sample.instruction_execution().logical_instruction_pointer().has_value());
      CHECK_FALSE(sample.metadata().timestamp().has_value());
      CHECK_FALSE(sample.metadata().cpu_id().has_value());
      CHECK_FALSE(sample.metadata().process_id().has_value());
      CHECK_FALSE(sample.metadata().thread_id().has_value());
      CHECK_FALSE(sample.metadata().period().has_value());
      CHECK_FALSE(sample.metadata().sample_id().has_value());
      CHECK_FALSE(sample.metadata().stream_id().has_value());
      CHECK_FALSE(sample.instruction_execution().callchain().has_value());
      CHECK_FALSE(sample.kernel_registers().has_value());
      CHECK_FALSE(sample.branch_stack().has_value());
      CHECK_FALSE(sample.user_stack().has_value());
    }

    REQUIRE_NOTHROW(sampler.close());
  }

  SECTION("kernel registers")
  {
    auto sampler = perf::Sampler{};
    REQUIRE_NOTHROW(sampler.trigger(perf::Cycles{}, perf::Period{ 100000U }));
    sampler.values().kernel_registers(perf::Registers{ std::vector<perf::Registers::x86>{ perf::Registers::x86::IP } });

    REQUIRE_NOTHROW(sampler.open());
    REQUIRE_NOTHROW(sampler.start());
    benchmark.run();
    REQUIRE_NOTHROW(sampler.stop());

    const auto samples = sampler.result();
    REQUIRE_FALSE(samples.empty());

    for (const auto& sample : samples) {
      /// Kernel registers are always decoded (ABI may be None for user-mode samples).
      CHECK(sample.kernel_registers().has_value());

      if (sample.kernel_registers().has_value() && sample.metadata().mode() == perf::Metadata::Mode::Kernel &&
          sample.kernel_registers()->abi() != perf::ABI::None) {
        CHECK(sample.kernel_registers()->get(perf::Registers::x86::IP).has_value());
        if (sample.kernel_registers()->get(perf::Registers::x86::IP).has_value()) {
          CHECK(sample.kernel_registers()->get(perf::Registers::x86::IP).value() != 0);
        }
      }

      /// Other fields must not be populated.
      CHECK_FALSE(sample.instruction_execution().logical_instruction_pointer().has_value());
      CHECK_FALSE(sample.metadata().timestamp().has_value());
      CHECK_FALSE(sample.metadata().cpu_id().has_value());
      CHECK_FALSE(sample.metadata().process_id().has_value());
      CHECK_FALSE(sample.metadata().thread_id().has_value());
      CHECK_FALSE(sample.metadata().period().has_value());
      CHECK_FALSE(sample.metadata().sample_id().has_value());
      CHECK_FALSE(sample.metadata().stream_id().has_value());
      CHECK_FALSE(sample.instruction_execution().callchain().has_value());
      CHECK_FALSE(sample.user_registers().has_value());
      CHECK_FALSE(sample.branch_stack().has_value());
      CHECK_FALSE(sample.user_stack().has_value());
    }

    REQUIRE_NOTHROW(sampler.close());
  }

  SECTION("callchain")
  {
    auto sampler = perf::Sampler{};
    REQUIRE_NOTHROW(sampler.trigger(perf::Cycles{}, perf::Period{ 100000U }));
    sampler.values().callchain(std::uint16_t{ 32U });

    REQUIRE_NOTHROW(sampler.open());
    REQUIRE_NOTHROW(sampler.start());
    benchmark.run();
    REQUIRE_NOTHROW(sampler.stop());

    const auto samples = sampler.result();
    REQUIRE_FALSE(samples.empty());

    for (const auto& sample : samples) {
      /// Callchain may be absent if the recorded depth is zero for that sample.
      if (sample.instruction_execution().callchain().has_value()) {
        CHECK_FALSE(sample.instruction_execution().callchain()->empty());
      }

      /// Other fields must not be populated.
      CHECK_FALSE(sample.instruction_execution().logical_instruction_pointer().has_value());
      CHECK_FALSE(sample.metadata().timestamp().has_value());
      CHECK_FALSE(sample.metadata().cpu_id().has_value());
      CHECK_FALSE(sample.metadata().process_id().has_value());
      CHECK_FALSE(sample.metadata().thread_id().has_value());
      CHECK_FALSE(sample.metadata().period().has_value());
      CHECK_FALSE(sample.metadata().sample_id().has_value());
      CHECK_FALSE(sample.metadata().stream_id().has_value());
      CHECK_FALSE(sample.user_registers().has_value());
      CHECK_FALSE(sample.kernel_registers().has_value());
      CHECK_FALSE(sample.branch_stack().has_value());
      CHECK_FALSE(sample.user_stack().has_value());
    }

    /// At least some samples must carry a non-empty callchain.
    REQUIRE(std::any_of(samples.begin(), samples.end(), [](const auto& s) {
      return s.instruction_execution().callchain().has_value() && !s.instruction_execution().callchain()->empty();
    }));

    REQUIRE_NOTHROW(sampler.close());
  }

  SECTION("user stack")
  {
    auto sampler = perf::Sampler{};
    REQUIRE_NOTHROW(sampler.trigger(perf::Cycles{}, perf::Period{ 100000U }));
    sampler.values().user_stack(512U);

    REQUIRE_NOTHROW(sampler.open());
    REQUIRE_NOTHROW(sampler.start());
    benchmark.run();
    REQUIRE_NOTHROW(sampler.stop());

    const auto samples = sampler.result();
    REQUIRE_FALSE(samples.empty());

    for (const auto& sample : samples) {
      /// User stack may be absent if the dynamic stack size is zero for that sample.
      if (sample.user_stack().has_value()) {
        CHECK_FALSE(sample.user_stack()->empty());
      }

      /// Other fields must not be populated.
      CHECK_FALSE(sample.instruction_execution().logical_instruction_pointer().has_value());
      CHECK_FALSE(sample.metadata().timestamp().has_value());
      CHECK_FALSE(sample.metadata().cpu_id().has_value());
      CHECK_FALSE(sample.metadata().process_id().has_value());
      CHECK_FALSE(sample.metadata().thread_id().has_value());
      CHECK_FALSE(sample.metadata().period().has_value());
      CHECK_FALSE(sample.metadata().sample_id().has_value());
      CHECK_FALSE(sample.metadata().stream_id().has_value());
      CHECK_FALSE(sample.instruction_execution().callchain().has_value());
      CHECK_FALSE(sample.user_registers().has_value());
      CHECK_FALSE(sample.kernel_registers().has_value());
      CHECK_FALSE(sample.branch_stack().has_value());
    }

    /// At least some samples must carry user stack bytes.
    REQUIRE(std::any_of(samples.begin(), samples.end(), [](const auto& s) { return s.user_stack().has_value(); }));

    REQUIRE_NOTHROW(sampler.close());
  }
}
