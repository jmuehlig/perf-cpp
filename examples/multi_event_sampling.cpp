#include "access_benchmark.h"
#include <iostream>
#include <perfcpp/sampler.h>

int
main()
{
  std::cout << "libperf-cpp example: Record perf samples including time, "
               "logical memory address, latency, and data source for "
               "single-threaded random access to an in-memory array "
               "using multiple events as trigger."
            << std::endl;

  /// Initialize counter definitions.
  /// Note that the perf::CounterDefinition holds all counter names and must be
  /// alive until the benchmark finishes.
  auto counter_definitions = perf::CounterDefinition{};
  counter_definitions.add("loads", perf::CounterConfig{ PERF_TYPE_RAW, 0x1CD, 0x3 });
  counter_definitions.add("stores", perf::CounterConfig{ PERF_TYPE_RAW, 0x2CD });
  counter_definitions.add("ibs_op", perf::CounterConfig{ 11U, 0x0 });

  /// Initialize sampler.
  auto perf_config = perf::SampleConfig{};
  perf_config.period(10000U);           /// Record every 10,000th event.

  auto sampling_counters = std::vector<std::vector<std::pair<std::string, perf::Precision>>>{};

  if (__builtin_cpu_is("intel") > 0) {
    if (__builtin_cpu_is("sapphirerapids")) {
      /// Note: For sampling on Sapphire Rapids, we have to prepend an auxiliary counter.
      sampling_counters.push_back({ {"mem-loads-aux", perf::Precision::MustHaveZeroSkid}, {"loads", perf::Precision::RequestZeroSkid} });
    } else {
      sampling_counters.push_back({ {"loads", perf::Precision::RequestZeroSkid} });
    }

    sampling_counters.push_back({ {"stores", perf::Precision::MustHaveZeroSkid} });
  }

  if (sampling_counters.empty()) {
    std::cout << "Error: Memory sampling with multiple triggers is not supported on this CPU." << std::endl;
    return 1;
  }

  auto sampler = perf::Sampler{ counter_definitions, perf_config };
  sampler.trigger(std::move(sampling_counters));
  sampler.values().time(true).logical_memory_address(true).data_src(true);

#ifndef NO_PERF_SAMPLE_WEIGHT_STRUCT
  sampler.values().weight_struct(true);
#else
  sampler.values().weight(true);
#endif

  /// Create random access benchmark.
  auto benchmark = perf::example::AccessBenchmark{ /*randomize the accesses*/ true,
                                                   /* create benchmark of 512 MB */ 512U,
                                                   /* also support writing */ true };

  /// Start sampling.
  try {
    sampler.start();
  } catch (std::runtime_error& exception) {
    std::cerr << exception.what() << std::endl;
    return 1;
  }

  /// Execute the benchmark (accessing cache lines in a random order).
  auto value = 0LL;
  for (auto index = 0U; index < benchmark.size(); ++index) {
    value += benchmark[index].value;

    /// Also write a value to get store events.
    benchmark.set(index, value);
  }
  asm volatile(""
               : "+r,m"(value)
               :
               : "memory"); /// We do not want the compiler to optimize away
                            /// this unused value.

  /// Stop sampling.
  sampler.stop();

  /// Get all the recorded samples.
  auto samples = sampler.result(/* sort by time */ true);
  const auto count_samples_before_filter = samples.size();

  /// Print the first samples.
  const auto count_show_samples = std::min<std::size_t>(samples.size(), 40U);
  std::cout << "\nRecorded " << count_samples_before_filter << " samples. " << samples.size()
            << " remaining after filter." << std::endl;
  std::cout << "Here are the first " << count_show_samples << " recorded samples:\n" << std::endl;
  for (auto index = 0U; index < count_show_samples; ++index) {
    const auto& sample = samples[index];

    /// Since we recorded the time, period, the instruction pointer, and the CPU
    /// id, we can only read these values.
    if (sample.time().has_value() && sample.logical_memory_address().has_value() && sample.data_src().has_value()) {
      auto data_source = "N/A";
      if (sample.data_src()->is_mem_l1()) {
        data_source = "L1d";
      } else if (sample.data_src()->is_mem_lfb()) {
        data_source = "LFB/MAB";
      } else if (sample.data_src()->is_mem_l2()) {
        data_source = "L2";
      } else if (sample.data_src()->is_mem_l3()) {
        data_source = "L3";
      } else if (sample.data_src()->is_mem_local_ram()) {
        data_source = "local RAM";
      }

      auto type = "N/A";
      if (sample.data_src()->is_load()) {
        type = "Load";
      } else if (sample.data_src()->is_store()) {
        type = "Store";
      }

      const auto weight = sample.weight().value_or(perf::Weight{ 0U, 0U, 0U });

      std::cout << "Time = " << sample.time().value() << " | Logical Mem Address = 0x" << std::hex
                << sample.logical_memory_address().value() << std::dec << " | Load Latency = " << weight.latency()
                << ", " << weight.var2() << ", " << weight.var3() << " | Type = " << type
                << " | Data Source = " << data_source << "\n";
    } else if (sample.count_loss().has_value()) {
      std::cout << "Loss = " << sample.count_loss().value() << "\n";
    }
  }
  std::cout << std::flush;

  /// Close the sampler.
  /// Note that the sampler can only be closed after reading the samples.
  sampler.close();

  return 0;
}