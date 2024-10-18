#include "access_benchmark.h"
#include <iostream>
#include <perfcpp/hardware_info.h>
#include <perfcpp/sampler.h>

int
main()
{
  std::cout << "libperf-cpp example: Record raw samples by AMD IBS for "
               "single-threaded random access to an in-memory array."
            << std::endl;

  /// Initialize counter definitions.
  /// Note that the perf::CounterDefinition holds all counter names and must be
  /// alive until the benchmark finishes.
  auto counter_definitions = perf::CounterDefinition{};

  /// Initialize sampler.
  auto perf_config = perf::SampleConfig{};
  perf_config.period(10000U); /// Record every 10,000th event.

  auto sampler = perf::Sampler{ counter_definitions, perf_config };

  /// Setup which counters trigger the writing of samples (depends on the underlying hardware substrate).
  if (perf::HardwareInfo::is_amd_ibs_supported()) {
    sampler.trigger("ibs_op", perf::Precision::MustHaveZeroSkid);
  } else {
    std::cout << "Error: The example for raw sampling is only implemented for AMD IBS." << std::endl;
    return 1;
  }

  /// Setup which data will be included into samples (raw data, instruction, logical memory).
  sampler.values().raw(true).instruction_pointer(true).logical_memory_address(true);

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

  /// Get all the recorded samples.
  auto samples = sampler.result();

  /// Print the first samples.
  const auto count_show_samples = std::min<std::size_t>(samples.size(), 40U);
  std::cout << "\nRecorded " << samples.size() << " samples. " << samples.size() << std::endl;
  std::cout << "Here are the first " << count_show_samples << " recorded samples:\n" << std::endl;
  for (auto index = 0U; index < count_show_samples; ++index) {
    const auto& sample = samples[index];

    /// See the AMD IBS manual at
    /// https://www.amd.com/content/dam/amd/en/documents/processor-tech-docs/programmer-references/24593.pdf (from page
    /// 428).

    if (sample.raw().has_value() && sample.instruction_pointer().has_value()) {
      const auto* registers =
        reinterpret_cast<const std::uint64_t*>(sample.raw().value().data() + /* 4 byte offset */ 4);

      // const auto ibs_execution_control_reg = registers[0U]; /// IBS Execution Control Register (see page 429 in the
      // referenced AMD manual)
      const auto ibs_rip_reg =
        registers[1U]; /// IBS Op Linear Address Register (see page 430 in the referenced AMD manual)
      // const auto ibs_data1_reg = registers[2U];         /// IBS Op Data 1 Register (see page 431 in the referenced
      // AMD manual) const auto ibs_data2_reg = registers[3U];      // IBS Op Data 2 Register (see page 432 in the
      // referenced AMD manual)
      const auto ibs_data3_reg = registers[4U]; // IBS Op Data 3 Register (see page 432 in the referenced AMD manual)
      const auto ibs_linear_addr_reg =
        registers[5]; // IBS Data Cache Linear Address Register (see page 434 in the referenced AMD manual)

      std::cout << "Raw (" << sample.raw().value().size() << " bytes): IP (from raw) = 0x" << std::hex << ibs_rip_reg
                << std::dec;
      if ((ibs_data3_reg & 1U << 17)) { /// Check if the address is valid
        std::cout << " ; Addr (from raw) = 0x" << std::hex << ibs_linear_addr_reg << std::dec;
      } else {
        std::cout << " ; Addr (from raw) not valid";
      }

      std::cout << " | IP (from perf) = 0x" << std::hex << sample.instruction_pointer().value() << std::dec;
      std::cout << " | Addr (from perf) = 0x" << std::hex << sample.logical_memory_address().value_or(0) << std::dec
                << "\n";
    }
  }
  std::cout << std::flush;

  /// Close the sampler.
  /// Note that the sampler can only be closed after reading the samples.
  sampler.close();

  return 0;
}