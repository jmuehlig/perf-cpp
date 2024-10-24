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
  try {
    event_counter.add({ "instructions",
                        "cycles",
                        "branches",
                        "cache-misses",
                        "dTLB-miss-ratio",
                        "L1-data-miss-ratio",
                        "cycles-per-instruction" });
  } catch (std::runtime_error& e) {
    std::cerr << e.what() << std::endl;
    return 1;
  }

  /// Create random access benchmark.
  auto benchmark = perf::example::AccessBenchmark{ /*randomize the accesses*/ true,
                                                   /* create benchmark of 512 MB */ 512U };

  /// Start recording.
  try {
    event_counter.start();
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

  /// Stop recording counters.
  event_counter.stop();

  /// Get the result (normalized per cache line).
  const auto result = event_counter.result(benchmark.size());

  /// Print the performance counters manually.
  std::cout << "\nResults:\n";
  for (const auto& [counter_name, counter_value] : result) {
    std::cout << counter_value << " " << counter_name << " / cache line" << std::endl;
  }

  /// Print the performance counters as table.
  std::cout << "\nResults as table:\n" << result.to_string() << std::endl;

  return 0;
}