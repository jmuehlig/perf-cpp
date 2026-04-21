#include <iostream>
#include <perfcpp/event_counter.hpp>
#include <perfcpp/metric/metric.hpp>

#include "../access_benchmark.h"

int
main()
{
  std::cout << "libperf-cpp example: Record RAPL power consumption counters (watts-pkg, watts-cores, watts-ram)."
            << std::endl;

  auto counter_definition = perf::CounterDefinition{};

  /// Register RAPL metrics that are supported on this hardware.
  auto metrics = std::vector<std::string>{};

  if (counter_definition.supports("energy-pkg")) {
    counter_definition.add(std::make_unique<perf::WattsPkg>());
    metrics.emplace_back("watts-pkg");
  }

  if (counter_definition.supports("energy-cores")) {
    counter_definition.add(std::make_unique<perf::WattsCores>());
    metrics.emplace_back("watts-cores");
  }

  if (counter_definition.supports("energy-ram")) {
    counter_definition.add(std::make_unique<perf::WattsRam>());
    metrics.emplace_back("watts-ram");
  }

  if (metrics.empty()) {
    std::cerr << "No RAPL counters (energy-pkg, energy-cores, energy-ram) are available on this system." << std::endl;
    return 1;
  }

  /// RAPL counters are system-wide: they require any-process and a pinned CPU core.
  auto config = perf::Config{};
  config.process(perf::Process::Any);
  config.cpu_core(perf::CpuCore{ 0U });

  /// Initialize performance counters.
  auto event_counter = perf::EventCounter{ counter_definition, config };

  try {
    event_counter.add(std::move(metrics));
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

  /// We do not want the compiler to optimize away this (otherwise) unused value (and consequently the loop above).
  benchmark.pretend_to_use(value);

  /// Stop recording counters.
  event_counter.stop();

  /// Get the result.
  const auto result = event_counter.result();

  /// Print the RAPL power metrics as table.
  std::cout << "\n" << result.to_string() << std::endl;

  return 0;
}
