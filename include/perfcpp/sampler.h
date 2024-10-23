#pragma once

#include "config.h"
#include "counter_definition.h"
#include "feature.h"
#include "group.h"
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
  /**
   * What to sample.
   */
  enum
    [[deprecated("Sampler::Type will be replaced by Sampler::values() interface from v.0.9.0.")]] Type : std::uint64_t{
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
#ifndef PERFCPP_NO_SAMPLE_PHYS_ADDR
      PhysicalMemAddress = PERF_SAMPLE_PHYS_ADDR,
#else
      PhysicalMemAddress = std::uint64_t(1U) << 63,
#endif

#ifndef PERFCPP_NO_SAMPLE_DATA_PAGE_SIZE /// PERF_SAMPLE_DATA_PAGE_SIZE is provided since Linux Kernel 5.11
      DataPageSize = PERF_SAMPLE_DATA_PAGE_SIZE,
#else
      DataPageSize = std::uint64_t(1U) << 63,
#endif

#ifndef PERFCPP_NO_SAMPLE_CODE_PAGE_SIZE /// PERF_SAMPLE_CODE_PAGE_SIZE is provided since Linux Kernel 5.11
      CodePageSize = PERF_SAMPLE_CODE_PAGE_SIZE,
#else
      CodePageSize = std::uint64_t(1U) << 63,
#endif

#ifndef PERFCPP_NO_SAMPLE_WEIGHT_STRUCT /// PERF_SAMPLE_WEIGHT_STRUCT is provided since Linux Kernel 5.12
      WeightStruct = PERF_SAMPLE_WEIGHT_STRUCT
#else
      WeightStruct = std::uint64_t(1U) << 63,
#endif
    };

  class Values
  {
    friend Sampler;
    friend MultiThreadSampler;
    friend MultiCoreSampler;

  public:
    Values& instruction_pointer(const bool include) noexcept
    {
      set(PERF_SAMPLE_IP, include);
      return *this;
    }

    Values& thread_id(const bool include) noexcept
    {
      set(PERF_SAMPLE_TID, include);
      return *this;
    }

    Values& time(const bool include) noexcept
    {
      set(PERF_SAMPLE_TIME, include);
      return *this;
    }

    Values& logical_memory_address(const bool include) noexcept
    {
      set(PERF_SAMPLE_ADDR, include);
      return *this;
    }

    Values& stream_id(const bool include) noexcept
    {
      set(PERF_SAMPLE_STREAM_ID, include);
      return *this;
    }

    Values& raw(const bool include) noexcept
    {
      set(PERF_SAMPLE_RAW, include);
      return *this;
    }

    Values& counter(std::vector<std::string>&& counter_names) noexcept
    {
      _counter_names = std::move(counter_names);
      set(PERF_SAMPLE_READ, !_counter_names.empty());
      return *this;
    }

    Values& callchain(const bool include) noexcept
    {
      set(PERF_SAMPLE_CALLCHAIN, include);
      return *this;
    }

    Values& callchain(const std::uint16_t max_call_stack) noexcept
    {
      _max_call_stack = max_call_stack;
      set(PERF_SAMPLE_CALLCHAIN, true);
      return *this;
    }

    Values& cpu_id(const bool include) noexcept
    {
      set(PERF_SAMPLE_CPU, include);
      return *this;
    }

    Values& period(const bool include) noexcept
    {
      set(PERF_SAMPLE_PERIOD, include);
      return *this;
    }

    Values& branch_stack(std::vector<BranchType>&& branch_types) noexcept
    {
      this->_branch_mask = std::uint64_t{ 0U };
      for (auto branch_type : branch_types) {
        this->_branch_mask |= static_cast<std::uint64_t>(branch_type);
      }

      set(PERF_SAMPLE_BRANCH_STACK, this->_branch_mask != 0ULL);
      return *this;
    }

    Values& user_registers(Registers registers) noexcept
    {
      _user_registers = registers;
      set(PERF_SAMPLE_REGS_USER, _user_registers.size() > 0U);
      return *this;
    }

    Values& weight(const bool include) noexcept
    {
      set(PERF_SAMPLE_WEIGHT, include);
      return *this;
    }

    Values& data_src(const bool include) noexcept
    {
      set(PERF_SAMPLE_DATA_SRC, include);
      return *this;
    }

    Values& identifier(const bool include) noexcept
    {
      set(PERF_SAMPLE_IDENTIFIER, include);
      return *this;
    }

    Values& kernel_registers(Registers registers) noexcept
    {
      _kernel_registers = registers;
      set(PERF_SAMPLE_REGS_INTR, _kernel_registers.size() > 0U);
      return *this;
    }

    Values& physical_memory_address(const bool include) noexcept
    {
#ifndef PERFCPP_NO_SAMPLE_PHYS_ADDR
      set(PERF_SAMPLE_PHYS_ADDR, include);
#endif
      return *this;
    }

    Values& cgroup(const bool include) noexcept
    {
#ifndef PERFCPP_NO_SAMPLE_CGROUP
      set(PERF_SAMPLE_CGROUP, include);
#endif
      return *this;
    }

    Values& data_page_size([[maybe_unused]] const bool include) noexcept
    {
#ifndef PERFCPP_NO_SAMPLE_DATA_PAGE_SIZE
      set(PERF_SAMPLE_DATA_PAGE_SIZE, include);
#endif
      return *this;
    }

    Values& code_page_size([[maybe_unused]] const bool include) noexcept
    {
#ifndef PERFCPP_NO_SAMPLE_CODE_PAGE_SIZE
      set(PERF_SAMPLE_CODE_PAGE_SIZE, include);
#endif
      return *this;
    }

    Values& weight_struct([[maybe_unused]] const bool include) noexcept
    {
#ifndef PERFCPP_NO_SAMPLE_WEIGHT_STRUCT
      set(PERF_SAMPLE_WEIGHT_STRUCT, include);
#endif
      return *this;
    }

    Values& context_switch(const bool include) noexcept
    {
      _is_include_context_switch = include;
      return *this;
    }

    Values& throttle(const bool include) noexcept
    {
      _is_include_throttle = include;
      return *this;
    }

    [[nodiscard]] bool is_set(const std::uint64_t perf_field) const noexcept
    {
      return static_cast<bool>(_mask & perf_field);
    }

    [[nodiscard]] Registers user_registers() const noexcept { return _user_registers; }
    [[nodiscard]] Registers kernel_registers() const noexcept { return _kernel_registers; }
    [[nodiscard]] const std::vector<std::string>& counters() const noexcept { return _counter_names; }
    [[nodiscard]] std::uint64_t branch_mask() const noexcept { return _branch_mask; }
    [[nodiscard]] std::uint16_t max_call_stack() const noexcept { return _max_call_stack; }

    [[nodiscard]] std::uint64_t get() const noexcept { return _mask; }

  private:
    std::uint64_t _mask{ 0ULL };
    std::vector<std::string> _counter_names;
    Registers _user_registers;
    Registers _kernel_registers;
    std::uint64_t _branch_mask{ 0ULL };

    std::uint16_t _max_call_stack{ 0U };

    bool _is_include_context_switch{ false };
    bool _is_include_throttle{ false };

    void set(const std::uint64_t perf_field, const bool is_enabled) noexcept
    {
      if (is_enabled) {
        enable(perf_field);
      } else {
        disable(perf_field);
      }
    }

    void enable(const std::uint64_t perf_field) noexcept { _mask |= perf_field; }

    void disable(const std::uint64_t perf_field) noexcept { _mask &= ~perf_field; }
  };

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

    [[nodiscard]] const std::string& name() const noexcept { return _name; }
    [[nodiscard]] std::optional<Precision> precision() const noexcept { return _precision; }
    [[nodiscard]] std::optional<PeriodOrFrequency> period_or_frequency() const noexcept { return _period_or_frequency; }

  private:
    std::string _name;
    std::optional<Precision> _precision{ std::nullopt };
    std::optional<PeriodOrFrequency> _period_or_frequency{ std::nullopt };
  };

  [[deprecated("Creating samplers with counters and sampling type will be replaced by Sampler::trigger() and "
               "Sampler::values() interfaces.")]] Sampler(const CounterDefinition& counter_list,
                                                          const std::string& counter_name,
                                                          const std::uint64_t type,
                                                          SampleConfig config = {})
    : Sampler(counter_list, std::string{ counter_name }, type, config)
  {
  }

  [[deprecated("Creating samplers with counters and sampling type will be replaced by Sampler::trigger() and "
               "Sampler::values() interfaces.")]] Sampler(const CounterDefinition& counter_list,
                                                          std::string&& counter_name,
                                                          const std::uint64_t type,
                                                          SampleConfig config = {})
    : Sampler(counter_list, std::vector<std::string>{ std::move(counter_name) }, type, config)
  {
  }

  [[deprecated("Creating samplers with counters and sampling type will be replaced by Sampler::trigger() and "
               "Sampler::values() interfaces.")]] Sampler(const CounterDefinition& counter_list,
                                                          std::vector<std::string>&& counter_names,
                                                          std::uint64_t type,
                                                          SampleConfig config = {});

  explicit Sampler(const CounterDefinition& counter_list, SampleConfig config = {})
    : _counter_definitions(counter_list)
    , _config(config)
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
  void close();

  /**
   * @return List of sampled events after closing the sampler.
   */
  [[nodiscard]] std::vector<Sample> result(bool sort_by_time = true) const;

  /**
   * @return The latest error reported by the sampler.
   */
  [[deprecated("Error handling will be moved to exceptions only.")]] [[nodiscard]] std::int64_t last_error()
    const noexcept
  {
    return _last_error;
  }

