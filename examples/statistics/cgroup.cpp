#include "../access_benchmark.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <perfcpp/event_counter.hpp>
#include <pthread.h>
#include <string>
#include <thread>

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
  std::cout << "libperf-cpp example: Record performance counters via cgroup monitoring on CPU 1.\n"
               "The benchmark runs in a thread pinned to CPU 1 which starts and stops the counter.\n"
               "Note: Requires CAP_PERFMON or perf_event_paranoid <= 0.\n"
            << std::endl;

  /// Locate the calling process's own cgroup path via /proc/self/cgroup (cgroupv2).
  const auto cgroup_path = read_own_cgroup_path();
  if (!cgroup_path.has_value()) {
    std::cerr << "Cannot read cgroup path from /proc/self/cgroup. Is this a cgroupv2 system?" << std::endl;
    return 1;
  }
  std::cout << "Monitoring cgroup: " << cgroup_path.value() << " on CPU 1." << std::endl;

  /// Open the cgroup directory and configure the event counter for CPU 1.
  auto config = perf::Config{};
  try {
    config.cgroup(perf::CGroupMonitor{ std::filesystem::path{ cgroup_path.value() } });
  } catch (std::runtime_error& e) {
    std::cerr << e.what() << std::endl;
    return 1;
  }
  config.cpu_core(1U);

  /// Initialize the event counter with the cgroup-scoped configuration.
  auto event_counter = perf::EventCounter{ config };

  /// Add performance counters to record.
  try {
    event_counter.add(
      { "instructions", "cycles", "branches", "branch-misses", "cache-misses", "cycles-per-instruction" });
  } catch (std::runtime_error& e) {
    std::cerr << e.what() << std::endl;
    return 1;
  }

  /// Create random access benchmark.
  auto benchmark = perf::example::AccessBenchmark{ /*randomize the accesses*/ true, /* 512 MB */ 512U };

  /// Run the benchmark in a thread pinned to CPU 1 so it falls within the monitored cgroup+CPU scope.
  auto benchmark_thread = std::thread{ [&event_counter, &benchmark]() {
    /// Pin this thread to CPU core 1.
    auto cpu_set = cpu_set_t{};
    CPU_ZERO(&cpu_set);
    CPU_SET(1, &cpu_set);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpu_set);

    /// Start recording.
    try {
      event_counter.start();
    } catch (std::runtime_error& e) {
      std::cerr << e.what() << std::endl;
      return;
    }

    /// Execute the benchmark (random access to cache lines).
    auto value = 0ULL;
    for (auto index = 0U; index < benchmark.size(); ++index) {
      value += benchmark[index].value;
    }
    benchmark.pretend_to_use(value);

    /// Stop recording.
    event_counter.stop();
  } };
  benchmark_thread.join();

  /// Print the results (normalized per cache line).
  const auto result = event_counter.result(benchmark.size());

  std::cout << "\nResults:\n";
  for (const auto& [counter_name, counter_value] : result) {
    std::cout << counter_value << " " << counter_name << " / cache line" << std::endl;
  }

  std::cout << "\n" << result.to_string() << std::endl;

  return 0;
}
