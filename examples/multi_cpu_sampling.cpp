#include "access_benchmark.h"
#include <atomic>
#include <iostream>
#include <numeric>
#include <perfcpp/sampler.h>
#include <thread>

int
main()
{
  std::cout << "libperf-cpp example: Record perf samples including time, "
               "instruction pointer, and cpu id for single-threaded random "
               "access to an in-memory array on multiple CPU cores."
            << std::endl;

  constexpr auto count_threads = 4U;

  /// Initialize counter definitions.
  /// Note that the perf::CounterDefinition holds all counter names and must be
  /// alive until the benchmark finishes.
  auto counter_definitions = perf::CounterDefinition{};

  /// Initialize sampler.
  auto perf_config = perf::SampleConfig{};
  perf_config.period(5000000U); /// Record every 5,000,000th event.

  /// Create a list of cpus to sample (all available, in this example).
  auto cpus_to_watch = std::vector<std::uint16_t>(std::min(4U, std::thread::hardware_concurrency()));
  std::iota(cpus_to_watch.begin(), cpus_to_watch.end(), 0U);

  auto sampler = perf::MultiCoreSampler{ counter_definitions, std::move(cpus_to_watch), perf_config };

  /// Setup event that triggers writing samples.
  sampler.trigger("cycles");

  /// Setup what data the samples should include (timestamp, instruction pointer, CPU id, thread id).
  sampler.values().time(true).instruction_pointer(true).cpu_id(true).thread_id(true);

  /// Create random access benchmark.
  auto benchmark = perf::example::AccessBenchmark{ /*randomize the accesses*/ true,
                                                   /* create benchmark of 512 MB */ 1024U };

  /// Allocate space for threads and their results.
  const auto items_per_thread = benchmark.size() / count_threads;
  auto threads = std::vector<std::thread>{};
  auto thread_local_results =
    std::vector<std::uint64_t>(count_threads, 0U); /// Array to store the thread-local results.

  /// Barrier for the threads to wait in order to start them all at the same time.
  auto thread_barrier = std::atomic<bool>{ false };

  for (auto thread_index = 0U; thread_index < count_threads; ++thread_index) {
    threads.emplace_back([thread_index, items_per_thread, &thread_local_results, &benchmark, &thread_barrier]() {
      auto local_value = 0ULL;

      /// Wait for the barrier to become "true", i.e., all threads are spawned.
      while (!thread_barrier)
        ;

      /// Process the data.
      for (auto index = 0U; index < items_per_thread; ++index) {
        local_value += benchmark[(thread_index * items_per_thread) + index].value;
      }

      thread_local_results[thread_index] = local_value;
    });
  }

  /// Start sampling for all specified CPUs at once.
  try {
    sampler.start();
  } catch (std::runtime_error& exception) {
    std::cerr << exception.what() << std::endl;
    return 1;
  }

  /// Let threads start.
  thread_barrier = true;

  /// Wait for all threads to finish.
  for (auto& thread : threads) {
    thread.join();
  }

  /// Stop sampling on all CPUs.
  sampler.stop();

  /// Add up the results so that the compiler does not get the idea of
  /// optimizing away the accesses.
  auto value = std::accumulate(thread_local_results.begin(), thread_local_results.end(), 0UL);
  asm volatile("" : "+r,m"(value) : : "memory");

  /// Get all the recorded samples – ordered by timestamp.
  auto samples = sampler.result(true);

  /// Print the first samples.
  const auto count_show_samples = std::min<std::size_t>(samples.size(), 40U);
  std::cout << "\nRecorded " << samples.size() << " samples." << std::endl;
  std::cout << "Here are the first " << count_show_samples << " recorded samples:\n" << std::endl;
  for (auto index = 0U; index < count_show_samples; ++index) {
    const auto& sample = samples[index];

    /// Since we recorded the time, period, the instruction pointer, and the CPU
    /// id, we can only read these values.
    if (sample.time().has_value() && sample.cpu_id().has_value() && sample.thread_id().has_value() &&
        sample.instruction_pointer().has_value() && sample.cpu_id().has_value()) {
      std::cout << "Time = " << sample.time().value() << " | CPU ID = " << sample.cpu_id().value()
                << " | Thread ID = " << sample.thread_id().value() << " | Instruction Pointer = 0x" << std::hex
                << sample.instruction_pointer().value() << std::dec << "\n";
    }
  }
  std::cout << std::flush;

  /// Close the sampler.
  /// Note that the sampler can only be closed after reading the samples.
  sampler.close();

  return 0;
}