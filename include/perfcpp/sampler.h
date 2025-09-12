#pragma once

#include "config.h"
#include "counter_definition.h"
#include "feature.h"
#include "group.h"
#include "requested_event.h"
#include "sample.h"
#include <chrono>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace perf {
class MultiSamplerBase;
class MultiThreadSampler;
class MultiCoreSampler;
class Sampler
{
  friend MultiSamplerBase;

public:
  class Values
  {
    friend Sampler;
    friend MultiThreadSampler;
    friend MultiCoreSampler;

  public:
    /**
     * Manage to include the instruction pointer into samples.
     *
     * See https://github.com/jmuehlig/perf-cpp/blob/dev/docs/sampling.md#instruction-pointer
     *
     * @param include True, if the instruction pointer should be included.
     * @return The Values instance.
     */
    Values& instruction_pointer(const bool include) noexcept
    {
      set(PERF_SAMPLE_IP, include);
      return *this;
    }

    /**
     * Manage to include the thread id into samples.
     *
     * See https://github.com/jmuehlig/perf-cpp/blob/dev/docs/sampling.md#id-of-the-recording-thread
     *
     * @param include True, if the thread id should be included.
     * @return The Values instance.
     */
    Values& thread_id(const bool include) noexcept
    {
      set(PERF_SAMPLE_TID, include);
      return *this;
    }

    /**
     * Manage to include a timestamp into samples.
     *
     * See https://github.com/jmuehlig/perf-cpp/blob/dev/docs/sampling.md#time
     *
     * @param include True, if the timestamp should be included.
     * @return The Values instance.
     */
    Values& timestamp(const bool include) noexcept
    {
      set(PERF_SAMPLE_TIME, include);
      return *this;
    }

    /**
     * Manage to include the logical memory address into samples.
     *
     * See https://github.com/jmuehlig/perf-cpp/blob/dev/docs/sampling.md#logical-memory-address
     *
     * @param include True, if the logical memory address should be included.
     * @return The Values instance.
     */
    Values& logical_memory_address(const bool include) noexcept
    {
      set(PERF_SAMPLE_ADDR, include);
      return *this;
    }

    /**
     * Manage to include the stream id into samples.
     *
     * See https://github.com/jmuehlig/perf-cpp/blob/dev/docs/sampling.md#stream-id
     *
     * @param include True, if the stream id should be included.
     * @return The Values instance.
     */
    Values& stream_id(const bool include) noexcept
    {
      set(PERF_SAMPLE_STREAM_ID, include);
      return *this;
    }

    /**
     * Manage to include raw data into samples.
     *
     * See https://github.com/jmuehlig/perf-cpp/blob/dev/docs/sampling.md#raw-values
     *
     * @param include True, if the raw data should be included.
     * @return The Values instance.
     */
    Values& raw(const bool include) noexcept
    {
      set(PERF_SAMPLE_RAW, include);
      return *this;
    }

    /**
     * Manage to include hardware counter values into samples.
     *
     * See https://github.com/jmuehlig/perf-cpp/blob/dev/docs/sampling.md#performance-counter-values
     *
     * @param counter_names List of counter names to record.
     * @return The Values instance.
     */
    Values& counter(std::vector<std::string>&& counter_names) noexcept
    {
      _counter_names = std::move(counter_names);
      set(PERF_SAMPLE_READ, !_counter_names.empty());
      return *this;
    }

    /**
     * Manage to include the callchain into samples.
     *
     * See https://github.com/jmuehlig/perf-cpp/blob/dev/docs/sampling.md#callchain
     *
     * @param include True, if the callchain should be included.
     * @return The Values instance.
     */
    Values& callchain(const bool include) noexcept
    {
      set(PERF_SAMPLE_CALLCHAIN, include);
      return *this;
    }

    /**
     * Manage to include the callchain into samples.
     *
     * See https://github.com/jmuehlig/perf-cpp/blob/dev/docs/sampling.md#callchain
     *
     * @param max_call_stack The maximum call stack size to include.
     * @return The Values instance.
     */
    Values& callchain(const std::uint16_t max_call_stack) noexcept
    {
      _max_call_stack = max_call_stack;
      set(PERF_SAMPLE_CALLCHAIN, true);
      return *this;
    }

    /**
     * Manage to include the cpu id into samples.
     *
     * See https://github.com/jmuehlig/perf-cpp/blob/dev/docs/sampling.md#id-of-the-recording-cpu
     *
     * @param include True, if the cpu id should be included.
     * @return The Values instance.
     */
    Values& cpu_id(const bool include) noexcept
    {
      set(PERF_SAMPLE_CPU, include);
      return *this;
    }

    /**
     * Manage to include the period into samples.
     *
     * See https://github.com/jmuehlig/perf-cpp/blob/dev/docs/sampling.md#period
     *
     * @param include True, if the period should be included.
     * @return The Values instance.
     */
    Values& period(const bool include) noexcept
    {
      set(PERF_SAMPLE_PERIOD, include);
      return *this;
    }

    /**
     * Manage to include branch stacks into samples.
     *
     * See https://github.com/jmuehlig/perf-cpp/blob/dev/docs/sampling.md#branch-stack-lbr
     *
     * @param branch_types List of branch types to include.
     * @return The Values instance.
     */
    Values& branch_stack(std::vector<BranchType>&& branch_types) noexcept
    {
      this->_branch_mask = std::uint64_t{ 0U };
      for (auto branch_type : branch_types) {
        this->_branch_mask |= static_cast<std::uint64_t>(branch_type);
      }

      set(PERF_SAMPLE_BRANCH_STACK, this->_branch_mask != 0ULL);
      return *this;
    }

    /**
     * Manage to include user-level registers into samples.
     *
     * See https://github.com/jmuehlig/perf-cpp/blob/dev/docs/sampling.md#registers-in-user-level
     *
     * @param registers List of registers to include.
     * @return The Values instance.
     */
    Values& user_registers(Registers&& registers) noexcept
    {
      _user_registers = std::move(registers);
      set(PERF_SAMPLE_REGS_USER, !_user_registers.empty());
      return *this;
    }

    /**
     * Manage to include user-level registers into samples.
     *
     * See https://github.com/jmuehlig/perf-cpp/blob/dev/docs/sampling.md#registers-in-user-level
     *
     * @param registers List of registers to include.
     * @return The Values instance.
     */
    Values& user_registers(std::vector<Registers::arm>&& registers) noexcept
    {
      return user_registers(Registers{ std::move(registers) });
    }

