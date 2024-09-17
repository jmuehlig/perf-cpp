#include "access_benchmark.h"
#include <iostream>
#include <perfcpp/sampler.h>

int
main()
{
  std::cout << "libperf-cpp example: Record perf samples including time, "
               "user_registers, and cpu id for single-threaded random "
               "access to an in-memory array."
            << std::endl;

  /// Initialize counter definitions.
  /// Note that the perf::CounterDefinition holds all counter names and must be
  /// alive until the benchmark finishes.
  auto counter_definitions = perf::CounterDefinition{};

  /// Initialize sampler.
  auto perf_config = perf::SampleConfig{};
  perf_config.precise_ip(0U);   /// precise_ip controls the amount of skid, see
                                /// https://man7.org/linux/man-pages/man2/perf_event_open.2.html
  perf_config.period(1000000U); /// Record every 10000th event.

  auto sampler = perf::Sampler{ counter_definitions, perf_config };
  sampler.trigger("cycles");
  sampler.values()
    .time(true)
    .user_registers(
      perf::Registers{ { perf::Registers::x86::IP, perf::Registers::x86::DI, perf::Registers::x86::R10 } })
    .kernel_registers(
      perf::Registers{ { perf::Registers::x86::IP, perf::Registers::x86::DI, perf::Registers::x86::R10 } })
    .cpu(true);

  /// Create random access benchmark.
  auto benchmark = perf::example::AccessBenchmark{ /*randomize the accesses*/ true,
                                                   /* create benchmark of 512 MB */ 512U };

  /// Start sampling.
  try {
    sampler.start();
  } catch (std::runtime_error& exception) {
    std::cerr << exception.what() << std::endl;
    return 1;
  }

  /// Execute the benchmark (accessing cache lines in a random order).
  auto value = 0ULL;
  for (auto index = 0U; index < benchmark.size(); ++index) {
    value += benchmark[index].value;
  }
  asm volatile(""
               : "+r,m"(value)
               :
               : "memory"); /// We do not want the compiler to optimize away
                            /// this unused value.

  /// Stop sampling.
  sampler.stop();

  /// Get all the recorded samples.
  const auto samples = sampler.result();

  /// Print the first samples.
  const auto count_show_samples = std::min<std::size_t>(samples.size(), 40U);
  std::cout << "\nRecorded " << samples.size() << " samples." << std::endl;
  std::cout << "Here are the first " << count_show_samples << " recorded samples:\n" << std::endl;
  for (auto index = 0U; index < count_show_samples; ++index) {
    const auto& sample = samples[index];

    /// Since we recorded the time, period, the instruction pointer, and the CPU
    /// id, we can only read these values.
    if (sample.time().has_value() && (sample.user_registers().has_value() || sample.kernel_registers().has_value()) &&
        sample.cpu_id().has_value()) {

      std::cout << "Time = " << sample.time().value() << " | CPU ID = " << sample.cpu_id().value();

      if (sample.user_registers().has_value()) {
        const auto& user_registers = sample.user_registers().value();
        std::cout << " | User Registers = IP(" << user_registers[0U] << "), DI(" << user_registers[1U] << "), R10("
                  << user_registers[2U] << ")";
      }

      if (sample.kernel_registers().has_value()) {
        const auto& kernel_registers = sample.kernel_registers().value();
        std::cout << " | Kernel Registers = IP(" << kernel_registers[0U] << "), DI(" << kernel_registers[1U]
                  << "), R10(" << kernel_registers[2U] << ")";
      }

      std::cout << "\n";
    }
  }
  std::cout << std::flush;

  /// Close the sampler.
  /// Note that the sampler can only be closed after reading the samples.
  sampler.close();

  return 0;
}