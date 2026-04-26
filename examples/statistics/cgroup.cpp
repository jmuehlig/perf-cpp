#include "../access_benchmark.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <perfcpp/event_counter.hpp>
#include <string>

/// Reads the cgroupv2 path of the calling process from /proc/self/cgroup.
[[nodiscard]] std::optional<std::string>
read_own_cgroup_path()
{
  auto file = std::ifstream{ "/proc/self/cgroup" };
  auto line = std::string{};
  while (std::getline(file, line)) {
    /// cgroupv2 lines start with "0::"; the relative path follows.
    if (line.rfind("0::", 0) == 0) {
      return "/sys/fs/cgroup" + line.substr(3);
    }
  }
  return std::nullopt;
}

int
main()
{
  std::cout << "libperf-cpp example: Record performance counters via cgroup monitoring on CPU 0.\n"
               "Run with 'taskset -c 0' to execute the workload on the monitored CPU.\n"
               "Any other core will produce zero counts.\n"
               "Note: Requires CAP_PERFMON or perf_event_paranoid <= 0.\n"
            << std::endl;

  /// Locate the calling process's own cgroup path via /proc/self/cgroup (cgroupv2).
  const auto cgroup_path = read_own_cgroup_path();
  if (!cgroup_path.has_value()) {
    std::cerr << "Cannot read cgroup path from /proc/self/cgroup. Is this a cgroupv2 system?" << std::endl;
    return 1;
  }
  std::cout << "Monitoring cgroup: " << cgroup_path.value() << " on CPU 0." << std::endl;

  /// Open the cgroup directory and configure the event counter for CPU 0.
  auto config = perf::Config{};
  try {
    config.cgroup(perf::CGroupMonitor{ std::filesystem::path{ cgroup_path.value() } });
  } catch (std::runtime_error& e) {
    std::cerr << e.what() << std::endl;
    return 1;
  }
  config.cpu_core(0U);

  /// Initialize the event counter with the cgroup-scoped configuration.
  auto event_counter = perf::EventCounter{ config };

  /// Add performance counters to record.
  try {
    event_counter.add({ "instructions", "cycles", "branches", "branch-misses", "cache-misses", "cycles-per-instruction" });
  } catch (std::runtime_error& e) {
    std::cerr << e.what() << std::endl;
    return 1;
  }

  /// Create random access benchmark.
  auto benchmark = perf::example::AccessBenchmark{ /*randomize the accesses*/ true, /* 512 MB */ 512U };

  /// Start recording.
  try {
    event_counter.start();
  } catch (std::runtime_error& e) {
    std::cerr << e.what() << std::endl;
    return 1;
  }

  /// Execute the benchmark (random access to cache lines).
  auto value = 0ULL;
  for (auto index = 0U; index < benchmark.size(); ++index) {
    value += benchmark[index].value;
  }
  benchmark.pretend_to_use(value);

  /// Stop recording.
  event_counter.stop();

  /// Print the results (normalized per cache line).
  const auto result = event_counter.result(benchmark.size());

  std::cout << "\nResults:\n";
  for (const auto& [counter_name, counter_value] : result) {
    std::cout << counter_value << " " << counter_name << " / cache line" << std::endl;
  }

  std::cout << "\n" << result.to_string() << std::endl;

  return 0;
}