    /**
     * Manage to include user-level registers into samples.
     *
     * See https://github.com/jmuehlig/perf-cpp/blob/dev/docs/sampling.md#registers-in-user-level
     *
     * @param registers List of registers to include.
     * @return The Values instance.
     */
    Values& user_registers(std::vector<Registers::arm64>&& registers) noexcept
    {
      return user_registers(Registers{ std::move(registers) });
    }

    /**
     * Manage to include user-level registers into samples.
     *
     * See https://github.com/jmuehlig/perf-cpp/blob/dev/docs/sampling.md#registers-in-user-level
     *
     * @param registers List of registers to include.
     * @return The Values instance.
     */
    Values& user_registers(std::vector<Registers::x86>&& registers) noexcept
    {
      return user_registers(Registers{ std::move(registers) });
    }

    /**
     * Manage to include user-level registers into samples.
     *
     * See https://github.com/jmuehlig/perf-cpp/blob/dev/docs/sampling.md#registers-in-user-level
     *
     * @param registers List of registers to include.
     * @return The Values instance.
     */
    Values& user_registers(std::vector<Registers::riscv>&& registers) noexcept
    {
      return user_registers(Registers{ std::move(registers) });
    }

    /**
     * Manage to include latency information into samples.
     *
     * See https://github.com/jmuehlig/perf-cpp/blob/dev/docs/sampling.md#memory-access-latency
     *
     * @param include True, if latency information should be included.
     * @return The Values instance.
     */
    Values& weight(const bool include) noexcept
    {
      set(PERF_SAMPLE_WEIGHT, include);
      return *this;
    }

    /**
     * Manage to include the data source for memory addresses into samples.
     *
     * See https://github.com/jmuehlig/perf-cpp/blob/dev/docs/sampling.md#data-source-of-a-memory-load
     *
     * @param include True, if the data source should be included.
     * @return The Values instance.
     */
    Values& data_source(const bool include) noexcept
    {
      set(PERF_SAMPLE_DATA_SRC, include);
      return *this;
    }

    /**
     * Manage to include hardware transaction abort reasons into samples.
     *
     * See https://github.com/jmuehlig/perf-cpp/blob/dev/docs/sampling.md#transaction-abort
     *
     * @param include True, if hardware transaction aborts should be included.
     * @return The Values instance.
     */
    Values& hardware_transaction_abort(const bool include) noexcept
    {
      set(PERF_SAMPLE_TRANSACTION, include);
      return *this;
    }

    /**
     * Manage to include the ID into samples.
     *
     * See https://github.com/jmuehlig/perf-cpp/blob/dev/docs/sampling.md#identifier
     *
     * @param include True, if the ID should be included.
     * @return The Values instance.
     */
    Values& identifier(const bool include) noexcept
    {
      set(PERF_SAMPLE_IDENTIFIER, include);
      return *this;
    }

    /**
     * Manage to include kernel-level registers into samples.
     *
     * See https://github.com/jmuehlig/perf-cpp/blob/dev/docs/sampling.md#registers-in-kernel-level
     *
     * @param registers List of registers to include.
     * @return The Values instance.
     */
    Values& kernel_registers(Registers&& registers) noexcept
    {
      _kernel_registers = std::move(registers);
      set(PERF_SAMPLE_REGS_INTR, !_kernel_registers.empty());
      return *this;
    }

    /**
     * Manage to include kernel-level registers into samples.
     *
     * See https://github.com/jmuehlig/perf-cpp/blob/dev/docs/sampling.md#registers-in-kernel-level
     *
     * @param registers List of registers to include.
     * @return The Values instance.
     */
    Values& kernel_registers(std::vector<Registers::arm>&& registers) noexcept
    {
      return kernel_registers(Registers{ std::move(registers) });
    }

    /**
     * Manage to include kernel-level registers into samples.
     *
     * See https://github.com/jmuehlig/perf-cpp/blob/dev/docs/sampling.md#registers-in-kernel-level
     *
     * @param registers List of registers to include.
     * @return The Values instance.
     */
    Values& kernel_registers(std::vector<Registers::arm64>&& registers) noexcept
    {
      return kernel_registers(Registers{ std::move(registers) });
    }

    /**
     * Manage to include kernel-level registers into samples.
     *
     * See https://github.com/jmuehlig/perf-cpp/blob/dev/docs/sampling.md#registers-in-kernel-level
     *
     * @param registers List of registers to include.
     * @return The Values instance.
     */
    Values& kernel_registers(std::vector<Registers::x86>&& registers) noexcept
    {
      return kernel_registers(Registers{ std::move(registers) });
    }

    /**
     * Manage to include kernel-level registers into samples.
     *
     * See https://github.com/jmuehlig/perf-cpp/blob/dev/docs/sampling.md#registers-in-kernel-level
     *
     * @param registers List of registers to include.
     * @return The Values instance.
     */
    Values& kernel_registers(std::vector<Registers::riscv>&& registers) noexcept
    {
      return kernel_registers(Registers{ std::move(registers) });
    }

    /**
     * Manage to include the user stack (as a list of bytes) into samples.
     *
     * See https://github.com/jmuehlig/perf-cpp/blob/dev/docs/sampling.md#user-stack
     *
     * @param max_stack_size The maximum size of the stack.
     * @return The Values instance.
     */
    Values& user_stack(const std::uint32_t max_stack_size) noexcept
    {
      _max_user_stack = max_stack_size;
      set(PERF_SAMPLE_STACK_USER, max_stack_size > 0U);
      return *this;
    }

    /**
     * Manage to include the physical memory address into samples (only available since Linux 4.13).
     *
     * See https://github.com/jmuehlig/perf-cpp/blob/dev/docs/sampling.md#physical-memory-address
     *
     * @param include True, if the physical memory address should be included.
     * @return The Values instance.
     */
    Values& physical_memory_address([[maybe_unused]] const bool include)
    {
#ifndef PERFCPP_NO_SAMPLE_PHYS_ADDR /// Sampling for physical memory address is supported since Linux 4.13
      set(PERF_SAMPLE_PHYS_ADDR, include);
#else
      throw SamplingFeatureIsNotSupported{ "physical memory address", "4.13" };
#endif
      return *this;
    }

