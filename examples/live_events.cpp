#include "access_benchmark.h"
#include <iostream>
#include <perfcpp/event_counter.h>

int
main()
{
  std::cout
    << "libperf-cpp example: Record a events and live events where the latter can be read without stopping the "
       "hardware performance counters. As a benchmark, we use random access to an in-memory array multiple times."
    << std::endl;

  /// Initialize performance counters.
  /// Note that the perf::CounterDefinition holds all counter names and must be
  /// alive until the benchmark finishes.
  auto counter_definitions = perf::CounterDefinition{};
  auto event_counter = perf::EventCounter{ counter_definitions };

  try {
    /// Add counters that are recorded over the entire period (from start to end).
    event_counter.add({ "cycles", "instructions", "cache-references", "cache-misses" });

    /// Add live counters that can be read without stopping the EventCounter.
    event_counter.add_live(std::vector<std::string>{ "cache-references", "cache-misses" });
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
  constexpr auto iterations = 20U;
  for (auto i = 0U; i < iterations; ++i) {

    /// Read the current counter value before the benchmark.
    const auto start_cache_references = event_counter.live_result(/* cache-references counter */ 0U, benchmark.size());
    const auto start_cache_misses = event_counter.live_result(/* cache-misses counter */ 1U, benchmark.size());

    /// Perform benchmark.
    auto value = 0ULL;
    for (auto index = 0U; index < benchmark.size(); ++index) {
      value += benchmark[index].value;
    }
    asm volatile(""
                 : "+r,m"(value)
                 :
                 : "memory"); /// We do not want the compiler to optimize away
                              /// this unused value.

    /// Read the current counter value after the benchmark.
    const auto end_cache_references = event_counter.live_result(/* cache-references counter */ 0U, benchmark.size());
    const auto end_cache_misses = event_counter.live_result(/* cache-misses counter */ 1U, benchmark.size());

    std::cout << "Live results: " << end_cache_references - start_cache_references << " cache-references, "
              << end_cache_misses - start_cache_misses << " cache-misses" << std::endl;
  }

  /// Stop recording counters.
  event_counter.stop();

  std::cout << "\nOverall Results:\n" << event_counter.result(benchmark.size() * iterations).to_string() << std::endl;

  return 0;
}