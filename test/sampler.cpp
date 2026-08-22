#include "access_benchmark.hpp"
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <perfcpp/exception.hpp>
#include <perfcpp/hardware_info.hpp>
#include <perfcpp/sampler.hpp>
#include <sched.h>
#include <string>
#include <thread>

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

class AverageCounter
{
public:
  AverageCounter& operator+=(const std::uint64_t value) noexcept
  {
    _sum += value;
    ++_count;
    return *this;
  }

  [[nodiscard]] std::uint64_t get() const noexcept { return _count > 0ULL ? _sum / _count : 0ULL; }

private:
  std::uint64_t _sum{ 0U };
  std::uint64_t _count{ 0U };
};

TEST_CASE("config", "[Sampler]")
{
  if (!(perf::HardwareInfo::is_intel() || perf::HardwareInfo::is_amd())) {
    SKIP("Sampler is not implemented for non-x86 hardware.");
  }

  /// Benchmark used for all sampling tests.
  /// Shared across SECTIONs: Catch2 re-runs this TEST_CASE body once per SECTION, so a non-static instance
  /// would re-allocate and re-shuffle this multi-hundred-MB benchmark for every sibling SECTION.
  static auto readonly_benchmark = perf::test::AccessBenchmark{ /* is random */ true, 1024U /* MB */ };

  SECTION("empty sampler")
  {
    auto sampler = perf::Sampler{};
    sampler.values().logical_instruction_pointer(true);

    REQUIRE_THROWS(sampler.open());
  }

  SECTION("start without prior open")
  {
    /// start() implicitly calls open(); no exception expected.
    auto sampler = perf::Sampler{};
    sampler.trigger("cycles");
    sampler.values().logical_instruction_pointer(true);

    REQUIRE_NOTHROW(sampler.start());
    readonly_benchmark.run();
    REQUIRE_NOTHROW(sampler.stop());
    sampler.close();
  }

  SECTION("double start without stop")
  {
    /// Second start() clears collected samples and re-enables; no exception expected.
    auto sampler = perf::Sampler{};
    sampler.trigger("cycles");
    sampler.values().logical_instruction_pointer(true);

    REQUIRE_NOTHROW(sampler.start());
    REQUIRE_NOTHROW(sampler.start());
    readonly_benchmark.run();
    REQUIRE_NOTHROW(sampler.stop());
    sampler.close();
  }

  SECTION("stop without prior start")
  {
    /// stop() on a sampler that was never opened/started must be a safe no-op.
    auto sampler = perf::Sampler{};
    sampler.trigger("cycles");
    sampler.values().logical_instruction_pointer(true);

    REQUIRE_NOTHROW(sampler.stop());
    REQUIRE_NOTHROW(sampler.close());
  }

  SECTION("result before any run")
  {
    /// result() on a sampler that was never opened/started/stopped must be safe and empty: nothing was
    /// recorded, so an empty result is the truth here – unlike after close(), where it would hide samples.
    auto sampler = perf::Sampler{};
    sampler.trigger("cycles");
    sampler.values().logical_instruction_pointer(true);

    const auto samples = sampler.result();
    REQUIRE(samples.empty());
    REQUIRE(samples.size() == 0U);
  }

  SECTION("result after close")
  {
    /// result() decodes from the sample buffer, which close() tears down; calling result() after close()
    /// is outside the documented contract ("after stopping, before closing the sampler") and must throw
    /// rather than silently return an empty result that hides the recorded samples.
    auto sampler = perf::Sampler{};
    sampler.trigger("cycles");
    sampler.values().logical_instruction_pointer(true);

    REQUIRE_NOTHROW(sampler.start());
    readonly_benchmark.run();
    REQUIRE_NOTHROW(sampler.stop());

    /// The result must be available as long as the sampler is not closed.
    REQUIRE_FALSE(sampler.result().empty());

    REQUIRE_NOTHROW(sampler.close());

    REQUIRE_THROWS_AS(sampler.result(), perf::CannotGetResultFromClosedSamplerError);
  }

  SECTION("perf file export after close")
  {
    /// to_perf_file() reads the same sample buffers as result() and must report the wrong call order, too –
    /// otherwise it would write a well-formed but empty perf file that looks like a successful export.
    const auto perf_file = std::filesystem::temp_directory_path() / "perf-cpp-test-export-after-close.data";

    auto sampler = perf::Sampler{};
    sampler.trigger("cycles");
    sampler.values().logical_instruction_pointer(true);

    REQUIRE_NOTHROW(sampler.start());
    readonly_benchmark.run();
    REQUIRE_NOTHROW(sampler.stop());
    REQUIRE_NOTHROW(sampler.close());

    REQUIRE_THROWS_AS(sampler.to_perf_file(perf_file.string()), perf::CannotGetResultFromClosedSamplerError);

    /// Nothing must have been written.
    REQUIRE_FALSE(std::filesystem::exists(perf_file));
  }

  SECTION("result of a re-opened sampler")
  {
    /// close() must not turn the sampler into a dead object: opening it again resets the state and makes
    /// the samples of the second run available.
    auto sampler = perf::Sampler{};
    sampler.trigger("cycles");
    sampler.values().logical_instruction_pointer(true);

    REQUIRE_NOTHROW(sampler.start());
    readonly_benchmark.run();
    REQUIRE_NOTHROW(sampler.stop());
    REQUIRE_NOTHROW(sampler.close());

    REQUIRE_NOTHROW(sampler.start());
    readonly_benchmark.run();
    REQUIRE_NOTHROW(sampler.stop());

    REQUIRE_FALSE(sampler.result().empty());

    REQUIRE_NOTHROW(sampler.close());
  }

  SECTION("zero period rejected")
  {
    auto sampler = perf::Sampler{};
    sampler.trigger(perf::Cycles{}, perf::Period{ 0U });
    sampler.values().logical_instruction_pointer(true);

    REQUIRE_THROWS_AS(sampler.open(), perf::InvalidSamplingPeriodError);
  }

  SECTION("zero frequency rejected")
  {
    auto sampler = perf::Sampler{};
    sampler.trigger(perf::Cycles{}, perf::Frequency{ 0U });
    sampler.values().logical_instruction_pointer(true);

    REQUIRE_THROWS_AS(sampler.open(), perf::InvalidSamplingFrequencyError);
  }

  SECTION("frequency exceeds system maximum")
  {
    auto sampler = perf::Sampler{};
    sampler.trigger(perf::Cycles{}, perf::Frequency{ perf::HardwareInfo::max_perf_sample_rate() + 1U });
    sampler.values().logical_instruction_pointer(true);

    REQUIRE_THROWS_AS(sampler.open(), perf::SamplingFrequencyExceedsMaximumError);
  }
}

