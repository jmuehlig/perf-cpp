#include "access_benchmark.h"
#include <iostream>
#include <perfcpp/analyzer/data.h>
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
  counter_definitions.add("mem_trans_retired.load_latency_gt_3", perf::CounterConfig{ PERF_TYPE_RAW, 0x1CD, 0x3 });

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
      sampler.trigger(
        { perf::Sampler::Trigger{ "mem-loads-aux", perf::Precision::MustHaveZeroSkid },
          perf::Sampler::Trigger{ "mem_trans_retired.load_latency_gt_3", perf::Precision::MustHaveZeroSkid } });
    } else {
      sampler.trigger("mem_trans_retired.load_latency_gt_3", perf::Precision::MustHaveZeroSkid);
    }
  } else {
    std::cout << "Error: Memory sampling is not supported on this CPU." << std::endl;
    return 1;
  }

  /// Setup which data will be included into samples (timestamp, virtual memory address, data source like L1d or RAM,
  /// and latency).
  sampler.values().logical_memory_address(true).data_src(true);
#ifndef NO_PERF_SAMPLE_WEIGHT_STRUCT
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
  auto data_analyzer = perf::analyzer::DataAnalyzer{};
  /// 1) Cache line that dictates the pattern.
  auto pattern_cache_line = perf::analyzer::DataType{ "pattern_cache_line", 64U };
  for (auto i = 0U; i < 8U; ++i) {
    auto member_name = std::string{ "index[" }.append(std::to_string(i)).append("]");
    pattern_cache_line.add<std::uint64_t>(std::move(member_name));
  }
  data_analyzer.add(std::move(pattern_cache_line));

  /// Register instances.
  data_analyzer.annotate("pattern_cache_line", benchmark.indices().data(), benchmark.indices().size());

  /// 2) Data that is accessed
  auto data_cache_line = perf::analyzer::DataType{ "data_cache_line", 64U };
  data_cache_line.add<std::uint64_t>("value");
  data_analyzer.add(std::move(data_cache_line));

  /// Register instances.
  data_analyzer.annotate("data_cache_line", benchmark.data_to_read().data(), benchmark.data_to_read().size());

  /// Get all the recorded samples.
  auto samples = sampler.result();
  const auto result = data_analyzer.map(samples);

  std::cout << result.to_string() << std::flush;

  /// Close the sampler.
  /// Note that the sampler can only be closed after reading the samples.
  sampler.close();

  return 0;
}