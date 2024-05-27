#include "access_benchmark.h"
#include "perfcpp/event_counter.h"
#include <iostream>
#include <numeric>
#include <thread>

int
main()
{
  std::cout << "libperf-cpp example: Record performance counter for "
               "multi-threaded random access to an in-memory array."
            << std::endl;
  std::cout << "We will record the counters per thread and merge the results "
               "afterwards."
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
                                                   /* create benchmark of 1024 MB */ 1024U };

  /// One event_counter instance for every thread.
  const auto count_threads = 2U;
  const auto items_per_thread = benchmark.size() / count_threads;
  auto threads = std::vector<std::thread>{};
  auto thread_local_results = std::vector<std::uint64_t>(2U, 0U); /// Array to store the thread-local results.

  /// Create a perf counter for every thread.
  /// Note that the `event_counter` object cannot be used afterwards.
  auto multithread_event_counter = perf::EventCounterMT{ std::move(event_counter), count_threads };

  for (auto thread_index = 0U; thread_index < count_threads; ++thread_index) {
    threads.emplace_back(
      [thread_index, items_per_thread, &thread_local_results, &benchmark, &multithread_event_counter]() {
        auto local_value = 0ULL;

        /// Start recording counters.
        /// In contrast to the inherit-thread example (see inherit_thread.cpp), we
        /// will record the performance counters on each thread.
        multithread_event_counter.start(thread_index);

        /// Process the data.
        for (auto index = 0U; index < items_per_thread; ++index) {
          local_value += benchmark[(thread_index * items_per_thread) + index].value;
        }

        /// Stop recording counters on this thread.
        multithread_event_counter.stop(thread_index);

        thread_local_results[thread_index] = local_value;
      });
  }

  /// Wait for all threads to finish.
  for (auto& thread : threads) {
    thread.join();
  }

  /// Add up the results so that the compiler does not get the idea of
  /// optimizing away the accesses.
  auto value = std::accumulate(thread_local_results.begin(), thread_local_results.end(), 0U);
  asm volatile("" : "+r,m"(value) : : "memory");

  /// Get the result (normalized per cache line) from the
  /// multithread_event_counter.
  auto result = multithread_event_counter.result(benchmark.size());

  /// Print the performance counters.
  std::cout << "\nHere are the results for " << count_threads << " threads:\n" << std::endl;
  for (const auto& [counter_name, counter_value] : result) {
    std::cout << counter_value << " " << counter_name << " per cache line" << std::endl;
  }

  return 0;
}