    /**
     * Manage to include the cgroup into samples (only available since Linux 5.7).
     *
     * See https://github.com/jmuehlig/perf-cpp/blob/dev/docs/sampling.md#cgroup
     *
     * @param include True, if the cgroup should be included.
     * @return The Values instance.
     */
    Values& cgroup([[maybe_unused]] const bool include)
    {
#ifndef PERFCPP_NO_SAMPLE_CGROUP /// Sampling cgroup is supported since Linux 5.7
      set(PERF_SAMPLE_CGROUP, include);
#else
      throw SamplingFeatureIsNotSupported{ "cgroup", "5.7" };
#endif
      return *this;
    }

    /**
     * Manage to include the data page size of memory accesses into samples.
     *
     * See https://github.com/jmuehlig/perf-cpp/blob/dev/docs/sampling.md#size-of-the-data-page
     *
     * @param include True, if the data page size should be included.
     * @return The Values instance.
     */
    Values& data_page_size([[maybe_unused]] const bool include)
    {
#ifndef PERFCPP_NO_SAMPLE_DATA_PAGE_SIZE /// Sampling the data page size is supported since Linux 5.11
      set(PERF_SAMPLE_DATA_PAGE_SIZE, include);
#else
      throw SamplingFeatureIsNotSupported{ "data page size", "5.11" };
#endif
      return *this;
    }

    /**
     * Manage to include the code page size into samples.
     *
     * See https://github.com/jmuehlig/perf-cpp/blob/dev/docs/sampling.md#size-of-the-code-page
     *
     * @param include True, if the code page size should be included.
     * @return The Values instance.
     */
    Values& code_page_size([[maybe_unused]] const bool include)
    {
#ifndef PERFCPP_NO_SAMPLE_CODE_PAGE_SIZE /// Sampling the code page size is supported since Linux 5.11
      set(PERF_SAMPLE_CODE_PAGE_SIZE, include);
#else
      throw SamplingFeatureIsNotSupported{ "code page size", "5.11" };
#endif
      return *this;
    }

    /**
     * Manage to include latency information into samples (only from Linux 5.12).
     *
     * See https://github.com/jmuehlig/perf-cpp/blob/dev/docs/sampling.md#memory-access-latency
     *
     * @param include True, if latency information should be included.
     * @return The Values instance.
     */
    Values& weight_struct([[maybe_unused]] const bool include)
    {
#ifndef PERFCPP_NO_SAMPLE_WEIGHT_STRUCT /// Sampling of weight structs (in contrast to simple weight) is supported since
                                        /// Linux 5.12
      set(PERF_SAMPLE_WEIGHT_STRUCT, include);
#else
      throw SamplingFeatureIsNotSupported{ "weight struct", "5.12" };
#endif
      return *this;
    }

    /**
     * Manage to include latency information into samples. This is a wrapper for weight (until Linux 5.12) and
     * weight_struct (since Linux 5.13)
     *
     * See https://github.com/jmuehlig/perf-cpp/blob/dev/docs/sampling.md#memory-access-latency
     *
     * @param include True, if latency information should be included.
     * @return The Values instance.
     */
    Values& latency([[maybe_unused]] const bool include) noexcept
    {
#ifndef PERFCPP_NO_SAMPLE_WEIGHT_STRUCT /// Sampling of weight structs (in contrast to simple weight) is supported since
                                        /// Linux 5.12
      weight_struct(include);
#else
      weight(include);
#endif
      return *this;
    }

    /**
     * Manage to include context switches into samples.
     *
     * See https://github.com/jmuehlig/perf-cpp/blob/dev/docs/sampling.md#context-switches
     *
     * @param include True, if context switches should be included.
     * @return The Values instance.
     */
    Values& context_switch(const bool include) noexcept
    {
      _is_include_context_switch = include;
      return *this;
    }

    /**
     * Manage to include throttling events into samples.
     *
     * See https://github.com/jmuehlig/perf-cpp/blob/dev/docs/sampling.md#throttle-and-unthrottle-events
     *
     * @param include True, if throttling events should be included.
     * @return The Values instance.
     */
    Values& throttle(const bool include) noexcept
    {
      _is_include_throttle = include;
      return *this;
    }

    /**
     * Manage to include extended mmap information into samples.
     *
     * See TODO
     *
     * @param include True, if extended mmap information should be included.
     * @return The Values instance.
     */
    Values& extended_mmap_information(const bool include) noexcept
    {
      _is_include_extended_mmap_information = include;
      return *this;
    }

    /**
     * Tests, if the given perf subsystem field is set for sampling.
     *
     * @param perf_subsystem_field Field of the perf subsystem.
     * @return True, if the flag is included into samples.
     */
    [[nodiscard]] bool is_set(const std::uint64_t perf_subsystem_field) const noexcept
    {
      return static_cast<bool>(_perf_subsystem_fields_mask & perf_subsystem_field);
    }

    /**
     * @return True, if throttle samples are requested by the user.
     */
    [[nodiscard]] bool is_include_throttle() const noexcept { return _is_include_throttle; }

    /**
     * @return True, if extended mmap information is requested by the user.
     */
    [[nodiscard]] bool is_include_extended_mmap_information() const noexcept
    {
      return _is_include_extended_mmap_information;
    }

    /**
     * @return The set of requested user registers to include into the samples.
     */
    [[nodiscard]] const Registers& user_registers() const noexcept { return _user_registers; }

    /**
     * @return The set of requested kernel registers to include into the samples.
     */
    [[nodiscard]] const Registers& kernel_registers() const noexcept { return _kernel_registers; }

    /**
     * @return The maximal size of the user stack to sample–requested by the user.
     */
    [[nodiscard]] std::uint32_t max_user_stack() const noexcept { return _max_user_stack; }

    /**
     * @return List of hardware counter names to sample.
     */
    [[nodiscard]] const std::vector<std::string>& counters() const noexcept { return _counter_names; }

    /**
     * @return
     */
    [[nodiscard]] std::uint64_t branch_mask() const noexcept { return _branch_mask; }

    /**
     * @return The maximal size of the call stack to sample–requested by the user.
     */
    [[nodiscard]] std::uint16_t max_call_stack() const noexcept { return _max_call_stack; }