private:
  /**
   * Represents a counter that is configured to somple;
   * including the counter group (plus counter names) and the buffer
   * user-level buffer that is used by the perf subsystem to store the samples.
   */
  class SampleCounter
  {
  public:
    explicit SampleCounter(Group&& group)
      : _group(std::move(group))
    {
    }
    SampleCounter(Group&& group, std::vector<std::string_view>&& counter_names)
      : _group(std::move(group))
      , _counter_names(std::move(counter_names))
    {
    }

    void buffer(void* buffer) noexcept { _buffer = buffer; }

    [[nodiscard]] Group& group() noexcept { return _group; }
    [[nodiscard]] const Group& group() const noexcept { return _group; }
    [[nodiscard]] void* buffer() const noexcept { return _buffer; }
    [[nodiscard]] const std::vector<std::string_view>& counter_names() const noexcept { return _counter_names; }

  private:
    /// Group including the leader that is responsible for sampling.
    Group _group;

    /// User-level, mmap-ed buffer that receives the samples by the perf subsystem.
    void* _buffer{ nullptr };

    /// List of counter names if counter values are sampled.
    std::vector<std::string_view> _counter_names;
  };

  /**
   * The UserLevelBufferEntry represents an entry in the user-level buffer filled by the perf-subsystem by parsing the
   * hardware-related samples. This helper assists in consuming data from the buffer and turning it into Samples.
   */
  class UserLevelBufferEntry
  {
  public:
    UserLevelBufferEntry(perf_event_header* header) noexcept
      : _head(std::uintptr_t(header + 1U))
      , _misc(header->misc)
      , _type(header->type)
    {
    }
    ~UserLevelBufferEntry() noexcept = default;

    template<typename T>
    [[nodiscard]] T read() noexcept
    {
      const auto data = *reinterpret_cast<T*>(_head);
      _head += sizeof(T);

      return data;
    }

    template<typename T>
    [[nodiscard]] const T* read(const std::size_t size) noexcept
    {
      auto* begin = reinterpret_cast<T*>(_head);
      _head += sizeof(T) * size;

      return begin;
    }

    template<typename T>
    void skip() noexcept
    {
      _head += sizeof(T);
    }

    template<typename T>
    void skip(const std::size_t size) noexcept
    {
      _head += sizeof(T) * size;
    }

    template<typename T>
    T as() const noexcept
    {
      return reinterpret_cast<T>(_head);
    }

    [[nodiscard]] Sample::Mode mode() const noexcept;

    [[nodiscard]] bool is_sample_event() const noexcept { return _type == PERF_RECORD_SAMPLE; }
    [[nodiscard]] bool is_loss_event() const noexcept { return _type == PERF_RECORD_LOST_SAMPLES; }
    [[nodiscard]] bool is_context_switch_event() const noexcept
    {
#ifndef PERFCPP_NO_RECORD_SWITCH
      return _type == PERF_RECORD_SWITCH || _type == PERF_RECORD_SWITCH_CPU_WIDE;
#else
      return false;
#endif
    }
    [[nodiscard]] bool is_context_switch_cpu_wide() const noexcept {
#ifndef PERFCPP_NO_RECORD_SWITCH
      return _type == PERF_RECORD_SWITCH_CPU_WIDE;
#else
      return false;
#endif
    }
    [[nodiscard]] bool is_cgroup_event() const noexcept
    {
#ifndef PERFCPP_NO_RECORD_CGROUP
      return _type == PERF_RECORD_CGROUP;
#else
      return false;
#endif
    }
    [[nodiscard]] bool is_throttle_event() const noexcept
    {
      return _type == PERF_RECORD_THROTTLE || _type == PERF_RECORD_UNTHROTTLE;
    }
    [[nodiscard]] bool is_throttle() const noexcept { return _type == PERF_RECORD_THROTTLE; }

    [[nodiscard]] bool is_exact_ip() const noexcept { return _misc & PERF_RECORD_MISC_EXACT_IP; }
    [[nodiscard]] bool is_context_switch_out() const noexcept {
#ifndef PERFCPP_NO_RECORD_SWITCH
      return _misc & PERF_RECORD_MISC_SWITCH_OUT;
#else
      return false;
#endif
    }
    [[nodiscard]] bool is_context_switch_out_preempt() const noexcept
    {
#ifndef PERFCPP_NO_RECORD_MISC_SWITCH_OUT_PREEMPT
      return _misc & PERF_RECORD_MISC_SWITCH_OUT_PREEMPT;
#else
      return false;
#endif
    }

  private:
    std::uintptr_t _head;
    const std::uint16_t _misc;
    const std::uint32_t _type;
  };

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

  /**
   * Reads the sample_id struct from the data located at sample_ptr into the provided sample.
   *
   * @param sample Sample to read the data into.
   */
  void read_sample_id(UserLevelBufferEntry& entry, Sample& sample) const noexcept;

  /**
   * Translates the current entry from the user-level buffer into a "normal" sample.
   *
   * @param entry Entry of the user-level buffer.
   * @param sample_counter The SampleCounter the entry is linked to in order to get the recorded counters (if any).
   *
   * @return Sample.
   */
  [[nodiscard]] perf::Sample read_sample_event(UserLevelBufferEntry entry, const SampleCounter& sample_counter) const;

  /**
   * Translates the current entry from the user-level buffer into a lost sample.
   *
   * @param entry Entry of the user-level buffer.
   *
   * @return Sample containing the loss.
   */
  [[nodiscard]] perf::Sample read_loss_event(UserLevelBufferEntry entry) const;

  /**
   * Translates the current entry from the user-level buffer into a context switch sample.
   *
   * @param entry Entry of the user-level buffer.
   *
   * @return Sample containing the context switch.
   */
  [[nodiscard]] perf::Sample read_context_switch_event(UserLevelBufferEntry entry) const;

  /**
   * Translates the current entry from the user-level buffer into a cgroup sample.
   *
   * @param entry Entry of the user-level buffer.
   *
   * @return Sample containing the cgroup.
   */
  [[nodiscard]] perf::Sample read_cgroup_event(UserLevelBufferEntry entry) const;

  /**
   * Translates the current entry from the user-level buffer into a throttle or unthrottle sample.
   *
   * @param entry Entry of the user-level buffer.
   *
   * @return Sample containing the throttle.
   */
  [[nodiscard]] perf::Sample read_throttle_event(UserLevelBufferEntry entry) const;

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

  /// Will be assigned to errorno.
  /// Attention: Will be deprecated when switching to exceptions only.
  std::int64_t _last_error{ 0 };
};

