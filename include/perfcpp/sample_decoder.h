#pragma once
#include "feature.h"
#include "metadata.h"
#include "requested_event.h"
#include "sampler.h"
#include <cstddef>
#include <cstdint>
#include <linux/perf_event.h>
#include <utility>
#include <vector>

namespace perf {
/**
 * The SampleDecoder translates raw values emitted by the perf subsystem into Samples.
 */
class SampleDecoder
{
private:
  /**
   * The UserLevelBufferEntry represents an entry in the user-level buffer filled by the perf subsystem by parsing the
   * hardware-related samples. This helper assists in consuming data from the buffer and turning it into Samples.
   */
  class SampleIterator
  {
  public:
    explicit SampleIterator(const std::uintptr_t address) noexcept
      : _header(reinterpret_cast<perf_event_header*>(address))
      , _data(std::uintptr_t(_header + 1U))
    {
    }

    SampleIterator(SampleIterator&& other) noexcept
      : _header(std::exchange(other._header, nullptr))
      , _data(std::exchange(other._data, 0ULL))
    {
    }

    ~SampleIterator() noexcept = default;

    [[nodiscard]] std::optional<Metadata::Mode> mode() const noexcept;
    [[nodiscard]] std::uint16_t size() const noexcept { return _header->size; }

    template<typename T>
    [[nodiscard]] T read() noexcept
    {
      const auto data = *reinterpret_cast<T*>(_data);
      _data += sizeof(T);

      return data;
    }

    template<typename T>
    [[nodiscard]] const T* read(const std::size_t size) noexcept
    {
      auto* begin = reinterpret_cast<T*>(_data);
      _data += sizeof(T) * size;

      return begin;
    }

    template<typename T>
    void skip() noexcept
    {
      _data += sizeof(T);
    }

    template<typename T>
    void skip(const std::size_t size) noexcept
    {
      _data += sizeof(T) * size;
    }

    template<typename T>
    T as() const noexcept
    {
      return reinterpret_cast<T>(_data);
    }

    [[nodiscard]] bool is_sample_event() const noexcept { return _header->type == PERF_RECORD_SAMPLE; }
    [[nodiscard]] bool is_loss_event() const noexcept
    {
#ifndef PERFCPP_NO_RECORD_LOST_SAMPLES /// PERF_RECORD_LOST_SAMPLES is supported since Linux 4.2
      return _header->type == PERF_RECORD_LOST_SAMPLES;
#else
      return false;
#endif
    }
    [[nodiscard]] bool is_context_switch_event() const noexcept
    {
#ifndef PERFCPP_NO_RECORD_SWITCH /// Switch events are supported since Linux 4.3
      return _header->type == PERF_RECORD_SWITCH || _header->type == PERF_RECORD_SWITCH_CPU_WIDE;
#else
      return false;
#endif
    }
    [[nodiscard]] bool is_context_switch_cpu_wide() const noexcept
    {
#ifndef PERFCPP_NO_RECORD_SWITCH /// Switch events are supported since Linux 4.3
      return _header->type == PERF_RECORD_SWITCH_CPU_WIDE;
#else
      return false;
#endif
    }
    [[nodiscard]] bool is_cgroup_event() const noexcept
    {
#ifndef PERFCPP_NO_RECORD_CGROUP /// cgroup events is supported since Linux 5.7
      return _header->type == PERF_RECORD_CGROUP;
#else
      return false;
#endif
    }
    [[nodiscard]] bool is_throttle_event() const noexcept
    {
      return _header->type == PERF_RECORD_THROTTLE || _header->type == PERF_RECORD_UNTHROTTLE;
    }
    [[nodiscard]] bool is_throttle() const noexcept { return _header->type == PERF_RECORD_THROTTLE; }

    [[nodiscard]] bool is_exact_ip() const noexcept { return _header->misc & PERF_RECORD_MISC_EXACT_IP; }
    [[nodiscard]] bool is_context_switch_out() const noexcept
    {
#ifndef PERFCPP_NO_RECORD_SWITCH /// Switch events are supported since Linux 4.3
      return _header->misc & PERF_RECORD_MISC_SWITCH_OUT;
#else
      return false;
#endif
    }
    [[nodiscard]] bool is_context_switch_out_preempt() const noexcept
    {
#ifndef PERFCPP_NO_RECORD_MISC_SWITCH_OUT_PREEMPT /// Preempt flag of switch events is supported since Linux 4.3
      return _header->misc & PERF_RECORD_MISC_SWITCH_OUT_PREEMPT;
#else
      return false;
#endif
    }

  private:
    /// Header of the event.
    perf_event_header* _header;

    /// Data (located directly after the header).
    std::uintptr_t _data;
  };

public:
  SampleDecoder(const CounterDefinition& counter_definition, const Sampler::Values& values)
    : _counter_definition(counter_definition)
    , _sampler_values(values)
  {
  }
  ~SampleDecoder() noexcept = default;

  /**
   * Decodes the samples from the sample buffers and translates them into Samples.
   *
   * @param sample_buffers Buffers holding the raw data that will be decoded and translated.
   * @param has_amd_ibs_op_pmu Flag, indicating if the IBS Op PMU was used.
   * @param has_amd_ibs_fetch_pmu Flag, indicating if the IBS Fetch PMU was used.
   * @param requested_event_set List of requested events.
   * @param count_hardware_counter Number of hardware recorded counters.
   * @return List of decoded samples.
   */
  [[nodiscard]] std::vector<Sample> decode(std::vector<std::vector<std::byte>>&& sample_buffers,
                                           bool has_amd_ibs_op_pmu,
                                           bool has_amd_ibs_fetch_pmu,
                                           const RequestedEventSet& requested_event_set,
                                           std::size_t count_hardware_counter);

private:
  const CounterDefinition& _counter_definition;

