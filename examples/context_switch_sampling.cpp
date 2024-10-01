#include "access_benchmark.h"
#include <iostream>
#include <perfcpp/sampler.h>

int
main()
{
  std::cout << "libperf-cpp example: Record perf samples including time, "
               "instruction pointer, and cpu id for single-threaded random "
               "access to an in-memory array."
            << std::endl;

  /// Initialize counter definitions.
  /// Note that the perf::CounterDefinition holds all counter names and must be
  /// alive until the benchmark finishes.
  auto counter_definitions = perf::CounterDefinition{};

  /// Initialize sampler.
  auto perf_config = perf::SampleConfig{};
  perf_config.period(10000U); /// Record every 10,000th event.
  perf_config.is_debug(true);

  auto sampler = perf::Sampler{ counter_definitions, perf_config };

  /// Event that generates an overflow which is samples.
  sampler.trigger("cycles", perf::Precision::RequestZeroSkid);

  /// Include Timestamp, period, instruction pointer, and CPU number into samples.
  sampler.values().time(true).period(true).instruction_pointer(true).cpu_id(true).context_switch(true);

  /// Create random access benchmark.
  auto benchmark = perf::example::AccessBenchmark{ /*randomize the accesses*/ true,
                                                   /* create benchmark of 512 MB */ 2048U };

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
  std::cout << "\nRecorded " << samples.size() << " samples." << std::endl;
  for (auto index = 0U; index < samples.size(); ++index) {
    const auto& sample = samples[index];

    /// Since we recorded the time, period, the instruction pointer, and the CPU
    /// id, we can only read these values.
    if (sample.time().has_value() && sample.context_switch().has_value()) {
      std::cout << "Time = " << sample.time().value()
                << " | is in = " << sample.context_switch().value().is_in()
                << " | is preempt = " << sample.context_switch().value().is_preempt()
                << "\n";
    }
  }
  std::cout << std::flush;

  /// Close the sampler.
  /// Note that the sampler can only be closed after reading the samples.
  sampler.close();

  return 0;
}