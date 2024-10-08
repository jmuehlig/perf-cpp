#include "access_benchmark.h"
#include <iostream>
#include <perfcpp/sampler.h>

/**
 * A function using multiple branches hard to optimize for the compiler for
 * demonstrating branch-sampling.
 *
 * @param cache_line Cache line to use as an input.
 * @return Another value through a handful of branches.
 */
[[nodiscard]] std::uint64_t
branchy_function(const perf::example::AccessBenchmark::cache_line& cache_line);

int
main()
{

  std::cout << "libperf-cpp example: Record perf branch samples for "
               "single-threaded sequential access to an in-memory array."
            << std::endl;

  /// Initialize counter definitions.
  /// Note that the perf::CounterDefinition holds all counter names and must be
  /// alive until the benchmark finishes.
  auto counter_definitions = perf::CounterDefinition{};

  /// Initialize sampler.
  auto perf_config = perf::SampleConfig{};
  perf_config.period(1000000U); /// Record every 1,000,000th event.

  auto sampler = perf::Sampler{ counter_definitions, perf_config };

  /// Setup which counters trigger the writing of samples.
  sampler.trigger("cycles", perf::Precision::AllowArbitrarySkid);

  /// Setup which data will be included into samples (timestamp and stack of branches).
  sampler.values().time(true).branch_stack(
    { perf::BranchType::User, perf::BranchType::Conditional }) /// Only sample conditional branches in user-mode.
    ;

  /// Create random access benchmark.
  auto benchmark = perf::example::AccessBenchmark{ /*sequential accesses*/ false,
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
    value += branchy_function(benchmark[index]);
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
  const auto count_show_samples = std::min<std::size_t>(samples.size(), 10U);
  std::cout << "\nRecorded " << samples.size() << " samples." << std::endl;
  std::cout << "Here are the first " << count_show_samples << " recorded samples:\n" << std::endl;

  for (auto index = 0U; index < count_show_samples; ++index) {
    const auto& sample = samples[index];

    /// Since we recorded the time, period, the instruction pointer, and the CPU
    /// id, we can only read these values.
    if (sample.time().has_value() && sample.branches().has_value()) {
      std::cout << "Time = " << sample.time().value() << "\n";
      for (const auto& branch : sample.branches().value()) {
        std::cout << "\tpredicted correct = " << branch.is_predicted() << " | from instruction "
                  << branch.instruction_pointer_from() << " | to instruction " << branch.instruction_pointer_to()
                  << "\n";
      }
    }
  }
  std::cout << std::flush;

  /// Close the sampler.
  /// Note that the sampler can only be closed after reading the samples.
  sampler.close();

  return 0;
}

std::uint64_t
branchy_function(const perf::example::AccessBenchmark::cache_line& cache_line)
{
  auto result = cache_line.value;

  for (int i = 0; i < 10; ++i) {
    switch ((cache_line.value >> (4 * i)) & 0xF) { // Extract 4 bits at a time
      case 0:
        result += cache_line.value * (i + 1);
        break;
      case 1:
        result -= cache_line.value / (i + 2);
        break;
      case 2:
        result *= cache_line.value + (i * 3);
        break;
      case 3:
        result /= (cache_line.value - i) | 1;
        break; // Avoid division by zero
      case 4:
        result ^= cache_line.value << i;
        break;
      case 5:
        result %= (cache_line.value >> i) | 1;
        break;
      case 6:
        result = ~result;
        break;
      case 7:
        result &= cache_line.value | (0xFF << (i * 8));
        break;
      case 8:
        result |= cache_line.value & (0xFFFF << (i * 16));
        break;
      case 9:
        result >>= cache_line.value % (i + 1);
        break;
      case 10:
        result <<= cache_line.value % (i + 2);
        break;
      case 11:
        result += cache_line.value + i * 7;
        break;
      case 12:
        result -= cache_line.value - i * 11;
        break;
      case 13:
        result *= cache_line.value * (i + 5);
        break;
      case 14:
        result /= (cache_line.value / (i + 3)) | 1;
        break;
      case 15:
        result ^= cache_line.value ^ (i * 13);
        break;
      default:
        result = cache_line.value;
    }
  }
  return result;
}