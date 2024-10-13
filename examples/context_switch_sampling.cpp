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

  auto sampler = perf::Sampler{ counter_definitions, perf_config };

  /// Event that generates an overflow which is samples.
  sampler.trigger("cycles", perf::Precision::RequestZeroSkid);

  /// Include Timestamp, period, instruction pointer, and CPU number into samples.
  sampler.values().time(true).cpu_id(true).context_switch(true);

  /// Create random access benchmark.
  auto benchmark = perf::example::AccessBenchmark{ /*randomize the accesses*/ true,
                                                   /* create benchmark of 2 GB */ 2048U };

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
  auto samples = sampler.result();
  const auto count_samples_before_filter = samples.size();

  /// Filter out samples without context switch.
  samples.erase(std::remove_if(samples.begin(),
                               samples.end(),
                               [](const auto& sample) {
                                 return !sample.cpu_id().has_value() || !sample.time().has_value() || !sample.context_switch().has_value();
                               }),
                samples.end());

  /// Print the first samples.
  const auto count_show_samples = std::min<std::size_t>(samples.size(), 40U);
  std::cout << "\nRecorded " << count_samples_before_filter << " samples. " << samples.size()
            << " remaining after filter." << std::endl;
  std::cout << "Here are the first " << count_show_samples << " recorded samples:\n" << std::endl;
  for (auto index = 0U; index < count_show_samples; ++index) {
    const auto& sample = samples[index];

    std::cout << "Time = " << sample.time().value() << " | CPU ID = " << sample.cpu_id().value() << " | is in = " << sample.context_switch().value().is_in()
              << " | is preempt = " << sample.context_switch().value().is_preempt() << "\n";
  }
  std::cout << std::flush;

  /// Close the sampler.
  /// Note that the sampler can only be closed after reading the samples.
  sampler.close();

  return 0;
}