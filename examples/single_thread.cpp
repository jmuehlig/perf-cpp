#include <iostream>
#include <perfcpp/event_counter.h>

#include "access_benchmark.h"

int
main()
{
  std::cout << "libperf-cpp example: Record performance counter for "
               "single-threaded random access to an in-memory array."
            << std::endl;

  /// Initialize performance counters.
  /// Note that the perf::CounterDefinition holds all counter names and must be
  /// alive until the benchmark finishes.
  auto counter_definitions = perf::CounterDefinition{};
  auto event_counter = perf::EventCounter{ counter_definitions };

  /// Add all the performance counters we want to record.
  if (!event_counter.add({ "instructions",
                           "cycles",
                           "branches",
                           "cache-misses",
                           "dTLB-miss-ratio",
                           "L1-data-miss-ratio",
                           "cycles-per-instruction" })) {
    std::cerr << "Could not add performance counters." << std::endl;
  }

  /// Create random access benchmark.
  auto benchmark = perf::example::AccessBenchmark{ /*randomize the accesses*/ true,
                                                   /* create benchmark of 512 MB */ 512U };

  /// Start recording.
  if (!event_counter.start()) {
    std::cerr << "Could not start performance counters." << std::endl;
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

  /// Stop recording counters.
  event_counter.stop();

  /// Get the result (normalized per cache line).
  const auto result = event_counter.result(benchmark.size());

  /// Print the performance counters.
  std::cout << "\nHere are the results:\n" << std::endl;
  for (const auto& [counter_name, counter_value] : result) {
    std::cout << counter_value << " " << counter_name << " per cache line" << std::endl;
  }

  return 0;
}