    /**
     * @return The mask of sample flags, i.e., values to include into the sample.
     */
    [[nodiscard]] std::uint64_t get() const noexcept { return _perf_subsystem_fields_mask; }

  private:
    /// Mask for fields to include into samples (as provided by the perf subsystem).
    std::uint64_t _perf_subsystem_fields_mask{ 0ULL };

    /// List of hardware counters and metrics to include into the sample.
    std::vector<std::string> _counter_names;

    /// List of user registers to include into the sample.
    Registers _user_registers;

    /// List of kernel registers to include into the sample.
    Registers _kernel_registers;

    /// Size of the user stack to include into the sample.
    std::uint32_t _max_user_stack{ 0U };

    /// Mask of branch flags to include into the sample.
    std::uint64_t _branch_mask{ 0ULL };

    /// Size of the call stack to include into the sample.
    std::uint16_t _max_call_stack{ 0U };

    /// Flag if context switches should be included.
    bool _is_include_context_switch{ false };

    /// Flag if throttle events should be included.
    bool _is_include_throttle{ false };

    /// Flag if extended mmap information (mmap2) should be included.
    bool _is_include_extended_mmap_information{ false };

    /**
     * En- or disables a specific perf subsystem field for sampling.
     *
     * @param perf_subsystem_field Field to include or exclude.
     * @param is_enabled Flag, if the field should be included or excluded.
     */
    void set(const std::uint64_t perf_subsystem_field, const bool is_enabled) noexcept
    {
      if (is_enabled) {
        _perf_subsystem_fields_mask |= perf_subsystem_field;
      } else {
        _perf_subsystem_fields_mask &= ~perf_subsystem_field;
      }
    }
  };

  /**
   * Represents a trigger condition for initiating a sampling event.
   */
  class Trigger
  {
  public:
    explicit Trigger(std::string&& name) noexcept
      : _name(std::move(name))
    {
    }

    Trigger(std::string&& name, const Precision precision) noexcept
      : _name(std::move(name))
      , _precision(precision)
    {
    }

    Trigger(std::string&& name, const PeriodOrFrequency period_or_frequency) noexcept
      : _name(std::move(name))
      , _period_or_frequency(period_or_frequency)
    {
    }

    Trigger(std::string&& name, const Precision precision, const PeriodOrFrequency period_or_frequency) noexcept
      : _name(std::move(name))
      , _precision(precision)
      , _period_or_frequency(period_or_frequency)
    {
    }

    ~Trigger() = default;

    /**
     * @return The name that identifies the trigger event.
     */
    [[nodiscard]] const std::string& name() const noexcept { return _name; }

    /**
     * @return The precision level associated with the sampling trigger, if set.
     */
    [[nodiscard]] std::optional<Precision> precision() const noexcept { return _precision; }

    /**
     * @return The configured period or frequency for sampling, if set.
     */
    [[nodiscard]] std::optional<PeriodOrFrequency> period_or_frequency() const noexcept { return _period_or_frequency; }

  private:
    std::string _name;
    std::optional<Precision> _precision{ std::nullopt };
    std::optional<PeriodOrFrequency> _period_or_frequency{ std::nullopt };
  };

  /**
   * Represents a counter that is configured to sample;
   * including the counter group (plus counter names) and the buffer
   * user-level buffer that is used by the perf subsystem to store the samples.
   */
  class SampleCounter
  {
  public:
    SampleCounter(Group&& group,
                  const bool has_intel_auxiliary_counter,
                  const bool has_amd_fetch_pmu_counter,
                  const bool has_amd_op_pmu_counter)
      : _group(std::move(group))
      , _has_intel_auxiliary_event(has_intel_auxiliary_counter)
      , _has_amd_ibs_fetch_pmu(has_amd_fetch_pmu_counter)
      , _has_amd_ibs_op_pmu(has_amd_op_pmu_counter)
    {
    }
    SampleCounter(Group&& group,
                  RequestedEventSet&& requested_events,
                  const bool has_auxiliary_counter,
                  const bool has_amd_fetch_pmu_counter,
                  const bool has_amd_op_pmu_counter)
      : _group(std::move(group))
      , _requested_events(std::move(requested_events))
      , _has_intel_auxiliary_event(has_auxiliary_counter)
      , _has_amd_ibs_fetch_pmu(has_amd_fetch_pmu_counter)
      , _has_amd_ibs_op_pmu(has_amd_op_pmu_counter)
    {
    }
    SampleCounter(SampleCounter&& other) noexcept = default;

    ~SampleCounter();

    [[nodiscard]] Group& group() noexcept { return _group; }
    [[nodiscard]] const Group& group() const noexcept { return _group; }
    [[nodiscard]] RequestedEventSet& requested_events() noexcept { return _requested_events; }
    [[nodiscard]] const RequestedEventSet& requested_events() const noexcept { return _requested_events; }
    [[nodiscard]] bool has_intel_auxiliary_event() const noexcept { return _has_intel_auxiliary_event; }
    [[nodiscard]] bool has_amd_fetch_pmu_counter() const noexcept { return _has_amd_ibs_fetch_pmu; }
    [[nodiscard]] bool has_amd_op_pmu_counter() const noexcept { return _has_amd_ibs_op_pmu; }

    /**
     * @return User-level buffer of the first counter (if not nullptr) or the second counter.
     */
    [[nodiscard]] std::vector<std::vector<std::byte>> consume_samples();

  private:
    /// Group including the leader that is responsible for sampling.
    Group _group;

    /// List of scheduled events if counter values are sampled.
    RequestedEventSet _requested_events;

    /// Indicates if this counter includes an auxiliary counter that is needed for some Intel architectures.
    bool _has_intel_auxiliary_event{ false };

    /// Indicates if the sampler uses the IbsFetch PMU by AMD's Instruction Based Sampling; this information is used for
    /// parsing raw data.
    bool _has_amd_ibs_fetch_pmu{ false };

    /// Indicates if the sampler uses the IbsOp PMU by AMD's Instruction Based Sampling; this information is used for
    /// parsing raw data.
    bool _has_amd_ibs_op_pmu{ false };
  };

  explicit Sampler(const CounterDefinition& counter_list, SampleConfig config = {})
    : _counter_definitions(counter_list)
    , _config(config)
  {
  }

  explicit Sampler(SampleConfig config = {})
    : Sampler(CounterDefinition::global(), config)
  {
  }

