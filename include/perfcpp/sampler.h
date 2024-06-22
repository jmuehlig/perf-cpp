#pragma once

#include "config.h"
#include "counter_definition.h"
#include "group.h"
#include "sample.h"
#include <chrono>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace perf {
class Sampler
{
public:
  /**
   * What to sample.
   */
  enum Type : std::uint64_t
  {
    InstructionPointer = PERF_SAMPLE_IP,
    ThreadId = PERF_SAMPLE_TID,
    Time = PERF_SAMPLE_TIME,
    LogicalMemAddress = PERF_SAMPLE_ADDR,
    CounterValues = PERF_SAMPLE_READ,
    Callchain = PERF_SAMPLE_CALLCHAIN,
    CPU = PERF_SAMPLE_CPU,
    Period = PERF_SAMPLE_PERIOD,
    BranchStack = PERF_SAMPLE_BRANCH_STACK,
    UserRegisters = PERF_SAMPLE_REGS_USER,
    Weight = PERF_SAMPLE_WEIGHT,
    DataSource = PERF_SAMPLE_DATA_SRC,
    Identifier = PERF_SAMPLE_IDENTIFIER,
    KernelRegisters = PERF_SAMPLE_REGS_INTR,
    PhysicalMemAddress = PERF_SAMPLE_PHYS_ADDR,

#ifndef NO_PERF_SAMPLE_DATA_PAGE_SIZE /// PERF_SAMPLE_DATA_PAGE_SIZE is provided since Linux Kernel 5.11
    DataPageSize = PERF_SAMPLE_DATA_PAGE_SIZE,
#else
    DataPageSize = std::uint64_t(1U) << 63,
#endif

#ifndef NO_PERF_SAMPLE_CODE_PAGE_SIZE /// PERF_SAMPLE_CODE_PAGE_SIZE is provided since Linux Kernel 5.11
    CodePageSize = PERF_SAMPLE_CODE_PAGE_SIZE,
#else
    CodePageSize = std::uint64_t(1U) << 63,
#endif

#ifndef NO_PERF_SAMPLE_WEIGHT_STRUCT /// PERF_SAMPLE_WEIGHT_STRUCT is provided since Linux Kernel 5.12
    WeightStruct = PERF_SAMPLE_WEIGHT_STRUCT
#else
    WeightStruct = std::uint64_t(1U) << 63,
#endif
  };

  Sampler(const CounterDefinition& counter_list,
          const std::string& counter_name,
          const std::uint64_t type,
          SampleConfig config = {})
    : Sampler(counter_list, std::string{ counter_name }, type, config)
  {
  }

  Sampler(const CounterDefinition& counter_list,
          std::string&& counter_name,
          const std::uint64_t type,
          SampleConfig config = {})
    : Sampler(counter_list, std::vector<std::string>{ std::move(counter_name) }, type, config)
  {
  }

  Sampler(const CounterDefinition& counter_list,
          std::vector<std::string>&& counter_names,
          std::uint64_t type,
          SampleConfig config = {});

  Sampler(Sampler&&) noexcept = default;
  Sampler(const Sampler&) = default;

  ~Sampler() = default;

  /**
   * Opens and starts recording performance counters.
   *
   * @return True, of the performance counters could be started.
   */
  bool start();

  /**
   * Stops recording performance counters.
   */
  void stop();

  /**
   * Closes the sampler, including mapped buffer.
   */
  void close();

  /**
   * @return List of sampled events after closing the sampler.
   */
  [[nodiscard]] std::vector<Sample> result() const;

  [[nodiscard]] std::int64_t last_error() const noexcept { return _last_error; }

private:
  const CounterDefinition& _counter_definitions;

  /// Perf config.
  SampleConfig _config;

  /// Combination of one ore more Sample::Type values.
  std::uint64_t _sample_type;

  /// Real counters to measure.
  class Group _group;

  /// Name of the counters to measure.
  std::vector<std::string> _counter_names;

  /// Buffer for the samples.
  void* _buffer{ nullptr };

  /// Will be assigned to errorno.
  std::int64_t _last_error{ 0 };

  /**
   * Opens the sampler.
   *
   * @return True, if the sampler could be opened.
   */
  [[nodiscard]] bool open();

  /**
   * Read format for sampled counter values.
   */
  struct read_format
  {
    /// Value and ID delivered by perf.
    struct value
    {
      std::uint64_t value;
      std::uint64_t id;
    };

    /// Number of counters in the following array.
    std::uint64_t count_members;
    std::array<value, Group::MAX_MEMBERS> values;
  };
};
}