  /// Values (i.e., requested fields) passed to the sampler when initializing.
  const Sampler::Values& _sampler_values;

  /**
   * Reads the sample_id struct from the data located at sample_ptr into the provided sample.
   *
   * @param sample Sample to read the data into.
   */
  void read_sample_id_all(SampleIterator& entry, Sample& sample) const noexcept;

  /**
   * Translates the current entry from the user-level buffer into a "normal" sample.
   *
   * @param entry Entry of the user-level buffer.
   * @param has_amd_ibs_op_pmu Flag indicating if the AMD IBS Op PMU is the trigger.
   * @param has_amd_ibs_fetch_pmu Flag, indicating if the AMD IBS Fetch PMU is the trigger.
   * @param requested_event_set Set of requested events
   * @param count_hardware_counter Number of recorded hardware counters.
   * @return Sample.
   */
  [[nodiscard]] perf::Sample read_sample_event(SampleIterator&& entry,
                                               bool has_amd_ibs_op_pmu,
                                               bool has_amd_ibs_fetch_pmu,
                                               const RequestedEventSet& requested_event_set,
                                               std::size_t count_hardware_counter) const;

  /**
   * Reads registers from the current buffer entry.
   *
   * @param entry Current position at the buffer.
   * @return Registers.
   */
  [[nodiscard]] static RegisterValues read_registers(SampleIterator& entry, const Registers& registers);

  /**
   * Reads hardware events from the current buffer entry.
   *
   * @param entry Current position at the buffer.
   * @param requested_event_set Set of requested events
   * @param count_hardware_counter Number of recorded hardware counters.
   * @return Event values
   */
  [[nodiscard]] std::optional<CounterResult> read_hardware_events(SampleIterator& entry,
                                                                  const RequestedEventSet& requested_event_set,
                                                                  std::size_t count_hardware_counter) const;

  /**
   * Reads the callchain from the current buffer entry.
   *
   * @param entry Current position at the buffer.
   * @return List of instruction pointers (the callchain).
   */
  [[nodiscard]] static std::optional<std::vector<std::uintptr_t>> read_callchain(SampleIterator& entry);

  /**
   * Reads the branch stack from the current buffer entry.
   *
   * @param entry Current position at the buffer.
   * @return Branch stack.
   */
  [[nodiscard]] static std::optional<std::vector<Branch>> read_branch_stack(SampleIterator& entry);

  /**
   * Reads the data source field and translates it into an instruction type, the source, snoop information, tlb
   * information, and lock information.
   *
   * @param source Data source field.
   * @return 3-tuple (instruction type, data source, snoop, (is l1 tlb hit bit, is l2 tlb hit bit), is locked bit)
   */
  [[nodiscard]] static std::tuple<std::optional<DataAccess::AccessType>,
                                  DataAccess::Source,
                                  std::optional<DataAccess::Snoop>,
                                  std::optional<std::pair<bool, bool>>,
                                  std::optional<bool>>
  read_data_access_source(std::uint64_t source);

  /**
   * Reads the hardware transaction abort from the current buffer entry.
   *
   * @param entry Current position at the buffer.
   * @return Hardware transaction abort.
   */
  [[nodiscard]] static InstructionExecution::HardwareTransactionAbort read_hardware_transaction_abort(
    std::uint64_t abort);

  /**
   * Enriches the given sample with information that is present in the IBS raw data but cannot be accessed by the perf
   * subsystem interface.
   *
   * @param is_ibs_fetch Flag if the sample PMU is ibs_fetch (ibs_op otherwise).
   * @param sample The sample to enrich; needs to contain raw data.
   */
  void enrich_ibs_sample_from_raw_data(bool is_ibs_fetch, Sample& sample) const noexcept;

  /**
   * Translates the TLB page size in a number of bytes, based on the options.
   *
   * @param is_1g True, if the page is 1GB.
   * @param is_2m True, if the page is 2MB.
   * @return The size in bytes.
   */
  static std::uint64_t calculate_tlb_page_size(bool is_1g, bool is_2m);

  /**
   * Translates the current entry from the user-level buffer into a lost sample.
   *
   * @param entry Entry of the user-level buffer.
   * @return Sample containing the loss.
   */
  [[nodiscard]] perf::Sample read_loss_event(SampleIterator&& entry) const noexcept;

  /**
   * Translates the current entry from the user-level buffer into a context switch sample.
   *
   * @param entry Entry of the user-level buffer.
   * @return Sample containing the context switch.
   */
  [[nodiscard]] perf::Sample read_context_switch_event(SampleIterator&& entry) const noexcept;

  /**
   * Translates the current entry from the user-level buffer into a cgroup sample.
   *
   * @param entry Entry of the user-level buffer.
   * @return Sample containing the cgroup.
   */
  [[nodiscard]] static perf::Sample read_cgroup_event(SampleIterator&& entry);

  /**
   * Translates the current entry from the user-level buffer into a throttle or un-throttle sample.
   *
   * @param entry Entry of the user-level buffer.
   * @return Sample containing the throttle.
   */
  [[nodiscard]] perf::Sample read_throttle_event(SampleIterator&& entry) const noexcept;
};
}