  Sampler(Sampler&&) noexcept = default;
  Sampler(const Sampler&) = default;

  ~Sampler() = default;

  /**
   * Set the trigger for sampling to a single counter.
   *
   * @param trigger_name Name of the counter that "triggers" sample recording.
   * @return Sampler
   */
  Sampler& trigger(std::string&& trigger_name)
  {
    return trigger(std::vector<std::vector<Trigger>>{ std::vector<Trigger>{ Trigger{ std::move(trigger_name) } } });
  }

  /**
   * Set the trigger for sampling to a single counter.
   *
   * @param trigger_name Name of the counter that "triggers" sample recording.
   * @param precision Precision of the event.
   * @return Sampler
   */
  Sampler& trigger(std::string&& trigger_name, const Precision precision)
  {
    return trigger(
      std::vector<std::vector<Trigger>>{ std::vector<Trigger>{ Trigger{ std::move(trigger_name), precision } } });
  }

  /**
   * Set the trigger for sampling to a single counter.
   *
   * @param trigger_name Name of the counter that "triggers" sample recording.
   * @param period Sampling period of the event.
   * @return Sampler
   */
  Sampler& trigger(std::string&& trigger_name, const class Period period)
  {
    return trigger(
      std::vector<std::vector<Trigger>>{ std::vector<Trigger>{ Trigger{ std::move(trigger_name), period } } });
  }

  /**
   * Set the trigger for sampling to a single counter.
   *
   * @param trigger_name Name of the counter that "triggers" sample recording.
   * @param frequency Sampling frequency of the event.
   * @return Sampler
   */
  Sampler& trigger(std::string&& trigger_name, const Frequency frequency)
  {
    return trigger(
      std::vector<std::vector<Trigger>>{ std::vector<Trigger>{ Trigger{ std::move(trigger_name), frequency } } });
  }

  /**
   * Set the trigger for sampling to a single counter.
   *
   * @param trigger_name Name of the counter that "triggers" sample recording.
   * @param precision Precision of the event.
   * @param period Sampling period of the event.
   * @return Sampler
   */
  Sampler& trigger(std::string&& trigger_name, const Precision precision, const class Period period)
  {
    return trigger(std::vector<std::vector<Trigger>>{
      std::vector<Trigger>{ Trigger{ std::move(trigger_name), precision, period } } });
  }

  /**
   * Set the trigger for sampling to a single counter.
   *
   * @param trigger_name Name of the counter that "triggers" sample recording.
   * @param precision Precision of the event.
   * @param frequency Sampling frequency of the event.
   * @return Sampler
   */
  Sampler& trigger(std::string&& trigger_name, const Precision precision, const Frequency frequency)
  {
    return trigger(std::vector<std::vector<Trigger>>{
      std::vector<Trigger>{ Trigger{ std::move(trigger_name), precision, frequency } } });
  }

  /**
   * Set the trigger for sampling to a list of different counters (e.g., mem loads and mem stores).
   *
   * @param trigger_names Names of the counters that "trigger" sample recording.
   * @return Sampler
   */
  Sampler& trigger(std::vector<std::string>&& trigger_names)
  {
    return trigger(std::vector<std::vector<std::string>>{ std::move(trigger_names) });
  }

  /**
   * Set the trigger for sampling to a list of different counters (e.g., mem loads and mem stores).
   *
   * @param triggers List of name-precision tuples that "trigger" sample recording.
   * @return Sampler
   */
  Sampler& trigger(std::vector<Trigger>&& triggers)
  {
    return trigger(std::vector<std::vector<Trigger>>{ std::move(triggers) });
  }

  /**
   * Set the trigger for sampling to a list of different counters (e.g., mem loads and mem stores).
   * Counters of the outer list will be grouped together, to enable auxiliary counter (e.g., needed
   * for Intel's Sapphire Rapids architecture).
   *
   * @param triggers Group of names of the counters that "trigger" sample recording.
   * @return Sampler
   */
  Sampler& trigger(std::vector<std::vector<std::string>>&& triggers);

  /**
   * Set the trigger for sampling to a list of different counters (e.g., mem loads and mem stores).
   * Counters of the outer list will be grouped together, to enable auxiliary counter (e.g., needed
   * for Intel's Sapphire Rapids architecture).
   *
   * @param triggers Group of names and precisions of the counters that "trigger" sample recording.
   * @return Sampler
   */
  Sampler& trigger(std::vector<std::vector<Trigger>>&& triggers);

  /**
   * @return Configurations to enable values that will be sampled.
   */
  [[nodiscard]] Values& values() noexcept { return _values; }

  /**
   * @return Config of the sampler.
   */
  [[nodiscard]] SampleConfig config() const noexcept { return _config; }

  /**
   * @return Config of the sampler.
   */
  [[nodiscard]] SampleConfig& config() noexcept { return _config; }

  /**
   * Opens the sampler.
   */
  void open();

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
  void close() noexcept;

  /**
   * @return List of sampled events after closing the sampler.
   */
  [[nodiscard]] std::vector<Sample> result(bool sort_by_time = true);

  /**
   * Writes the sampled result into a perf data file that can be read by the "perf report" subcommand.
   *
   * @param output_file_name Name of the perf data file.
   */
  void to_perf_file(std::string_view output_file_name);

private:
  /**
   * Transforms a list of trigger events into a single SampleCounter that includes a group of hardware events.
   *
   * @param pmu_name Name of the PMU.
   * @param trigger_group List of triggers to transform.
   * @return Sample counter, consisting of a group of trigger event(s).
   */
  [[nodiscard]] SampleCounter transform_trigger_to_sample_counter(
    std::string_view pmu_name,
    const std::vector<std::tuple<std::string_view, std::optional<Precision>, std::optional<PeriodOrFrequency>>>&
      trigger_group) const;

  /**
   * Adds the given metric and all dependent events and metrics to the event set and all events to the given group.
   *
   * @param metric Metric to add.
   * @param pmu_name PMU.
   * @param requested_event_set Requested event set.
   * @param group Group.
   */
  void add(std::pair<std::string_view, Metric&> metric,
           std::string_view pmu_name,
           RequestedEventSet& requested_event_set,
           Group& group) const;

