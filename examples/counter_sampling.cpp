#include "access_benchmark.h"
#include <iostream>
#include <perfcpp/sampler.h>

int
main()
{
  std::cout << "libperf-cpp example: Record perf samples including performance "
               "counters for single-threaded random access to an in-memory array."
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

  auto sampler = perf::Sampler{
    counter_definitions,
    std::vector<std::string>{
      "cycles", "L1-dcache-loads", "L1-dcache-load-misses" }, /// List of events. The first event generates
                                                              /// an overflow which is sampled (here we
                                                              /// sample every 1,000,000th cycle), the rest
                                                              /// is recorded.
    perf::Sampler::Type::Time |
      perf::Sampler::Type::CounterValues, /// Controls what to include into the sample, see
                                          /// https://man7.org/linux/man-pages/man2/perf_event_open.2.html
    perf_config
  };

  /// Create random access benchmark.
  auto benchmark = perf::example::AccessBenchmark{ /*randomize the accesses*/ true,
                                                   /* create benchmark of 512 MB */ 512U };

  /// Start sampling.
  if (!sampler.start()) {
    std::cerr << "Could not start sampling, errno = " << sampler.last_error() << "." << std::endl;
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

  std::optional<perf::CounterResult> last_counter_result = std::nullopt; /// Remember the last counter result to show
                                                                         /// only the difference.

  for (auto index = 0U; index < count_show_samples; ++index) {
    const auto& sample = samples[index];

    /// Since we recorded the time, period, the instruction pointer, and the CPU
    /// id, we can only read these values.
    if (sample.time().has_value() && sample.counter_result().has_value()) {
      if (last_counter_result.has_value()) {
        std::cout << "Time = " << sample.time().value() << " | cycles (diff) = "
                  << sample.counter_result()->get("cycles").value_or(.0) -
                       last_counter_result->get("cycles").value_or(.0)
                  << " | L1-dcache-loads (diff) = "
                  << sample.counter_result()->get("L1-dcache-loads").value_or(.0) -
                       last_counter_result->get("L1-dcache-loads").value_or(.0)
                  << " | L1-dcache-load-misses (diff) = "
                  << sample.counter_result()->get("L1-dcache-load-misses").value_or(.0) -
                       last_counter_result->get("L1-dcache-load-misses").value_or(.0)
                  << "\n";
      }

      last_counter_result = sample.counter_result();
    }
  }
  std::cout << std::flush;

  /// Close the sampler.
  /// Note that the sampler can only be closed after reading the samples.
  sampler.close();

  return 0;
}