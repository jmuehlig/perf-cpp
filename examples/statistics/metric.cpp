#include <iostream>
#include <perfcpp/event_counter.h>
#include <perfcpp/metric.h>

#include "../access_benchmark.h"

/**
 * Example of a metric implementation that calculates the number of branch misses per executed branch instruction.
 */
class BranchMissesPerBranchInstruction final : public perf::Metric
{
public:
  [[nodiscard]] std::string name() const override { return "branch-misses-per-branch-instruction"; }

  [[nodiscard]] std::vector<std::string> required_counter_names() const override
  {
    return { "branch-misses", "branch-instructions" };
  }

  [[nodiscard]] std::optional<double> calculate(const perf::CounterResult& result) const override
  {
    const auto branch_misses = result.get("branch-misses");
    const auto branch_instructions = result.get("branch-instructions");

    if (branch_misses.has_value() && branch_instructions.has_value()) {
      if (branch_instructions.value() > 0U) {
        return branch_misses.value() / branch_instructions.value();
      }
    }

    return std::nullopt;
  }

private:
};

int
main()
{
  std::cout << "libperf-cpp example: Implementing new metrics." << std::endl;

  auto counter_definition = perf::CounterDefinition{};

  /// Define a metric that returns the number of cache misses per cache reference:
  counter_definition.add("cache-misses-per-reference", "d_ratio(`cache-misses`, `cache-references`)");

  /// Define a metric that sums up all L1 loads:
  counter_definition.add("l1-loads", "`L1-dcache-loads` + `L1-icache-loads`");

  /// Define a metric that sums up all L1 load misses:
  counter_definition.add("l1-load-misses", "sum(`L1-dcache-load-misses`, `L1-icache-load-misses`)");

  /// Define a metric that calculates the ratio between L1 load misses and L1 loads:
  counter_definition.add("l1-misses-per-load", "`l1-load-misses` / `l1-loads`");


  /// Initialize the above defined metric that returns the number of branch misses per branch instruction.
  counter_definition.add(std::make_unique<BranchMissesPerBranchInstruction>());

  /// Initialize performance counters.
  auto event_counter = perf::EventCounter{ counter_definition };

  /// Add the new defined metrics.
  try {
    event_counter.add(std::vector<std::string>{ "cache-misses-per-reference", "branch-misses-per-branch-instruction", "l1-loads", "l1-load-misses", "l1-misses-per-load" });
  } catch (std::runtime_error& e) {
    std::cerr << e.what() << std::endl;
    return 1;
  }

  /// Create random access benchmark.
  auto benchmark = perf::example::AccessBenchmark{ /*randomize the accesses*/ true,
                                                   /* create benchmark of 512 MB */ 512 };

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

  /// Get the result.
  const auto result = event_counter.result();

  /// Print the metrics as table.
  std::cout << "\nResults as table:\n" << result.to_string() << std::endl;

  return 0;
}