  /**
   * Checks if the mem-loads-aux auxiliary counter is needed by the trigger group, which is true for some Intel
   * architectures (e.g., Sapphire Rapids).
   *
   * @param pmu_name Name of the PMU.
   * @param trigger_group List of triggers.
   * @return Pair of bool, the first indicates if the auxiliary counter is needed, the second indicates that the counter
   * is already included in the trigger group.
   */
  [[nodiscard]] std::pair<bool, bool> is_auxiliary_event_needed_and_already_included(
    std::string_view pmu_name,
    const std::vector<std::tuple<std::string_view, std::optional<Precision>, std::optional<PeriodOrFrequency>>>&
      trigger_group) const;

  /**
   * Consumes the sample data from the sample counters. This will only happen once; the sample data is reset when
   * starting the sampler (again).
   *
   * @return Reference of the consumed sample data (either consumed now or by an ealier call).
   */
  std::vector<std::vector<std::vector<std::byte>>>& consume_sample_data();

  const CounterDefinition& _counter_definitions;

  /// List of triggers. Each trigger will open an individual group of counters.
  /// "Normally", a 1-dimensional list would be enough, but since Intel Sapphire Rapids,
  /// we need auxiliary counters for mem-loads, mem-stores, etc.
  std::vector<std::vector<std::tuple<std::string_view, std::optional<Precision>, std::optional<PeriodOrFrequency>>>>
    _triggers;

  /// Values to record into every sample.
  Values _values;

  /// Perf config.
  SampleConfig _config;

  /// List of counter groups used to sample – will be filled when "opening" the sampler.
  std::vector<SampleCounter> _sample_counter;

  /// Flag if the sampler is already opened, i.e., the events are configured.
  /// This enables the user to open the sampler specifically – or open the
  /// sampler when starting.
  bool _is_opened{ false };

  /// Sample data per sample counter consumed from mmaped buffers. The data will be reset when starting the sampler and
  /// consumed when needing the data the first time (e.g., when calculating the result).
  std::vector<std::vector<std::vector<std::byte>>> _sample_data;
};

/**
 * The MultiSamplerBase is the foundation for samplers that have multiple sub-samplers, for example, MultiThreadSampler.
 */
class MultiSamplerBase
{
public:
  virtual ~MultiSamplerBase() = default;

  /**
   * @return Configurations to enable values that will be sampled.
   */
  [[nodiscard]] Sampler::Values& values() noexcept { return _values; }

  /**
   * @return Config of the sampler.
   */
  [[nodiscard]] SampleConfig config() const noexcept { return _config; }

  /**
   * @return Config of the sampler.
   */
  [[nodiscard]] SampleConfig& config() noexcept { return _config; }

  /**
   * Closes the sampler, including mapped buffer.
   */
  void close() noexcept
  {
    for (auto& sampler : samplers()) {
      sampler.close();
    }
  }

  /**
   * @return List of sampled events after stopping the sampler.
   */
  [[nodiscard]] std::vector<Sample> result(const bool sort_by_time = true) { return result(samplers(), sort_by_time); }

  /**
   * Writes the sampled result into a perf data file that can be read by the "perf report" subcommand.
   *
   * @param output_file_name Name of the perf data file.
   */
  void to_perf_file(std::string_view output_file_name) { to_perf_file(samplers(), output_file_name); }

protected:
  explicit MultiSamplerBase(SampleConfig config)
    : _config(config)
  {
  }

  /**
   * @return A list of multiple samplers.
   */
  [[nodiscard]] virtual std::vector<Sampler>& samplers() noexcept = 0;

  /**
   * @return A list of multiple samplers.
   */
  [[nodiscard]] virtual const std::vector<Sampler>& samplers() const noexcept = 0;

  /**
   * Creates a single result from multiple samplers.
   *
   * @param samplers List of samplers.
   * @param is_sort_by_time Flag to sort the result by timestamp attribute (if sampled).
   *
   * @return Single list of results from all incoming samplers.
   */
  [[nodiscard]] static std::vector<Sample> result(std::vector<Sampler>& samplers, bool is_sort_by_time);

  /**
   * Writes the sampled result into a perf data file that can be read by the "perf report" subcommand.
   *
   * @param samplers List of samplers.
   * @param output_file_name Name of the perf data file.
   */
  static void to_perf_file(std::vector<Sampler>& samplers, std::string_view output_file_name);

  /**
   * Initializes the given trigger(s) for the given list of samplers.
   *
   * @param samplers List of samplers.
   * @param trigger_names List of triggers.
   */
  static void trigger(std::vector<Sampler>& samplers, std::vector<std::vector<std::string>>&& trigger_names);

  /**
   * Initializes the given trigger(s) for the given list of samplers.
   *
   * @param samplers List of samplers.
   * @param triggers List of triggers.
   */
  static void trigger(std::vector<Sampler>& samplers, std::vector<std::vector<Sampler::Trigger>>&& triggers);

  /**
   * Initializes the given sampler with values and config.
   *
   * @param sampler Sampler to open.
   */
  void open(Sampler& sampler) const { open(sampler, _config); }

  /**
   * Initializes the given sampler with values and config.
   *
   * @param sampler Sampler to open.
   * @param config Config for that sampler.
   */
  void open(Sampler& sampler, SampleConfig config) const;

  /**
   * Initializes the given sampler with values and config.
   * After initialization, the sampler will be started.
   *
   * @param sampler Sampler to start.
   */
  void start(Sampler& sampler) const { start(sampler, _config); }

  /**
   * Initializes the given sampler with values and config.
   * After initialization, the sampler will be started.
   *
   * @param sampler Sampler to start.
   * @param config Config for that sampler.
   */
  void start(Sampler& sampler, SampleConfig config) const;

  /// Values to record into every sample.
  Sampler::Values _values;

  /// Perf config.
  SampleConfig _config;
};

class MultiThreadSampler final : public MultiSamplerBase
{
public:
  MultiThreadSampler(const CounterDefinition& counter_definition, std::uint16_t num_threads, SampleConfig config = {});

  explicit MultiThreadSampler(const std::uint16_t num_threads, SampleConfig config = {})
    : MultiThreadSampler(CounterDefinition::global(), num_threads, config)
  {
  }

  MultiThreadSampler(MultiThreadSampler&&) noexcept = default;

  ~MultiThreadSampler() override = default;

