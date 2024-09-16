#include "access_benchmark.h"
#include <iostream>
#include <numeric>
#include <perfcpp/sampler.h>
#include <thread>

int
main()
{
  std::cout << "libperf-cpp example: Record perf samples including time, "
               "instruction pointer, and cpu id for single-threaded random "
               "access to an in-memory array on multiple threads."
            << std::endl;

  constexpr auto count_threads = 4U;

  /// Initialize counter definitions.
  /// Note that the perf::CounterDefinition holds all counter names and must be
  /// alive until the benchmark finishes.
  auto counter_definitions = perf::CounterDefinition{};

  /// Initialize sampler.
  auto perf_config = perf::SampleConfig{};
  perf_config.precise_ip(0U);   /// precise_ip controls the amount of skid, see
                                /// https://man7.org/linux/man-pages/man2/perf_event_open.2.html
  perf_config.period(5000000U); /// Record every 5,000,000th event.

  auto sampler = perf::MultiThreadSampler{
    counter_definitions,
    "cycles", /// Event that generates an overflow which is samples (here we
              /// sample every 1,000,000th cycle)
    perf::Sampler::Type::Time | perf::Sampler::Type::InstructionPointer | perf::Sampler::Type::CPU |
      perf::Sampler::Type::ThreadId, /// Controls what to include into the sample, see
                                     /// https://man7.org/linux/man-pages/man2/perf_event_open.2.html
    count_threads,                   /// Number of threads.
    perf_config
  };

  /// Create random access benchmark.
  auto benchmark = perf::example::AccessBenchmark{ /*randomize the accesses*/ true,
                                                   /* create benchmark of 512 MB */ 1024U };

  /// Allocate space for threads and their results.
  const auto items_per_thread = benchmark.size() / count_threads;
  auto threads = std::vector<std::thread>{};
  auto thread_local_results =
    std::vector<std::uint64_t>(count_threads, 0U); /// Array to store the thread-local results.

  for (auto thread_index = 0U; thread_index < count_threads; ++thread_index) {
    threads.emplace_back([thread_index, items_per_thread, &thread_local_results, &benchmark, &sampler]() {
      auto local_value = 0ULL;

      /// Start sampling per thread.
      try {
        sampler.start(thread_index);
      } catch (std::runtime_error& exception) {
        std::cerr << exception.what() << std::endl;
        return;
      }

      /// Process the data.
      for (auto index = 0U; index < items_per_thread; ++index) {
        local_value += benchmark[(thread_index * items_per_thread) + index].value;
      }

      /// Stop sampling on this thread.
      sampler.stop(thread_index);

      thread_local_results[thread_index] = local_value;
    });
  }

  /// Wait for all threads to finish.
  for (auto& thread : threads) {
    thread.join();
  }

  /// Add up the results so that the compiler does not get the idea of
  /// optimizing away the accesses.
  auto value = std::accumulate(thread_local_results.begin(), thread_local_results.end(), 0UL);
  asm volatile("" : "+r,m"(value) : : "memory");

  /// Get all the recorded samples.
  auto samples = sampler.result();

  /// Sort samples by time since they may be mixed from different threads.
  std::sort(samples.begin(), samples.end(), [](const auto& a, const auto& b) {
    if (a.time().has_value() && b.time().has_value()) {
      return a.time().value() < b.time().value();
    }
    return false;
  });

  /// Print the first samples.
  const auto count_show_samples = std::min<std::size_t>(samples.size(), 40U);
  std::cout << "\nRecorded " << samples.size() << " samples." << std::endl;
  std::cout << "Here are the first " << count_show_samples << " recorded samples:\n" << std::endl;
  for (auto index = 0U; index < count_show_samples; ++index) {
    const auto& sample = samples[index];

    /// Since we recorded the time, period, the instruction pointer, and the CPU
    /// id, we can only read these values.
    if (sample.time().has_value() && sample.thread_id().has_value() && sample.instruction_pointer().has_value() &&
        sample.cpu_id().has_value()) {
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