class MultiSamplerBase
{
public:
  ~MultiSamplerBase() = default;

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
  void close()
  {
    for (auto& sampler : samplers()) {
      sampler.close();
    }
  }

  /**
   * @return List of sampled events after stopping the sampler.
   */
  [[nodiscard]] std::vector<Sample> result(const bool sort_by_time = true) const
  {
    return result(samplers(), sort_by_time);
  }

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
   * @param sampler List of samplers.
   * @param sort_by_time Flag to sort the result by timestamp attribute (if sampled).
   * @return Single list of results from all incoming samplers.
   */
  [[nodiscard]] static std::vector<Sample> result(const std::vector<Sampler>& sampler, bool sort_by_time);

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
   * @param trigger_names List of triggers.
   */
  static void trigger(std::vector<Sampler>& samplers, std::vector<std::vector<Sampler::Trigger>>&& triggers);

  /**
   * Initializes the given sampler with values and config.
   *
   * @param sampler Sampler to open.
   */
  void open(Sampler& sampler) { open(sampler, _config); }

  /**
   * Initializes the given sampler with values and config.
   *
   * @param sampler Sampler to open.
   * @param config Config for that sampler.
   */
  void open(Sampler& sampler, SampleConfig config);

  /**
   * Initializes the given sampler with values and config.
   * After initialization, the sampler will be started.
   *
   * @param sampler Sampler to start.
   */
  void start(Sampler& sampler) { start(sampler, _config); }