  /**
   * Set the trigger for sampling to a single counter.
   *
   * @param trigger_name Name of the counter that "triggers" sample recording.
   * @return MultiThreadSampler
   */
  MultiThreadSampler& trigger(std::string&& trigger_name)
  {
    return trigger(std::vector<std::vector<Sampler::Trigger>>{
      std::vector<Sampler::Trigger>{ Sampler::Trigger{ std::move(trigger_name) } } });
  }

  /**
   * Set the trigger for sampling to a single counter.
   *
   * @param trigger_name Name of the counter that "triggers" sample recording.
   * @param precision Precision of the event.
   * @return MultiThreadSampler
   */
  MultiThreadSampler& trigger(std::string&& trigger_name, const Precision precision)
  {
    return trigger(std::vector<std::vector<Sampler::Trigger>>{
      std::vector<Sampler::Trigger>{ Sampler::Trigger{ std::move(trigger_name), precision } } });
  }

  /**
   * Set the trigger for sampling to a single counter.
   *
   * @param trigger_name Name of the counter that "triggers" sample recording.
   * @param period Sampling period of the event.
   * @return MultiThreadSampler
   */
  MultiThreadSampler& trigger(std::string&& trigger_name, const class Period period)
  {
    return trigger(std::vector<std::vector<Sampler::Trigger>>{
      std::vector<Sampler::Trigger>{ Sampler::Trigger{ std::move(trigger_name), period } } });
  }

  /**
   * Set the trigger for sampling to a single counter.
   *
   * @param trigger_name Name of the counter that "triggers" sample recording.
   * @param frequency Sampling frequency of the event.
   * @return MultiThreadSampler
   */
  MultiThreadSampler& trigger(std::string&& trigger_name, const Frequency frequency)
  {
    return trigger(std::vector<std::vector<Sampler::Trigger>>{
      std::vector<Sampler::Trigger>{ Sampler::Trigger{ std::move(trigger_name), frequency } } });
  }

  /**
   * Set the trigger for sampling to a single counter.
   *
   * @param trigger_name Name of the counter that "triggers" sample recording.
   * @param precision Precision of the event.
   * @param period Sampling period of the event.
   * @return MultiThreadSampler
   */
  MultiThreadSampler& trigger(std::string&& trigger_name, const Precision precision, const class Period period)
  {
    return trigger(std::vector<std::vector<Sampler::Trigger>>{
      std::vector<Sampler::Trigger>{ Sampler::Trigger{ std::move(trigger_name), precision, period } } });
  }

  /**
   * Set the trigger for sampling to a list of different counters (e.g., mem loads and mem stores).
   *
   * @param trigger_names Names of the counters that "triggers" sample recording.
   * @return MultiThreadSampler
   */
  MultiThreadSampler& trigger(std::vector<std::string>&& trigger_names)
  {
    return trigger(std::vector<std::vector<std::string>>{ std::move(trigger_names) });
  }

  /**
   * Set the trigger for sampling to a list of different counters (e.g., mem loads and mem stores).
   *
   * @param triggers List of triggers tuples that "trigger" sample recording.
   * @return MultiThreadSampler
   */
  MultiThreadSampler& trigger(std::vector<Sampler::Trigger>&& triggers)
  {
    return trigger(std::vector<std::vector<Sampler::Trigger>>{ std::move(triggers) });
  }

  /**
   * Set the trigger for sampling to a list of different counters (e.g., mem loads and mem stores).
   * Counters of the outer list will be grouped together, to enable auxiliary counter (e.g., needed
   * for Intel's Sapphire Rapids architecture).
   *
   * @param trigger_names Group of names of the counters that "triggers" sample recording.
   * @return MultiThreadSampler
   */
  MultiThreadSampler& trigger(std::vector<std::vector<std::string>>&& trigger_names)
  {
    MultiSamplerBase::trigger(_thread_local_samplers, std::move(trigger_names));
    return *this;
  }

  /**
   * Set the trigger for sampling to a list of different counters (e.g., mem loads and mem stores).
   * Counters of the outer list will be grouped together, to enable auxiliary counter (e.g., needed
   * for Intel's Sapphire Rapids architecture).
   *
   * @param triggers Group of names and precisions of the counters that "trigger" sample recording.
   * @return MultiThreadSampler
   */
  MultiThreadSampler& trigger(std::vector<std::vector<Sampler::Trigger>>&& triggers)
  {
    MultiSamplerBase::trigger(_thread_local_samplers, std::move(triggers));
    return *this;
  }

  /**
   * Opens recording performance counters on a specific thread.
   *
   * @param thread_id Id of the thread to start.
   */
  void open(const std::uint16_t thread_id) { MultiSamplerBase::open(_thread_local_samplers[thread_id]); }

  /**
   * Opens and starts recording performance counters on a specific thread.
   *
   * @param thread_id Id of the thread to start.
   * @return True, of the performance counters could be started.
   */
  bool start(const std::uint16_t thread_id)
  {
    MultiSamplerBase::start(_thread_local_samplers[thread_id]);
    return true;
  }

  /**
   * Stops recording performance counters for a specific thread.
   *
   * @param thread_id Id of the thread to stop.
   */
  void stop(const std::uint16_t thread_id) { _thread_local_samplers[thread_id].stop(); }

  /**
   * Stops recording performance counters for all threads.
   */
  void stop()
  {
    for (auto& sampler : _thread_local_samplers) {
      sampler.stop();
    }
  }

private:
  std::vector<Sampler> _thread_local_samplers;

  /**
   * @return A list of multiple samplers.
   */
  [[nodiscard]] std::vector<Sampler>& samplers() noexcept override { return _thread_local_samplers; }

  /**
   * @return A list of multiple samplers.
   */
  [[nodiscard]] const std::vector<Sampler>& samplers() const noexcept override { return _thread_local_samplers; }
};

class MultiCoreSampler final : public MultiSamplerBase
{
public:
  MultiCoreSampler(const CounterDefinition& counter_definition,
                   std::vector<std::uint16_t>&& core_ids,
                   SampleConfig config = {});

  explicit MultiCoreSampler(std::vector<std::uint16_t>&& core_ids, SampleConfig config = {})
    : MultiCoreSampler(CounterDefinition::global(), std::move(core_ids), config)
  {
  }

  MultiCoreSampler(MultiCoreSampler&&) noexcept = default;

  ~MultiCoreSampler() override = default;

