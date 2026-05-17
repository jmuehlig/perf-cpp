#include "../access_benchmark.h"
#include <iostream>
#include <perfcpp/sampler.hpp>

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

  /// Initialize sampler.
  auto sampler = perf::Sampler{};

  /// Setup which counters trigger the writing of samples.
  sampler.trigger(perf::Cycles{}, perf::Precision::AllowArbitrarySkid, perf::Period{ 1000000U });

  /// Setup which data will be included into samples (timestamp and stack of branches).
  /// The second argument enables per-entry branch classification (Linux 4.15+).
  sampler.values().timestamp(true).branch_stack({ perf::BranchType::User, perf::BranchType::Conditional },
                                                /*is_record_branch_classification=*/true);

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

  /// We do not want the compiler to optimize away this (otherwise) unused value (and consequently the loop above).
  benchmark.pretend_to_use(value);

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

    if (sample.metadata().timestamp().has_value() && sample.branch_stack().has_value()) {
      std::cout << "Time = " << sample.metadata().timestamp().value() << "\n";
      for (const auto& branch : sample.branch_stack().value()) {
        std::cout << "\tpredicted correct = " << branch.is_predicted() << " | from 0x" << std::hex
                  << branch.instruction_pointer_from() << std::dec << " | to 0x" << std::hex
                  << branch.instruction_pointer_to() << std::dec;

        if (branch.cycles().has_value()) {
          std::cout << " | cycles = " << branch.cycles().value();
        }

        if (branch.classification().has_value()) {
          switch (branch.classification().value()) {
            case perf::Branch::Classification::Conditional:
              std::cout << " | conditional";
              break;
            case perf::Branch::Classification::Unconditional:
              std::cout << " | unconditional";
              break;
            case perf::Branch::Classification::Call:
              std::cout << " | call";
              break;
            case perf::Branch::Classification::IndirectCall:
              std::cout << " | indirect call";
              break;
            case perf::Branch::Classification::Return:
              std::cout << " | return";
              break;
            case perf::Branch::Classification::Syscall:
              std::cout << " | syscall";
              break;
            case perf::Branch::Classification::SyscallReturn:
              std::cout << " | syscall return";
              break;
            case perf::Branch::Classification::ConditionalCall:
              std::cout << " | conditional call";
              break;
            case perf::Branch::Classification::ConditionalReturn:
              std::cout << " | conditional return";
              break;
            case perf::Branch::Classification::Indirect:
              std::cout << " | indirect";
              break;
            case perf::Branch::Classification::ExceptionReturn:
              std::cout << " | exception return";
              break;
            case perf::Branch::Classification::Interrupt:
              std::cout << " | interrupt";
              break;
            case perf::Branch::Classification::SystemError:
              std::cout << " | system error";
              break;
            case perf::Branch::Classification::NotInTransaction:
              std::cout << " | not in transaction";
              break;
            default:
              break;
          }
        }

        if (branch.speculation_result().has_value()) {
          switch (branch.speculation_result().value()) {
            case perf::Branch::Speculation::Wrong:
              std::cout << " | wrong path";
              break;
            case perf::Branch::Speculation::Correct:
              std::cout << " | correct path";
              break;
            case perf::Branch::Speculation::SpeculativeCorrect:
              std::cout << " | speculative correct path";
              break;
          }
        }

        std::cout << "\n";
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

  for (auto i = 0U; i < 10U; ++i) {
    switch ((cache_line.value >> (4U * i)) & 0xF) { // Extract 4 bits at a time
      case 0ULL:
        result += cache_line.value * (i + 1U);
        break;
      case 1ULL:
        result -= cache_line.value / (i + 2U);
        break;
      case 2ULL:
        result *= cache_line.value + (i * 3U);
        break;
      case 3ULL:
        result /= (cache_line.value - i) | 1U;
        break; // Avoid division by zero
      case 4ULL:
        result ^= cache_line.value << i;
        break;
      case 5ULL:
        result %= (cache_line.value >> i) | 1U;
        break;
      case 6ULL:
        result = ~result;
        break;
      case 7ULL:
        result &= cache_line.value | (std::uint64_t(0xFF) << (i * 8U));
        break;
      case 8ULL:
        result |= cache_line.value & (std::uint64_t(0xFFFF) << (i * 16U));
        break;
      case 9ULL:
        result >>= cache_line.value % (i + 1);
        break;
      case 10ULL:
        result <<= cache_line.value % (i + 2);
        break;
      case 11ULL:
        result += cache_line.value + i * 7;
        break;
      case 12ULL:
        result -= cache_line.value - i * 11;
        break;
      case 13ULL:
        result *= cache_line.value * (i + 5);
        break;
      case 14ULL:
        result /= (cache_line.value / (i + 3)) | 1;
        break;
      case 15ULL:
        result ^= cache_line.value ^ (i * 13);
        break;
      default:
        result = cache_line.value;
    }
  }
  return result;
}