  /**
   * Initializes the given sampler with values and config.
   * After initialization, the sampler will be started.
   *
   * @param sampler Sampler to start.
   * @param config Config for that sampler.
   */
  void start(Sampler& sampler, SampleConfig config);

  /// Values to record into every sample.
  Sampler::Values _values;

  /// Perf config.
  SampleConfig _config;
};

class MultiThreadSampler final : public MultiSamplerBase
{
public:
  [[deprecated(
    "Creating samplers with counters and sampling type will be replaced by MultiThreadSampler::trigger() "
    "and MultiThreadSampler::values() interfaces.")]] MultiThreadSampler(const CounterDefinition& counter_list,
                                                                         const std::string& counter_name,
                                                                         const std::uint64_t type,
                                                                         const std::uint16_t num_threads,
                                                                         SampleConfig config = {})
    : MultiThreadSampler(counter_list, std::string{ counter_name }, type, num_threads, config)
  {
  }

  [[deprecated(
    "Creating samplers with counters and sampling type will be replaced by MultiThreadSampler::trigger() "
    "and MultiThreadSampler::values() interfaces.")]] MultiThreadSampler(const CounterDefinition& counter_list,
                                                                         std::string&& counter_name,
                                                                         const std::uint64_t type,
                                                                         const std::uint16_t num_threads,
                                                                         SampleConfig config = {})
    : MultiThreadSampler(counter_list, std::vector<std::string>{ std::move(counter_name) }, type, num_threads, config)
  {
  }

