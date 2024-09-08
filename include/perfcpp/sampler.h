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
class MultiSamplerBase;
class Sampler
{
  friend MultiSamplerBase;

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
          const std::uint64_t type,
          SampleConfig config = {})
    : Sampler(counter_list, std::vector<std::vector<std::string>>{ std::move(counter_names) }, type, config)
  {
  }

  Sampler(const CounterDefinition& counter_list,
          std::vector<std::vector<std::string>>&& counter_names,
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
  [[nodiscard]] std::vector<Sample> result(bool sort_by_time = true) const;

  [[nodiscard]] std::int64_t last_error() const noexcept { return _last_error; }

private:
  const CounterDefinition& _counter_definitions;

  /// Perf config.
  SampleConfig _config;

  /// Combination of one ore more Sample::Type values.
  std::uint64_t _sample_type;

  /// Groups of real counters to measure.
  std::vector<class Group> _groups;

  /// Name of the counters to measure (per group).
  std::vector<std::vector<std::string_view>> _counter_names;

  /// Buffers for the samples of every group.
  std::vector<void*> _buffers;

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

class MultiSamplerBase
{
protected:
  [[nodiscard]] static std::vector<Sample> result(const std::vector<Sampler>& sampler, bool sort_by_time);
};

class MultiThreadSampler final : private MultiSamplerBase
{
public:
  MultiThreadSampler(const CounterDefinition& counter_list,
                     const std::string& counter_name,
                     const std::uint64_t type,
                     const std::uint16_t num_threads,
                     SampleConfig config = {})
    : MultiThreadSampler(counter_list, std::string{ counter_name }, type, num_threads, config)
  {
  }

  MultiThreadSampler(const CounterDefinition& counter_list,
                     std::string&& counter_name,
                     const std::uint64_t type,
                     const std::uint16_t num_threads,
                     SampleConfig config = {})
    : MultiThreadSampler(counter_list, std::vector<std::string>{ std::move(counter_name) }, type, num_threads, config)
  {
  }

  MultiThreadSampler(const CounterDefinition& counter_list,
                     std::vector<std::string>&& counter_names,
                     std::uint64_t type,
                     std::uint16_t num_threads,
                     SampleConfig config = {});

  MultiThreadSampler(MultiThreadSampler&&) noexcept = default;

  ~MultiThreadSampler() = default;

  /**
   * Opens and starts recording performance counters on a specific thread.
   *
   * @param thread_id Id of the thread to start.
   * @return True, of the performance counters could be started.
   */
  bool start(const std::uint16_t thread_id) { return _thread_local_samplers[thread_id].start(); }

  /**
   * Stops recording performance counters for a specific thread.
   *
   * @param thread_id Id of the thread to stop.
   */
  void stop(const std::uint16_t thread_id) { _thread_local_samplers[thread_id].stop(); }

  /**
   * Closes the sampler, including mapped buffer, for all threads.
   */
  void close()
  {
    for (auto& sampler : _thread_local_samplers) {
      sampler.close();
    }
  }

  /**
   * @return List of sampled events after closing the sampler.
   */
  [[nodiscard]] std::vector<Sample> result(const bool sort_by_time = true) const
  {
    return MultiSamplerBase::result(_thread_local_samplers, sort_by_time);
  }

private:
  std::vector<Sampler> _thread_local_samplers;
};

class MultiCoreSampler final : private MultiSamplerBase
{
public:
  MultiCoreSampler(const CounterDefinition& counter_list,
                   const std::string& counter_name,
                   const std::uint64_t type,
                   std::vector<std::uint16_t>&& core_ids,
                   SampleConfig config = {})
    : MultiCoreSampler(counter_list, std::string{ counter_name }, type, std::move(core_ids), config)
  {
  }

  MultiCoreSampler(const CounterDefinition& counter_list,
                   std::string&& counter_name,
                   const std::uint64_t type,
                   std::vector<std::uint16_t>&& core_ids,
                   SampleConfig config = {})
    : MultiCoreSampler(counter_list,
                       std::vector<std::string>{ std::move(counter_name) },
                       type,
                       std::move(core_ids),
                       config)
  {
  }

  MultiCoreSampler(const CounterDefinition& counter_list,
                   std::vector<std::string>&& counter_names,
                   std::uint64_t type,
                   std::vector<std::uint16_t>&& core_ids,
                   SampleConfig config = {});

  MultiCoreSampler(MultiCoreSampler&&) noexcept = default;

  ~MultiCoreSampler() = default;

  /**
   * Opens and starts recording performance counters for all specified cores.
   *
   * @return True, of the performance counters could be started.
   */
  bool start();

  /**
   * Stops recording performance counters for all specified cores.
   */
  void stop()
  {
    for (auto& sampler : _core_local_samplers) {
      sampler.stop();
    }
  }

  /**
   * Closes the sampler, including mapped buffer, for all threads.
   */
  void close()
  {
    for (auto& sampler : _core_local_samplers) {
      sampler.close();
    }
  }

  /**
   * @return List of sampled events after closing the sampler.
   */
  [[nodiscard]] std::vector<Sample> result(const bool sort_by_time = true) const
  {
    return MultiSamplerBase::result(_core_local_samplers, sort_by_time);
  }

private:
  std::vector<Sampler> _core_local_samplers;
};

class SampleTimestampComparator
{
public:
  bool operator()(const Sample& left, const Sample& right) const { return left.time().value() < right.time().value(); }
};
}