  /**
   * Set the trigger for sampling to a single counter.
   *
   * @param trigger_name Name of the counter that "triggers" sample recording.
   * @return MultiCoreSampler
   */
  MultiCoreSampler& trigger(std::string&& trigger_name)
  {
    return trigger(std::vector<std::vector<Sampler::Trigger>>{
      std::vector<Sampler::Trigger>{ Sampler::Trigger{ std::move(trigger_name) } } });
  }

  /**
   * Set the trigger for sampling to a single counter.
   *
   * @param trigger_name Name of the counter that "triggers" sample recording.
   * @param precision Precision of the event.
   * @return MultiCoreSampler
   */
  MultiCoreSampler& trigger(std::string&& trigger_name, const Precision precision)
  {
    return trigger(std::vector<std::vector<Sampler::Trigger>>{
      std::vector<Sampler::Trigger>{ Sampler::Trigger{ std::move(trigger_name), precision } } });
  }

  /**
   * Set the trigger for sampling to a single counter.
   *
   * @param trigger_name Name of the counter that "triggers" sample recording.
   * @param period Sampling period of the event.
   * @return MultiCoreSampler
   */
  MultiCoreSampler& trigger(std::string&& trigger_name, const class Period period)
  {
    return trigger(std::vector<std::vector<Sampler::Trigger>>{
      std::vector<Sampler::Trigger>{ Sampler::Trigger{ std::move(trigger_name), period } } });
  }

  /**
   * Set the trigger for sampling to a single counter.
   *
   * @param trigger_name Name of the counter that "triggers" sample recording.
   * @param frequency Sampling frequency of the event.
   * @return MultiCoreSampler
   */
  MultiCoreSampler& trigger(std::string&& trigger_name, const Frequency frequency)
  {
    return trigger(std::vector<std::vector<Sampler::Trigger>>{
      std::vector<Sampler::Trigger>{ Sampler::Trigger{ std::move(trigger_name), frequency } } });
  }

  /**
   * Set the trigger for sampling to a single counter.
   *
   * @param trigger_name Name of the counter that "triggers" sample recording.
   * @param precision Precision of the event.
   * @param period Sampling period of the event.
   * @return MultiCoreSampler
   */
  MultiCoreSampler& trigger(std::string&& trigger_name, const Precision precision, const class Period period)
  {
    return trigger(std::vector<std::vector<Sampler::Trigger>>{
      std::vector<Sampler::Trigger>{ Sampler::Trigger{ std::move(trigger_name), precision, period } } });
  }

  /**
   * Set the trigger for sampling to a list of different counters (e.g., mem loads and mem stores).
   *
   * @param trigger_names Names of the counters that "triggers" sample recording.
   * @return MultiCoreSampler
   */
  MultiCoreSampler& trigger(std::vector<std::string>&& trigger_names)
  {
    return trigger(std::vector<std::vector<std::string>>{ std::move(trigger_names) });
  }

  /**
   * Set the trigger for sampling to a list of different counters (e.g., mem loads and mem stores).
   *
   * @param triggers List of triggers tuples that "trigger" sample recording.
   * @return MultiCoreSampler
   */
  MultiCoreSampler& trigger(std::vector<Sampler::Trigger>&& triggers)
  {
    return trigger(std::vector<std::vector<Sampler::Trigger>>{ std::move(triggers) });
  }

  /**
   * Set the trigger for sampling to a list of different counters (e.g., mem loads and mem stores).
   * Counters of the outer list will be grouped together, to enable auxiliary counter (e.g., needed
   * for Intel's Sapphire Rapids architecture).
   *
   * @param trigger_names Group of names of the counters that "triggers" sample recording.
   * @return MultiCoreSampler
   */
  MultiCoreSampler& trigger(std::vector<std::vector<std::string>>&& trigger_names)
  {
    MultiSamplerBase::trigger(_core_local_samplers, std::move(trigger_names));
    return *this;
  }

  /**
   * Set the trigger for sampling to a list of different counters (e.g., mem loads and mem stores).
   * Counters of the outer list will be grouped together, to enable auxiliary counter (e.g., needed
   * for Intel's Sapphire Rapids architecture).
   *
   * @param triggers Group of names and precisions of the counters that "trigger" sample recording.
   * @return MultiCoreSampler
   */
  MultiCoreSampler& trigger(std::vector<std::vector<Sampler::Trigger>>&& triggers)
  {
    MultiSamplerBase::trigger(_core_local_samplers, std::move(triggers));
    return *this;
  }

  /**
   * Opens recording performance counters for all specified cores.
   *
   */
  void open();

  /**
   * Opens and starts recording performance counters for all specified cores.
   *
   * @return True, of the performance counters could be started.
   */
  bool start();

  /**
   * Stops the sampler.
   */
  void stop()
  {
    for (auto& sampler : this->_core_local_samplers) {
      sampler.stop();
    }
  }

private:
  /**
   * @return A list of multiple samplers.
   */
  [[nodiscard]] std::vector<Sampler>& samplers() noexcept override { return _core_local_samplers; }

  /**
   * @return A list of multiple samplers.
   */
  [[nodiscard]] const std::vector<Sampler>& samplers() const noexcept override { return _core_local_samplers; }

  /// List of samplers.
  std::vector<Sampler> _core_local_samplers;

  /// List of core ids the samplers should record on.
  std::vector<std::uint16_t> _core_ids;
};

/**
 * Comparator to order the samples by timestamp after collecting multiple samples from different threads or cores.
 */
class SampleTimestampComparator
{
public:
  bool operator()(const Sample& left, const Sample& right) const noexcept
  {
    if (!left.metadata().timestamp().has_value()) {
      return right.metadata().timestamp().has_value();
    }

    if (!right.metadata().timestamp().has_value()) {
      return false;
    }

    return left.metadata().timestamp().value() < right.metadata().timestamp().value();
  }
};

/**
 * Compares the given counter with a counter descriptor, i.e., a 3-tuple of PMU name, event name, and event
 * configuration.
 */
class CounterComparator
{
public:
  explicit CounterComparator(const Counter& counter) noexcept
    : _counter(counter)
  {
  }
  ~CounterComparator() noexcept = default;

  [[nodiscard]] bool operator()(
    const std::tuple<std::string_view, std::string_view, CounterConfig>& event_descriptor) const
  {
    return _counter == std::get<2>(event_descriptor);
  }

private:
  const Counter& _counter;
};
}