  [[deprecated(
    "Creating samplers with counters and sampling type will be replaced by MultiThreadSampler::trigger() "
    "and MultiThreadSampler::values() interfaces.")]] MultiThreadSampler(const CounterDefinition& counter_list,
                                                                         std::vector<std::string>&& counter_names,
                                                                         std::uint64_t type,
                                                                         std::uint16_t num_threads,
                                                                         SampleConfig config = {});

  explicit MultiThreadSampler(const CounterDefinition& counter_list,
                              std::uint16_t num_threads,
                              SampleConfig config = {});

  MultiThreadSampler(MultiThreadSampler&&) noexcept = default;

  ~MultiThreadSampler() = default;

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
   * @param trigger_name Name of the counters that "triggers" sample recording.
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
   * @param trigger_name Group of names of the counters that "triggers" sample recording.
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
  [[deprecated(
    "Creating samplers with counters and sampling type will be replaced by MultiCoreSampler::trigger() and "
    "MultiCoreSampler::values() interfaces from v.0.9.0.")]] MultiCoreSampler(const CounterDefinition& counter_list,
                                                                              const std::string& counter_name,
                                                                              const std::uint64_t type,
                                                                              std::vector<std::uint16_t>&& core_ids,
                                                                              SampleConfig config = {})
    : MultiCoreSampler(counter_list, std::string{ counter_name }, type, std::move(core_ids), config)
  {
  }

  [[deprecated(
    "Creating samplers with counters and sampling type will be replaced by MultiCoreSampler::trigger() and "
    "MultiCoreSampler::values() interfaces from v.0.9.0.")]] MultiCoreSampler(const CounterDefinition& counter_list,
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

  [[deprecated(
    "Creating samplers with counters and sampling type will be replaced by MultiCoreSampler::trigger() and "
    "MultiCoreSampler::values() interfaces from v.0.9.0.")]] MultiCoreSampler(const CounterDefinition& counter_list,
                                                                              std::vector<std::string>&& counter_names,
                                                                              std::uint64_t type,
                                                                              std::vector<std::uint16_t>&& core_ids,
                                                                              SampleConfig config = {});

  explicit MultiCoreSampler(const CounterDefinition& counter_list,
                            std::vector<std::uint16_t>&& core_ids,
                            SampleConfig config = {});

  MultiCoreSampler(MultiCoreSampler&&) noexcept = default;

  ~MultiCoreSampler() = default;

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
   * @param trigger_name Name of the counters that "triggers" sample recording.
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
   * @param trigger_name Group of names of the counters that "triggers" sample recording.
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

class SampleTimestampComparator
{
public:
  bool operator()(const Sample& left, const Sample& right) const { return left.time().value() < right.time().value(); }
};
}