TEST_CASE("sampling", "[Sampler]")
{
  if (!(perf::HardwareInfo::is_intel() || perf::HardwareInfo::is_amd())) {
    SKIP("Sampler is not implemented for non-x86 hardware.");
  }

  /// Benchmark used for all sampling tests.
  /// Shared across SECTIONs: Catch2 re-runs this TEST_CASE body once per SECTION, so a non-static instance
  /// would re-allocate and re-shuffle this multi-hundred-MB benchmark for every sibling SECTION.
  static auto readonly_benchmark = perf::test::AccessBenchmark{ /* is random */ true, 2048 /* MB */ };

  SECTION("IP with cycles")
  {
    auto sampler = perf::Sampler{};
    REQUIRE_NOTHROW(sampler.trigger("cycles"));
    sampler.values().logical_instruction_pointer(true);

    REQUIRE_NOTHROW(sampler.open());
    REQUIRE_NOTHROW(sampler.start());

    readonly_benchmark.run();

    REQUIRE_NOTHROW(sampler.stop());

    const auto samples = sampler.result();
    REQUIRE_FALSE(samples.empty());
    for (const auto& sample : samples) {
      CHECK_FALSE(sample.metadata().timestamp().has_value());
      CHECK(sample.instruction_execution().logical_instruction_pointer().has_value());
    }

    REQUIRE_NOTHROW(sampler.close());
  }

  SECTION("IP with cycles (typed)")
  {
    auto sampler = perf::Sampler{};
    REQUIRE_NOTHROW(sampler.trigger(perf::Cycles{}));
    sampler.values().logical_instruction_pointer(true);

    REQUIRE_NOTHROW(sampler.open());
    REQUIRE_NOTHROW(sampler.start());

    readonly_benchmark.run();

    REQUIRE_NOTHROW(sampler.stop());

    const auto samples = sampler.result();
    REQUIRE_FALSE(samples.empty());
    for (const auto& sample : samples) {
      CHECK_FALSE(sample.metadata().timestamp().has_value());
      CHECK(sample.instruction_execution().logical_instruction_pointer().has_value());
    }

    REQUIRE_NOTHROW(sampler.close());
  }

  SECTION("re-start")
  {
    auto sampler = perf::Sampler{};
    REQUIRE_NOTHROW(sampler.trigger("cycles"));
    sampler.values().logical_instruction_pointer(true).timestamp(true);

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

  SECTION("sample period")
  {
    auto sampler1 = perf::Sampler{};

    REQUIRE_NOTHROW(sampler1.trigger(perf::Cycles{}, perf::Precision::RequestZeroSkid, perf::Period{ 200000 }));
    sampler1.values().logical_instruction_pointer(true).timestamp(true);

    REQUIRE_NOTHROW(sampler1.open());

    REQUIRE_NOTHROW(sampler1.start());
    readonly_benchmark.run();
    REQUIRE_NOTHROW(sampler1.stop());

    const auto samples1 = sampler1.result(true);
    REQUIRE_FALSE(samples1.empty());
    REQUIRE_NOTHROW(sampler1.close());

    auto sampler2 = perf::Sampler{};

    REQUIRE_NOTHROW(sampler2.trigger("cycles", perf::Precision::RequestZeroSkid, perf::Period{ 800000 }));
    sampler2.values().logical_instruction_pointer(true).timestamp(true);

    REQUIRE_NOTHROW(sampler2.open());

    REQUIRE_NOTHROW(sampler2.start());
    readonly_benchmark.run();
    REQUIRE_NOTHROW(sampler2.stop());

    const auto samples2 = sampler2.result(true);
    REQUIRE_FALSE(samples2.empty());
    REQUIRE_NOTHROW(sampler2.close());

    REQUIRE(samples1.size() > (samples2.size() * 3U));
  }

  SECTION("mem-loads")
  {
    auto sampler = perf::Sampler{};

    if (perf::HardwareInfo::is_intel()) {
      REQUIRE_NOTHROW(sampler.trigger(perf::MemoryLoads{}, perf::Precision::MustHaveZeroSkid, perf::Period{ 16000 }));
    } else if (perf::HardwareInfo::is_amd()) {
      REQUIRE_NOTHROW(
        sampler.trigger(perf::IbsOp{ /*upos = */ true }, perf::Precision::RequestZeroSkid, perf::Period{ 16000 }));
    }

    REQUIRE_NOTHROW(
      sampler.values().logical_memory_address(true).data_source(true).data_access_latency(true).instruction_latency(
        true));
    REQUIRE_NOTHROW(sampler.open());

    REQUIRE_NOTHROW(sampler.start());
    readonly_benchmark.run();
    REQUIRE_NOTHROW(sampler.stop());

    const auto samples = sampler.result();
    REQUIRE_FALSE(samples.empty());

    auto l1d = AverageCounter{};
    auto l2 = AverageCounter{};
    auto l3 = AverageCounter{};
    auto ram = AverageCounter{};

    for (const auto& sample : samples) {
      if (sample.data_access().is_load() && sample.data_access().logical_memory_address().has_value()) {
        if (perf::HardwareInfo::is_intel()) {
          if (perf::HardwareInfo::is_intel_12th_generation_or_newer()) {
            /// REQUIRE, not CHECK: several unconditional .value() reads below depend on this being present.
            REQUIRE(sample.data_access().latency().cache_access().has_value());
            if (sample.data_access().source().has_value()) {
              if (sample.data_access().source()->is_l1_hit()) {
                l1d += sample.data_access().latency().cache_access().value();
              } else if (sample.data_access().source()->is_l2_hit()) {
                l2 += sample.data_access().latency().cache_access().value();
              } else if (sample.data_access().source()->is_l3_hit()) {
                l3 += sample.data_access().latency().cache_access().value();
              } else if (sample.data_access().source()->is_memory_hit()) {
                ram += sample.data_access().latency().cache_access().value();
              }
            }
          } else {
            /// REQUIRE, not CHECK: several unconditional .value() reads below depend on this being present.
            REQUIRE(sample.instruction_execution().latency().instruction_retirement().has_value());
            if (sample.data_access().source().has_value()) {
              if (sample.data_access().source()->is_l1_hit()) {
                l1d += sample.instruction_execution().latency().instruction_retirement().value();
              } else if (sample.data_access().source()->is_l2_hit()) {
                l2 += sample.instruction_execution().latency().instruction_retirement().value();
              } else if (sample.data_access().source()->is_l3_hit()) {
                l3 += sample.instruction_execution().latency().instruction_retirement().value();
              } else if (sample.data_access().source()->is_memory_hit()) {
                ram += sample.instruction_execution().latency().instruction_retirement().value();
              }
            }
          }

        } else if (perf::HardwareInfo::is_amd()) {
          /// REQUIRE, not CHECK: several unconditional .value() reads below depend on this being present.
          REQUIRE(sample.data_access().latency().cache_miss().has_value());
          if (sample.data_access().source().has_value()) {
            if (sample.data_access().source()->is_l1_hit()) {
              l1d += sample.data_access().latency().cache_miss().value_or(0U);
            } else if (sample.data_access().source()->is_l2_hit()) {
              l2 += sample.data_access().latency().cache_miss().value();
            } else if (sample.data_access().source()->is_l3_hit()) {
              l3 += sample.data_access().latency().cache_miss().value();
            } else if (sample.data_access().source()->is_memory_hit()) {
              ram += sample.data_access().latency().cache_miss().value();
            }
          }
        }
      }
    }

    if (perf::HardwareInfo::is_intel()) {
      REQUIRE(l1d.get() < 10U);
    } else if (perf::HardwareInfo::is_amd()) {
      REQUIRE(l1d.get() == 0U);
    }

    REQUIRE(l3.get() < 170U);
    REQUIRE(ram.get() > 170U);
  }

  SECTION("mem-loads-with-filter")
  {
    auto filtered_sampler = perf::Sampler{};
    auto not_filtered_sampler = perf::Sampler{};

    if (perf::HardwareInfo::is_intel()) {
      REQUIRE_NOTHROW(
        filtered_sampler.trigger(perf::MemoryLoads{ 60U }, perf::Precision::MustHaveZeroSkid, perf::Period{ 8000 }));
      REQUIRE_NOTHROW(
        not_filtered_sampler.trigger(perf::MemoryLoads{ 1U }, perf::Precision::MustHaveZeroSkid, perf::Period{ 8000 }));
    } else if (perf::HardwareInfo::is_amd()) {
      REQUIRE_NOTHROW(filtered_sampler.trigger(perf::IbsOp{ /*upos = */ true, /* l3miss only = */ true },
                                               perf::Precision::RequestZeroSkid,
                                               perf::Period{ 16000 }));
      REQUIRE_NOTHROW(not_filtered_sampler.trigger(
        perf::IbsOp{ /*upos = */ true }, perf::Precision::RequestZeroSkid, perf::Period{ 16000 }));
    }

    REQUIRE_NOTHROW(filtered_sampler.values()
                      .logical_memory_address(true)
                      .data_source(true)
                      .data_access_latency(true)
                      .instruction_latency(true));
    REQUIRE_NOTHROW(not_filtered_sampler.values()
                      .logical_memory_address(true)
                      .data_source(true)
                      .data_access_latency(true)
                      .instruction_latency(true));

    // Warmup run
    readonly_benchmark.run();

    // Sample without filter.
    REQUIRE_NOTHROW(not_filtered_sampler.open());

    REQUIRE_NOTHROW(not_filtered_sampler.start());
    readonly_benchmark.run();
    REQUIRE_NOTHROW(not_filtered_sampler.stop());

    const auto not_filtered_samples = not_filtered_sampler.result();
    REQUIRE_FALSE(not_filtered_samples.empty());

    // Sample with filter.
    REQUIRE_NOTHROW(filtered_sampler.open());

    REQUIRE_NOTHROW(filtered_sampler.start());
    readonly_benchmark.run();
    REQUIRE_NOTHROW(filtered_sampler.stop());

    const auto filtered_samples = filtered_sampler.result();
    REQUIRE_FALSE(filtered_samples.empty());

    // Compare sample results
    REQUIRE(filtered_samples.size() < not_filtered_samples.size());
  }

  SECTION("branch stack")
  {
    auto sampler = perf::Sampler{};
    REQUIRE_NOTHROW(sampler.trigger(perf::Cycles{}, perf::Precision::AllowArbitrarySkid, perf::Period{ 1000000U }));
    sampler.values().timestamp(true).branch_stack({ perf::BranchType::User, perf::BranchType::Conditional });

    REQUIRE_NOTHROW(sampler.open());
    REQUIRE_NOTHROW(sampler.start());

    readonly_benchmark.run();

    REQUIRE_NOTHROW(sampler.stop());

    const auto samples = sampler.result();
    REQUIRE_FALSE(samples.empty());

    for (const auto& sample : samples) {
      CHECK(sample.metadata().timestamp().has_value());
      /// REQUIRE, not CHECK: the loop below iterates sample.branch_stack().value() unconditionally.
      REQUIRE(sample.branch_stack().has_value());
      CHECK_FALSE(sample.branch_stack()->empty());

      for (const auto& branch : sample.branch_stack().value()) {
        CHECK(branch.instruction_pointer_from() != 0U);
        CHECK(branch.instruction_pointer_to() != 0U);

        /// Predicted and mispredicted are mutually exclusive.
        CHECK_FALSE((branch.is_predicted() && branch.is_mispredicted()));
      }
    }

    REQUIRE_NOTHROW(sampler.close());
  }

  SECTION("user registers")
  {
    auto sampler = perf::Sampler{};
    REQUIRE_NOTHROW(sampler.trigger(perf::Cycles{}, perf::Period{ 100000U }));
    sampler.values()
      .timestamp(true)
      .user_registers(
        perf::Registers{ std::vector<perf::Registers::x86>{ perf::Registers::x86::IP, perf::Registers::x86::SP } })
      .cpu_id(true);

    REQUIRE_NOTHROW(sampler.open());
    REQUIRE_NOTHROW(sampler.start());

    readonly_benchmark.run();

    REQUIRE_NOTHROW(sampler.stop());

    const auto samples = sampler.result();
    REQUIRE_FALSE(samples.empty());

    for (const auto& sample : samples) {
      CHECK(sample.metadata().timestamp().has_value());
      CHECK(sample.metadata().cpu_id().has_value());
      /// REQUIRE, not CHECK: `registers` below binds .value() and is dereferenced unconditionally for the
      /// rest of this iteration.
      REQUIRE(sample.user_registers().has_value());

      const auto& registers = sample.user_registers().value();

      /// User-mode samples always carry a valid ABI with populated register values.
      if (sample.metadata().mode() == perf::Metadata::Mode::User) {
        CHECK(registers.abi() != perf::ABI::None);
        CHECK(registers.get(perf::Registers::x86::IP).has_value());
        if (registers.get(perf::Registers::x86::IP).has_value()) {
          CHECK(registers.get(perf::Registers::x86::IP).value() != 0);
        }
        CHECK(registers.get(perf::Registers::x86::SP).has_value());
      }

      /// Non-requested register is absent regardless of mode.
      CHECK_FALSE(registers.get(perf::Registers::x86::AX).has_value());
    }

    REQUIRE_NOTHROW(sampler.close());
  }

  SECTION("user and kernel registers")
  {
    auto sampler = perf::Sampler{};
    REQUIRE_NOTHROW(sampler.trigger(perf::Cycles{}, perf::Period{ 100000U }));
    sampler.values()
      .timestamp(true)
      .user_registers(
        perf::Registers{ std::vector<perf::Registers::x86>{ perf::Registers::x86::IP, perf::Registers::x86::SP } })
      .kernel_registers(
        perf::Registers{ std::vector<perf::Registers::x86>{ perf::Registers::x86::IP, perf::Registers::x86::SP } });

    REQUIRE_NOTHROW(sampler.open());
    REQUIRE_NOTHROW(sampler.start());

    readonly_benchmark.run();

    REQUIRE_NOTHROW(sampler.stop());

    const auto samples = sampler.result();
    REQUIRE_FALSE(samples.empty());

    for (const auto& sample : samples) {
      /// REQUIRE, not CHECK: the kernel-mode branch below dereferences user_registers() unconditionally.
      REQUIRE(sample.user_registers().has_value());

      if (sample.metadata().mode() == perf::Metadata::Mode::User) {
        CHECK(sample.user_registers()->abi() != perf::ABI::None);
      }

      /// For kernel-mode samples, both register sets are present and reflect distinct contexts.
      if (sample.metadata().mode() == perf::Metadata::Mode::Kernel && sample.kernel_registers().has_value()) {
        /// User and kernel IPs are in distinct virtual address ranges on x86_64
        /// (userspace < 0x0000800000000000, kernel >= 0xffff000000000000), so they must differ.
        const auto user_ip = sample.user_registers()->get(perf::Registers::x86::IP);
        const auto kernel_ip = sample.kernel_registers()->get(perf::Registers::x86::IP);

        if (user_ip.has_value() && kernel_ip.has_value()) {
          CHECK(user_ip.value() != kernel_ip.value());
        }
      }
    }

    REQUIRE_NOTHROW(sampler.close());
  }

  SECTION("clock monotonic timestamps in CLOCK_MONOTONIC range")
  {
    auto sample_config = perf::SampleConfig{};
    sample_config.clock(perf::Clock::Monotonic);

    auto sampler = perf::Sampler{ sample_config };
    REQUIRE_NOTHROW(sampler.trigger(perf::Cycles{}, perf::Period{ 100000U }));
    sampler.values().timestamp(true);

    REQUIRE_NOTHROW(sampler.open());

    struct timespec ts_before;
    ::clock_gettime(CLOCK_MONOTONIC, &ts_before);
    const auto before_ns =
      static_cast<std::uint64_t>(ts_before.tv_sec) * 1000000000ULL + static_cast<std::uint64_t>(ts_before.tv_nsec);

    REQUIRE_NOTHROW(sampler.start());
    readonly_benchmark.run();
    REQUIRE_NOTHROW(sampler.stop());

    struct timespec ts_after;
    ::clock_gettime(CLOCK_MONOTONIC, &ts_after);
    const auto after_ns =
      static_cast<std::uint64_t>(ts_after.tv_sec) * 1000000000ULL + static_cast<std::uint64_t>(ts_after.tv_nsec);

    const auto samples = sampler.result();
    REQUIRE_FALSE(samples.empty());

    for (const auto& sample : samples) {
      CHECK(sample.metadata().timestamp().has_value());
      if (sample.metadata().timestamp().has_value()) {
        CHECK(sample.metadata().timestamp().value() >= before_ns);
        CHECK(sample.metadata().timestamp().value() <= after_ns);
      }
    }

    REQUIRE_NOTHROW(sampler.close());
  }

  SECTION("metric-l1d-per-load")
  {
    auto counter_definition = perf::CounterDefinition{};
    /// Add metric that calculates the L1d miss ratio.
    counter_definition.add("L1d-misses-per-load", "'L1-dcache-load-misses'/'L1-dcache-loads'");

    auto sampler = perf::Sampler{ counter_definition };

    REQUIRE_NOTHROW(sampler.trigger("cycles", perf::Precision::AllowArbitrarySkid, perf::Period{ 100000 }));
    REQUIRE_NOTHROW(sampler.values().timestamp(true).counter({ "L1d-misses-per-load" }));
    REQUIRE_NOTHROW(sampler.open());

    REQUIRE_NOTHROW(sampler.start());
    readonly_benchmark.run();
    REQUIRE_NOTHROW(sampler.stop());

    const auto samples = sampler.result(true);
    REQUIRE_FALSE(samples.empty());

    auto last_timestamp = std::optional<std::uint64_t>{ std::nullopt };

    for (const auto& sample : samples) {
      CHECK(sample.metadata().timestamp().has_value());
      if (last_timestamp.has_value() && sample.metadata().timestamp().has_value()) {
        CHECK(sample.metadata().timestamp().value() > last_timestamp.value());
      }
      last_timestamp = sample.metadata().timestamp();

      CHECK(sample.counter().has_value());
      if (sample.counter().has_value()) {
        CHECK(sample.counter()->get("L1d-misses-per-load").has_value());
        if (sample.counter()->get("L1d-misses-per-load").has_value()) {
          CHECK(sample.counter()->get("L1d-misses-per-load").value() > 0);
          CHECK(sample.counter()->get("L1d-misses-per-load").value() < 1.1);
        }
      }
    }
  }

  SECTION("context switch")
  {
    auto sampler = perf::Sampler{};
    REQUIRE_NOTHROW(sampler.trigger(perf::Cycles{}, perf::Period{ 100000U }));
    sampler.values().timestamp(true).context_switch(true);

    REQUIRE_NOTHROW(sampler.open());
    REQUIRE_NOTHROW(sampler.start());

    /// Force repeated voluntary switch-out/switch-in of this thread while sampling is active.
    for (auto i = 0U; i < 20U; ++i) {
      readonly_benchmark.run();
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    REQUIRE_NOTHROW(sampler.stop());

    const auto samples = sampler.result();
    REQUIRE_FALSE(samples.empty());

    auto context_switch_sample_count = 0U;
    for (const auto& sample : samples) {
      if (sample.context_switch().has_value()) {
        ++context_switch_sample_count;

        const auto& context_switch = sample.context_switch().value();

        /// Switching in and switching out are mutually exclusive.
        CHECK(context_switch.is_in() == !context_switch.is_out());

        /// Process/thread id are only populated in CPU-wide sampling mode.
        CHECK_FALSE(context_switch.process_id().has_value());
        CHECK_FALSE(context_switch.thread_id().has_value());
      }
    }

    CHECK(context_switch_sample_count > 0U);

    REQUIRE_NOTHROW(sampler.close());
  }
}

TEST_CASE("sampling with cgroup", "[Sampler]")
{
  if (!(perf::HardwareInfo::is_intel() || perf::HardwareInfo::is_amd())) {
    SKIP("Sampler is not implemented for non-x86 hardware.");
  }

  const auto cgroup_path = read_own_cgroup_path();
  REQUIRE(cgroup_path.has_value());

  const auto cpu_id = ::sched_getcpu();
  REQUIRE(cpu_id >= 0);

  /// Cgroup monitoring requires a specific CPU core; pin this thread to the current CPU so the
  /// workload runs on the monitored core.
  auto saved_affinity = cpu_set_t{};
  ::sched_getaffinity(0, sizeof(cpu_set_t), &saved_affinity);

  auto pinned_set = cpu_set_t{};
  CPU_ZERO(&pinned_set);
  CPU_SET(cpu_id, &pinned_set);
  ::sched_setaffinity(0, sizeof(cpu_set_t), &pinned_set);

  auto sample_config = perf::SampleConfig{};
  sample_config.cgroup(perf::CGroupMonitor{ std::filesystem::path{ cgroup_path.value() } });
  sample_config.cpu_core(static_cast<std::uint16_t>(cpu_id));

  auto sampler = perf::Sampler{ sample_config };
  REQUIRE_NOTHROW(sampler.trigger(perf::Cycles{}, perf::Period{ 100000U }));
  sampler.values().timestamp(true).logical_instruction_pointer(true);

  auto readonly_benchmark = perf::test::AccessBenchmark{ /* is random */ true, 1024U /* MB */ };

  REQUIRE_NOTHROW(sampler.open());
  REQUIRE_NOTHROW(sampler.start());

  readonly_benchmark.run();

  REQUIRE_NOTHROW(sampler.stop());

  const auto samples = sampler.result();
  REQUIRE_FALSE(samples.empty());

  for (const auto& sample : samples) {
    CHECK(sample.instruction_execution().logical_instruction_pointer().has_value());
    if (sample.instruction_execution().logical_instruction_pointer().has_value()) {
      CHECK(sample.instruction_execution().logical_instruction_pointer().value() != 0U);
    }
  }

  REQUIRE_NOTHROW(sampler.close());

  ::sched_setaffinity(0, sizeof(cpu_set_t), &saved_affinity);
}
