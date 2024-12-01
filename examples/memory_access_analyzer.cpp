#include "access_benchmark.h"
#include <iostream>
#include <perfcpp/analyzer/memory_access.h>
#include <perfcpp/hardware_info.h>
#include <perfcpp/sampler.h>

int
main()
{
  std::cout << "libperf-cpp example: Sample memory addresses and analyze data objects." << std::endl;

  /// Initialize counter definitions.
  /// Note that the perf::CounterDefinition holds all counter names and must be
  /// alive until the benchmark finishes.
  auto counter_definitions = perf::CounterDefinition{};

  /// Initialize sampler.
  auto perf_config = perf::SampleConfig{};
  perf_config.period(16000U); /// Record every 16,000th event.

  auto sampler = perf::Sampler{ counter_definitions, perf_config };

  /// Setup which counters trigger the writing of samples (depends on the underlying hardware substrate).
  if (perf::HardwareInfo::is_amd_ibs_supported()) {
    sampler.trigger("ibs_op_uops", perf::Precision::MustHaveZeroSkid);
  } else if (perf::HardwareInfo::is_intel()) {
    if (perf::HardwareInfo::is_intel_aux_counter_required()) {
      /// Note: For sampling on Sapphire Rapids, we have to prepend an auxiliary counter.
      sampler.trigger({ perf::Sampler::Trigger{ "mem-loads-aux", perf::Precision::MustHaveZeroSkid },
                        perf::Sampler::Trigger{ "mem-loads", perf::Precision::MustHaveZeroSkid } });
    } else {
      sampler.trigger("mem-loads", perf::Precision::MustHaveZeroSkid);
    }
  } else {
    std::cout << "Error: Memory sampling is not supported on this CPU." << std::endl;
    return 1;
  }

  /// Setup which data will be included into samples (timestamp, virtual memory address, data source like L1d or RAM,
  /// and latency).
  sampler.values().logical_memory_address(true).data_src(true);
#ifndef PERFCPP_NO_SAMPLE_WEIGHT_STRUCT
  sampler.values().weight_struct(true);
#else
  sampler.values().weight(true);
#endif

  /// Create random access benchmark.
  auto benchmark = perf::example::AccessBenchmark{ /*randomize the accesses*/ true,
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
    value += benchmark[index].value;
  }
  asm volatile(""
               : "+r,m"(value)
               :
               : "memory"); /// We do not want the compiler to optimize away
                            /// this unused value.

  /// Stop sampling.
  sampler.stop();

  /// Create data types for analyzer.
  auto data_analyzer = perf::analyzer::MemoryAccess{};

  /// 1) Create and add the "index" data type (normal u64 that dictates the pattern through the data array in the random
  /// access benchmark).
  auto index = perf::analyzer::DataType{ "index", sizeof(std::uint64_t) };
  index.add<std::uint64_t>("index");
  data_analyzer.add(std::move(index));

  /// 2) Create and add the "data_cache_line" data type (single cache line that is accessed in the random access
  /// benchmark).
  auto cache_line = perf::analyzer::DataType{ "data_cache_line", sizeof(perf::example::AccessBenchmark::cache_line) };
  cache_line.add<std::uint64_t>("value");
  data_analyzer.add(std::move(cache_line));

  /// 3) Register instances in memory for both data types.
  data_analyzer.annotate("index", benchmark.indices());
  data_analyzer.annotate("data_cache_line", benchmark.data_to_read());

  /// 4) Get all the recorded samples.
  auto samples = sampler.result();

  /// 5) Map the samples to data type instances.
  const auto result = data_analyzer.map(samples);

  /// 6) Print the results to the console.
  std::cout << result.to_string() << std::flush;

  return 0;
}