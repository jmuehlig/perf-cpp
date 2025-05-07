#include <algorithm>
#include <perfcpp/exception.h>
#include <perfcpp/hardware_info.h>
#include <perfcpp/ibs_parser.h>
#include <perfcpp/sampler.h>
#include <stdexcept>
#include <utility>

perf::Sampler&
perf::Sampler::trigger(std::vector<std::vector<std::string>>&& list_of_trigger_names)
{
  auto triggers = std::vector<std::vector<Trigger>>{};
  triggers.reserve(list_of_trigger_names.size());

  /// Turn the list of event names in a list of Sampler::Trigger objects and continue processing (checking if the
  /// trigger is an existing event, not a metric, etc.) there.
  for (auto& trigger_names : list_of_trigger_names) {
    auto trigger_with_precision = std::vector<Trigger>{};
    std::transform(trigger_names.begin(),
                   trigger_names.end(),
                   std::back_inserter(trigger_with_precision),
                   [](auto& name) { return Trigger{ std::move(name) }; });
    triggers.push_back(std::move(trigger_with_precision));
  }

  return this->trigger(std::move(triggers));
}

perf::Sampler&
perf::Sampler::trigger(std::vector<std::vector<Trigger>>&& triggers)
{
  /// Deny to modify triggers after the sampler was already opened.
  if (this->_is_opened) {
    throw CannotChangeTriggerWhenSamplerOpenedError{};
  }

  /// Remove all triggers that where added so far.
  this->_triggers.clear();

  /// When no triggers provided, we're done.
  if (triggers.empty()) {
    return *this;
  }

  /// Process all requested triggers.
  this->_triggers.reserve(triggers.size());
  for (auto& trigger_group : triggers) {
    auto trigger_group_references =
      std::vector<std::tuple<std::string_view, std::optional<Precision>, std::optional<PeriodOrFrequency>>>{};
    trigger_group_references.reserve(trigger_group.size());

    for (auto& trigger : trigger_group) {
      /// Reject metrics as trigger events as metrics consist of multiple events.
      if (this->_counter_definitions.is_metric(trigger.name())) {
        throw MetricNotSupportedAsSamplingTriggerError{ trigger.name() };
      }

      /// Read the config (like event id etc.) from every trigger name and verify that the trigger event exists in the
      /// CounterDefinition.
      if (auto counter_config = this->_counter_definitions.counter(trigger.name()); !counter_config.empty()) {
        trigger_group_references.emplace_back(
          std::get<1>(counter_config.front()), trigger.precision(), trigger.period_or_frequency());
      } else {
        throw CannotFindEventError{ trigger.name() };
      }
    }
    this->_triggers.push_back(std::move(trigger_group_references));
  }

  return *this;
}

void
perf::Sampler::open()
{
  /// Do not open again, if the sampler was already opened.
  /// The is_open flag will be reset on closing the sampler.
  if (std::exchange(this->_is_opened, true)) {
    return;
  }

  /// Build the groups from triggers + counters from values.
  for (const auto& trigger_group : this->_triggers) {
    /// Convert the trigger group (list of (event name, configuration attributes)) into "real" sample counters, which is
    /// basically a group of hardware events (one ore multiple triggers and to-recorded hardware events, if requested).
    if (!trigger_group.empty()) {
      /// As each event can be available on different, heterogeneous PMUs, we need to check if a trigger is available on
      /// multiple PMUs and add it multiple times – once per PMU.
      const auto event_name = std::get<0>(trigger_group.front());
      for (const auto& hardware_counter : this->_counter_definitions.counter(event_name)) {
        auto sample_counter = this->transform_trigger_to_sample_counter(std::get<0>(hardware_counter), trigger_group);
        this->_sample_counter.push_back(std::move(sample_counter));
      }
    }
  }

  /// Verify that at least one trigger was configured.
  if (this->_sample_counter.empty()) {
    throw CannotStartEmptySamplerError{};
  }

  /// Check if cgroup is included into sampling – only if supported by the underlying kernel.
#ifndef PERFCPP_NO_RECORD_CGROUP /// Recording cgroup is supported since Linux 5.7
  const auto is_include_cgroup = this->_values.is_set(PERF_SAMPLE_CGROUP);
#else
  const auto is_include_cgroup = false;
#endif

  /// Open the trigger hardware events.
  for (auto& sample_counter : this->_sample_counter) {
    /// Open the group.
    sample_counter.group().open(
      this->_config,
      this->_values.is_set(PERF_SAMPLE_READ),
      sample_counter.has_intel_auxiliary_counter(),
      this->_config.buffer_pages(),
      this->_values.get(),
      this->_values.is_set(PERF_SAMPLE_BRANCH_STACK) ? std::make_optional(this->_values.branch_mask()) : std::nullopt,
      this->_values.is_set(PERF_SAMPLE_REGS_USER) ? std::make_optional(this->_values.user_registers().mask())
                                                  : std::nullopt,
      this->_values.is_set(PERF_SAMPLE_REGS_INTR) ? std::make_optional(this->_values.kernel_registers().mask())
                                                  : std::nullopt,
      this->_values.is_set(PERF_SAMPLE_STACK_USER) ? std::make_optional(this->_values.max_user_stack()) : std::nullopt,
      this->_values.is_set(PERF_SAMPLE_CALLCHAIN) ? std::make_optional(this->_values.max_call_stack()) : std::nullopt,
      this->_values._is_include_context_switch,
      is_include_cgroup);
  }
}

bool
perf::Sampler::start()
{
  /// Open the groups, if not already done.
  this->open();

  /// Enable the counters to start sampling.
  for (const auto& sample_counter : this->_sample_counter) {
    sample_counter.group().enable();
  }

  return true;
}

void
perf::Sampler::stop()
{
  /// Disable the counters.
  for (const auto& sample_counter : this->_sample_counter) {
    sample_counter.group().disable();
  }
}

void
perf::Sampler::close() noexcept
{
  if (std::exchange(this->_is_opened, false)) {
    /// Clear all buffers, groups, and counter names
    /// in order to enable opening again.
    this->_sample_counter.clear();
  }
}

perf::Sampler::SampleCounter
perf::Sampler::transform_trigger_to_sample_counter(
  const std::string_view pmu_name,
  const std::vector<std::tuple<std::string_view, std::optional<Precision>, std::optional<PeriodOrFrequency>>>&
    trigger_group) const
{
  /// Group of hardware events.
  auto group = Group{};

  /// List of counter names that should be read later from results.
  auto requested_events = RequestedEventSet{};

  /// Check if the auxiliary counter is needed and needs to be added.
  const auto [is_auxiliary_event_needed, is_auxiliary_event_included] =
    this->is_auxiliary_event_needed_and_already_included(pmu_name, trigger_group);

  /// If the auxiliary counter is needed but not included, add it.
  if (is_auxiliary_event_needed && !is_auxiliary_event_included) {
    if (auto auxiliary_event = this->_counter_definitions.counter(pmu_name, "mem-loads-aux");
        auxiliary_event.has_value()) {

      /// Read the counter config (like event id, etc.).
      auto auxiliary_counter_config = std::get<2>(auxiliary_event.value());

      /// The auxiliary event needs constant skid.
      auxiliary_counter_config.precise_ip(Precision::MustHaveConstantSkid);

      /// Set the counters period or frequency equal to the first trigger (or fall back to config if not configured).
      auto period_or_frequency = std::get<2>(trigger_group.front());
      auxiliary_counter_config.period_or_frequency(period_or_frequency.value_or(this->_config.period_for_frequency()));

      /// Add the counter to the group.
      group.add(auxiliary_counter_config);
    } else {
      throw AuxiliaryEventForSamplingNotFoundError{};
    }
  }

  /// Add the trigger(s) to the group. For the most time, this will be a single trigger.
  for (const auto& trigger : trigger_group) {
    const auto [event_name, precision, period_or_frequency] = trigger;
    if (auto counter_name_and_config = this->_counter_definitions.counter(pmu_name, event_name);
        counter_name_and_config.has_value()) {

      /// Read the counter config (like event id, etc.).
      auto counter_config = std::get<2>(counter_name_and_config.value());

      /// Set the counters precise_ip (fall back to config if empty).
      counter_config.precise_ip(static_cast<std::uint8_t>(precision.value_or(this->_config.precise_ip())));

      /// Set the counters period or frequency (fall back to config if empty).
      counter_config.period_or_frequency(period_or_frequency.value_or(this->_config.period_for_frequency()));

      /// Add the counter to the group.
      group.add(counter_config);

      /// Notice the counter name of the trigger event.
      if (this->_values.is_set(PERF_SAMPLE_READ)) {
        requested_events.add(pmu_name, event_name, 0U);
      }
    } else {
      throw CannotFindEventError{ pmu_name, event_name };
    }
  }

  /// Add possible counters as value to the sample.
  if (this->_values.is_set(PERF_SAMPLE_READ)) {
    for (const auto& event_name : this->_values.counters()) {

      /// Check if the event is a true hardware counter – if so, just add it to the list.
      if (auto counter_config = this->_counter_definitions.counter(pmu_name, event_name); counter_config.has_value()) {
        /// Add the event to the requested event set.
        /// If the request returns true, the event as indeed added and needs to be added to the group.
        const auto is_added =
          requested_events.add(pmu_name, std::get<1>(counter_config.value()), std::uint8_t(group.size()));
        if (is_added) {
          group.add(std::get<2>(counter_config.value()));
        }
      }

      /// Otherwise, check if the event is a metric. In that case, add all depending hardware counters (if not already
      /// done).
      else if (auto metric = this->_counter_definitions.metric(event_name); metric.has_value()) {
        const auto metric_name = std::get<0>(metric.value());
        /// For metrics, we need to add every hardware counter the metric depends on (and check their existence).
        for (const auto& depending_counter_name : std::get<1>(metric.value()).required_counter_names()) {
          if (auto depending_counter_config = this->_counter_definitions.counter(pmu_name, depending_counter_name);
              depending_counter_config.has_value()) {

            /// Add the event to the requested event set.
            /// If the request returns true, the event is indeed added and needs to be added to the group.
            const auto is_added =
              requested_events.add(pmu_name, std::get<0>(depending_counter_config.value()), std::uint8_t(group.size()));
            if (is_added) {
              group.add(std::get<2>(depending_counter_config.value()));
            }
          } else if (this->_counter_definitions.is_time_event(depending_counter_name)) {
            throw TimeEventNotSupportedForSamplingError{ event_name };
          } else {
            throw CannotFindEventForMetricError{ depending_counter_name, metric_name };
          }
        }

        /// Add the metric to the list of scheduled events.
        requested_events.add(metric_name, RequestedEvent::Type::Metric, true);
      }

      /// Otherwise, check if the event is a time event. Time events are not supported for sampling; let the user know.
      else if (this->_counter_definitions.is_time_event(event_name)) {
        throw TimeEventNotSupportedForSamplingError{ event_name };
      }

      /// Throw an exception of the event is neither a hardware event or a metric.
      else {
        throw CannotFindEventOrMetricError{ event_name };
      }
    }

    if (!requested_events.empty()) {
      return SampleCounter{ std::move(group),
                            std::move(requested_events),
                            is_auxiliary_event_needed,
                            pmu_name == "ibs_fetch",
                            pmu_name == "ibs_op" };
    }
  }

  return SampleCounter{ std::move(group), is_auxiliary_event_needed, pmu_name == "ibs_fetch", pmu_name == "ibs_op" };
}

std::pair<bool, bool>
perf::Sampler::is_auxiliary_event_needed_and_already_included(
  std::string_view pmu_name,
  const std::vector<std::tuple<std::string_view, std::optional<Precision>, std::optional<PeriodOrFrequency>>>&
    trigger_group) const
{
  if (HardwareInfo::is_intel_aux_counter_required()) {
    if (const auto mem_loads_event = this->_counter_definitions.counter(pmu_name, std::string_view{ "mem-loads" });
        mem_loads_event.has_value()) {
      if (const auto mem_loads_aux_event =
            this->_counter_definitions.counter(pmu_name, std::string_view{ "mem-loads-aux" });
          mem_loads_aux_event.has_value()) {

        /// Check if any of the other triggers is a mem-loads event.
        for (const auto& trigger : trigger_group) {
          if (auto trigger_event = this->_counter_definitions.counter(pmu_name, std::get<0>(trigger));
              trigger_event.has_value()) {
            const auto is_mem_loads_event = std::get<2>(trigger_event.value()) == std::get<2>(mem_loads_event.value());

            /// If there is a mem-loads event we need the auxiliary counter.
            if (is_mem_loads_event) {

              /// Check if the first counter in the group is already the mem-loads-aux event.
              if (const auto leading_trigger_event =
                    this->_counter_definitions.counter(pmu_name, std::get<0>(trigger_group.front()));
                  leading_trigger_event.has_value()) {
                const auto has_mem_loads_aux_event =
                  std::get<2>(leading_trigger_event.value()) == std::get<2>(mem_loads_aux_event.value());
                return std::make_pair(true, has_mem_loads_aux_event);
              }

              /// The auxiliary counter is needed, but not included.
              return std::make_pair(true, false);
            }
          }
        }
      }
    }
  }

  return std::make_pair(false, false);
}

std::vector<perf::Sample>
perf::Sampler::result(const bool sort_by_time) const
{
  auto result = std::vector<Sample>{};
  result.reserve(2048U);

  for (const auto& sample_counter : this->_sample_counter) {

    /// Get all buffers: the current mmap-ed ringbuffer and the application-level buffers used to copy the ringbuffer to
    const auto buffer_ranges = sample_counter.group().sample_buffer_ranges();

    /// Read samples from all the buffers (mmap-ed perf buffer and application-level buffers).
    for (const auto& [start, end] : buffer_ranges) {
      auto iterator = start;

      /// Scan over all samples stored in the user-level buffer.
      while (iterator < end) {
        auto entry = SampleBuffer::Entry{ iterator };
        const auto size = entry.size();

        if (size == 0ULL) {
          break;
        }

        if (entry.is_sample_event()) { /// Read "normal" samples.
          result.push_back(this->read_sample_event(std::move(entry), sample_counter));
        } else if (entry.is_loss_event()) { /// Read lost samples.
          result.push_back(this->read_loss_event(std::move(entry)));
        } else if (entry.is_context_switch_event()) { /// Read context switch.
          result.push_back(this->read_context_switch_event(std::move(entry)));
        } else if (entry.is_cgroup_event()) { /// Read cgroup samples.
          result.push_back(Sampler::read_cgroup_event(std::move(entry)));
        } else if (entry.is_throttle_event() && this->_values._is_include_throttle) { /// Read (un-) throttle samples.
          result.push_back(this->read_throttle_event(std::move(entry)));
        }

        /// Go to the next sample.
        iterator += size;
      }
    }
  }

  /// Sort the samples if requested and we can sort by time.
  if (this->_values.is_set(PERF_SAMPLE_TIME) && sort_by_time) {
    std::sort(result.begin(), result.end(), SampleTimestampComparator{});
  }

  return result;
}

void
perf::Sampler::read_sample_id_all(perf::SampleBuffer::Entry& entry, Sample& sample) const noexcept
{
  if (this->_values.is_set(PERF_SAMPLE_TID)) {
    sample.metadata().process_id(entry.read<std::uint32_t>());
    sample.metadata().thread_id(entry.read<std::uint32_t>());
  }

  if (this->_values.is_set(PERF_SAMPLE_TIME)) {
    sample.metadata().timestamp(entry.read<std::uint64_t>());
  }

  if (this->_values.is_set(PERF_SAMPLE_STREAM_ID)) {
    sample.metadata().stream_id(entry.read<std::uint64_t>());
  }

  if (this->_values.is_set(PERF_SAMPLE_CPU)) {
    sample.metadata().cpu_id(entry.read<std::uint32_t>());
    entry.skip<std::uint32_t>(); /// Skip "res" field.
  }

  if (this->_values.is_set(PERF_SAMPLE_IDENTIFIER)) {
    sample.metadata().sample_id(entry.read<std::uint64_t>());
  }
}

perf::Sample
perf::Sampler::read_sample_event(perf::SampleBuffer::Entry entry, const SampleCounter& sample_counter) const
{
  auto sample = Sample{};
  sample.metadata().mode(entry.mode());
  sample.instruction_execution().is_instruction_pointer_exact(entry.is_exact_ip());

  if (this->_values.is_set(PERF_SAMPLE_IDENTIFIER)) {
    sample.metadata().sample_id(entry.read<std::uint64_t>());
  }

  if (this->_values.is_set(PERF_SAMPLE_IP)) {
    sample.instruction_execution().logical_instruction_pointer(entry.read<std::uintptr_t>());
  }

  if (this->_values.is_set(PERF_SAMPLE_TID)) {
    sample.metadata().process_id(entry.read<std::uint32_t>());
    sample.metadata().thread_id(entry.read<std::uint32_t>());
  }

  if (this->_values.is_set(PERF_SAMPLE_TIME)) {
    sample.metadata().timestamp(entry.read<std::uint64_t>());
  }

  if (this->_values.is_set(PERF_SAMPLE_STREAM_ID)) {
    sample.metadata().stream_id(entry.read<std::uint64_t>());
  }

  if (this->_values.is_set(PERF_SAMPLE_ADDR)) {
    sample.data_access().logical_memory_address(entry.read<std::uint64_t>());
  }

  if (this->_values.is_set(PERF_SAMPLE_CPU)) {
    sample.metadata().cpu_id(entry.read<std::uint32_t>());
    entry.skip<std::uint32_t>(); /// Skip "res".
  }

  if (this->_values.is_set(PERF_SAMPLE_PERIOD)) {
    sample.metadata().period(entry.read<std::uint64_t>());
  }

  if (this->_values.is_set(PERF_SAMPLE_READ)) {
    auto counter_result = Sampler::read_hardware_events(entry, sample_counter);
    if (counter_result.has_value()) {
      sample.counter_result(std::move(counter_result.value()));
    }
  }

  if (this->_values.is_set(PERF_SAMPLE_CALLCHAIN)) {
    auto callchain = Sampler::read_callchain(entry);
    if (callchain.has_value()) {
      sample.instruction_execution().callchain(std::move(callchain.value()));
    }
  }

  if (this->_values.is_set(PERF_SAMPLE_RAW)) {
    /// Read the size of the raw sample.
    const auto raw_data_size = entry.read<std::uint32_t>();

    /// Read the raw data.
    const auto* raw_sample_data = entry.read<std::byte>(raw_data_size);
    sample.raw(std::vector<std::byte>{ raw_sample_data, raw_sample_data + raw_data_size });
  }

  if (this->_values.is_set(PERF_SAMPLE_BRANCH_STACK)) {
    auto branch_stack = Sampler::read_branch_stack(entry);
    if (branch_stack.has_value()) {
      sample.branch_stack(std::move(branch_stack.value()));
    }
  }

  if (this->_values.is_set(PERF_SAMPLE_REGS_USER)) {
    sample.user_registers(Sampler::read_registers(entry, this->_values._user_registers));
  }

  if (this->_values.is_set(PERF_SAMPLE_STACK_USER)) {
    const auto size = entry.read<std::uint64_t>();
    const auto* stack_data = entry.read<std::byte>(size);
    const auto dyn_size = size > 0ULL ? entry.read<std::uint64_t>() : 0ULL;

    sample.user_stack(std::vector<std::byte>{ stack_data, stack_data + dyn_size });
  }

  if (this->_values.is_set(PERF_SAMPLE_WEIGHT)) {
    const auto weight = static_cast<std::uint32_t>(entry.read<std::uint64_t>());
    if (HardwareInfo::is_intel()) {
      /// Intel reports the instruction latency before th 12th generation–and cache access latency from that.
      if (HardwareInfo::is_intel_12th_generation_or_newer()) {
        sample.data_access().latency().data_access(weight);
      } else {
        sample.instruction_execution().latency().instruction_retirement(weight);
      }
    } else if (HardwareInfo::is_amd()) {
      /// AMD reports the cache miss latency; i.e., no latency for L1d hits is reported.
      sample.data_access().latency().cache_miss(weight);
    }
  }

#ifndef PERFCPP_NO_SAMPLE_WEIGHT_STRUCT /// Sampling of weight structs (in contrast to simple weight) is supported since
                                        /// Linux 5.12
  if (this->_values.is_set(PERF_SAMPLE_WEIGHT_STRUCT)) {
    const auto weight = entry.read<perf_sample_weight>();

    /// Parse the weight into latency information, depending on the underlying hardware.
    if (HardwareInfo::is_intel()) {
      if (HardwareInfo::is_intel_12th_generation_or_newer()) {
        sample.data_access().latency().data_access(weight.var1_dw);
        sample.instruction_execution().latency().instruction_retirement(weight.var2_w);
      } else {
        sample.instruction_execution().latency().instruction_retirement(weight.var1_dw);
      }
    } else if (HardwareInfo::is_amd()) {
      /// See https://github.com/torvalds/linux/blob/master/arch/x86/events/amd/ibs.c#L1119
      sample.data_access().latency().cache_miss(weight.var1_dw);
      sample.instruction_execution().latency().uop_tag_to_retirement(weight.var2_w);
    }
  }
#endif

  if (this->_values.is_set(PERF_SAMPLE_DATA_SRC)) {
    /// Read source value from perf.
    const auto [instruction_type, data_source, tlb, is_locked] =
      Sampler::read_data_access_source(entry.read<std::uint64_t>());

    /// Set memory instruction type, if not already set.
    if (!sample.instruction_execution().type().has_value() && instruction_type.has_value()) {
      sample.instruction_execution().type(instruction_type.value());
    }

    /// Set data source.
    sample.data_access().source(data_source);

    /// Set TLB hit.
    if (tlb.has_value()) {
      sample.data_access().tlb().is_l1_hit(std::get<0>(tlb.value()));
      sample.data_access().tlb().is_l2_hit(std::get<1>(tlb.value()));
    }

    /// Set is_locked information.
    if (is_locked.has_value()) {
      sample.instruction_execution().is_locked(is_locked.value());
    }
  }

  if (this->_values.is_set(PERF_SAMPLE_TRANSACTION)) {
    sample.instruction_execution().hardware_transaction_abort(
      Sampler::read_hardware_transaction_abort(entry.read<std::uint64_t>()));
  }

  if (this->_values.is_set(PERF_SAMPLE_REGS_INTR)) {
    sample.kernel_registers(Sampler::read_registers(entry, this->_values._kernel_registers));
  }

#ifndef PERFCPP_NO_SAMPLE_PHYS_ADDR /// Sampling for physical memory address is supported since Linux 4.13
  if (this->_values.is_set(PERF_SAMPLE_PHYS_ADDR)) {
    sample.data_access().physical_memory_address(entry.read<std::uint64_t>());
  }
#endif

#ifndef PERFCPP_NO_SAMPLE_CGROUP /// Sampling cgroup is supported since Linux 5.7
  if (this->_values.is_set(PERF_SAMPLE_CGROUP)) {
    sample.cgroup_id(entry.read<std::uint64_t>());
  }
#endif

#ifndef PERFCPP_NO_SAMPLE_DATA_PAGE_SIZE /// Sampling the data page size is supported since Linux 5.11
  if (this->_values.is_set(PERF_SAMPLE_DATA_PAGE_SIZE)) {
    sample.data_access().page_size(entry.read<std::uint64_t>());
  }
#endif

#ifndef PERFCPP_NO_SAMPLE_CODE_PAGE_SIZE /// Sampling the code page size is supported since Linux 5.11
  if (this->_values.is_set(PERF_SAMPLE_CODE_PAGE_SIZE)) {
    sample.instruction_execution().page_size(entry.read<std::uint64_t>());
  }
#endif

  /// Enrich AMD IBS samples with information that is not accessible through the perf_event_open interface by
  /// interpreting the raw data, if enabled.
  if (this->_values.is_set(PERF_SAMPLE_RAW) && sample.raw().has_value() && HardwareInfo::is_amd() &&
      (sample_counter.has_amd_op_pmu_counter() || sample_counter.has_amd_fetch_pmu_counter())) {
    Sampler::enrich_ibs_sample_from_raw_data(sample_counter.has_amd_fetch_pmu_counter(), sample);
  }

  return sample;
}

perf::RegisterValues
perf::Sampler::read_registers(SampleBuffer::Entry& entry, const Registers& registers)
{
  /// Read the register ABI.
  const auto abi = static_cast<ABI>(entry.read<std::uint64_t>());

  if (registers.empty()) {
    return RegisterValues{ abi };
  }

  const auto count_registers = registers.size();

  /// Map holding all register values.
  auto register_values = std::unordered_map<std::uint8_t, std::int64_t>{};
  register_values.reserve(count_registers);

  /// Read raw register values from perf data.
  const auto* perf_registers = entry.read<std::int64_t>(count_registers);

  /// Transform raw perf register array into register value map by linking values to specified registers. Note that
  /// registers can be a vector of x86, arm, arm64, etc.
  std::visit(
    [count_registers, perf_registers, &register_values](const auto& specified_registers) {
      for (auto register_id = 0U; register_id < count_registers; ++register_id) {
        register_values.insert(
          std::make_pair(static_cast<std::uint8_t>(specified_registers[register_id]), perf_registers[register_id]));
      }
    },
    registers.registers());

  return RegisterValues{ abi, std::move(register_values) };
}

std::optional<perf::CounterResult>
perf::Sampler::read_hardware_events(perf::SampleBuffer::Entry& entry, const SampleCounter& sample_counter) const
{
  /// Read the number of counters.
  const auto count_counter_values = entry.read<decltype(CounterValues<Group::MAX_MEMBERS>::count_members)>();

  /// Time enabled and running for correction.
  const auto time_enabled = entry.read<decltype(CounterValues<Group::MAX_MEMBERS>::time_enabled)>();
  const auto time_running = entry.read<decltype(CounterValues<Group::MAX_MEMBERS>::time_running)>();
  const auto multiplexing_correction = time_running > 0ULL ? double(time_enabled) / double(time_running) : 1.;

  /// Read the counters (if the number matches the number of specified counters).
  auto* counter_values = entry.read<CounterValues<Group::MAX_MEMBERS>::value>(count_counter_values);
  if (count_counter_values != sample_counter.group().size()) {
    return std::nullopt;
  }

  /// Create a list of results with only hardware events – regardless of their visibility in the result. This list will
  /// be used to build a result containing visible events and metrics.
  auto hardware_counter_results = std::vector<std::pair<std::string_view, double>>{};
  hardware_counter_results.reserve(sample_counter.group().size());
  for (const auto& requested_event : sample_counter.requested_events()) {
    if (requested_event.is_hardware_event()) {
      const auto counter_index = requested_event.scheduled_group()->position();
      /// Counter value (corrected).
      const auto counter_result = double(counter_values[counter_index].value) * multiplexing_correction;
      hardware_counter_results.emplace_back(requested_event.event_name(), counter_result);
    }
  }

  /// Build a result containing metrics and hardware events requested by teh user.
  return sample_counter.requested_events().result(
    this->_counter_definitions, CounterResult{ std::move(hardware_counter_results) }, 1ULL);
}

std::optional<std::vector<std::uintptr_t>>
perf::Sampler::read_callchain(perf::SampleBuffer::Entry& entry)
{
  /// Read the size of the callchain.
  const auto callchain_size = entry.read<std::uint64_t>();

  if (callchain_size == 0U) {
    return std::nullopt;
  }

  auto callchain = std::vector<std::uintptr_t>{};
  callchain.reserve(callchain_size);

  /// Read the callchain entries.
  auto* instruction_pointers = entry.read<std::uint64_t>(callchain_size);
  for (auto index = 0U; index < callchain_size; ++index) {
    callchain.push_back(std::uintptr_t{ instruction_pointers[index] });
  }

  return callchain;
}

std::optional<std::vector<perf::Branch>>
perf::Sampler::read_branch_stack(perf::SampleBuffer::Entry& entry)
{
  /// Read the size of the branch stack.
  const auto count_branches = entry.read<std::uint64_t>();

  if (count_branches == 0U) {
    return std::nullopt;
  }

  auto branches = std::vector<Branch>{};
  branches.reserve(count_branches);

  /// Read the branch stack entries.
  auto* sampled_branches = entry.read<perf_branch_entry>(count_branches);
  for (auto i = 0U; i < count_branches; ++i) {
    const auto& branch = sampled_branches[i];
#ifndef PERFCPP_NO_BRANCH_STACK_CYCLES /// Cycles in branch stacks is supported since Linux 4.3
    const auto cycles = branch.cycles;
#else
    const auto cycles = 0ULL;
#endif
    branches.emplace_back(branch.from, branch.to, branch.mispred, branch.predicted, branch.in_tx, branch.abort, cycles);
  }

  return branches;
}

std::tuple<std::optional<perf::InstructionExecution::InstructionType>,
           perf::DataAccess::Source,
           std::optional<std::pair<bool, bool>>,
           std::optional<bool>>
perf::Sampler::read_data_access_source(const std::uint64_t source)
{
  /// Read memory instruction type.
  const auto mem_operation = perf_mem_data_src{ source }.mem_op;
  auto memory_instruction_type = std::optional<InstructionExecution::InstructionType>{ std::nullopt };
  if (mem_operation & PERF_MEM_OP_LOAD) {
    memory_instruction_type = InstructionExecution::InstructionType::MemoryLoad;
  } else if (mem_operation & PERF_MEM_OP_STORE) {
    memory_instruction_type = InstructionExecution::InstructionType::MemoryStore;
  } else if (mem_operation & PERF_MEM_OP_PFETCH) {
    memory_instruction_type = InstructionExecution::InstructionType::SoftwarePrefetch;
  }

  /// Translate into Source object.
  auto data_access_source = DataAccess::Source{};

  /// Cache or RAM hit.
#ifndef PERFCPP_NO_MEM_LVLNUM /// lvl_num field is supported since Linux 6.1
  const auto perf_lvl_num = perf_mem_data_src{ source }.mem_lvl_num;
  data_access_source.is_l1d_hit(perf_lvl_num == PERF_MEM_LVLNUM_L1);
  data_access_source.is_l2_hit(perf_lvl_num == PERF_MEM_LVLNUM_L2);
  data_access_source.is_l3_hit(perf_lvl_num == PERF_MEM_LVLNUM_L3);
  data_access_source.is_l4_hit(perf_lvl_num == PERF_MEM_LVLNUM_L4);
  data_access_source.is_memory_hit(perf_lvl_num == PERF_MEM_LVLNUM_RAM);
  data_access_source.is_mhb_hit(perf_lvl_num == PERF_MEM_LVLNUM_LFB);
  data_access_source.is_uncachable_memory(perf_lvl_num == PERF_MEM_LVLNUM_UNC);
#else /// Use lvl before Linux 6.1
  const auto perf_lvl = perf_mem_data_src{ source }.mem_lvl;
  data_access_source.is_l1d_hit((perf_lvl & PERF_MEM_LVL_L1) && (perf_lvl & PERF_MEM_LVL_HIT));
  data_access_source.is_l2_hit((perf_lvl & PERF_MEM_LVL_L2) && (perf_lvl & PERF_MEM_LVL_HIT));
  data_access_source.is_l3_hit((perf_lvl & PERF_MEM_LVL_L3) && (perf_lvl & PERF_MEM_LVL_HIT));
  data_access_source.is_memory_hit((perf_lvl & PERF_MEM_LVL_LOC_RAM) || (perf_lvl & PERF_MEM_LVL_REM_RAM1) ||
                                   (perf_lvl & PERF_MEM_LVL_REM_RAM2));
  data_access_source.is_mhb_hit((perf_lvl & PERF_MEM_LVL_LFB) && (perf_lvl & PERF_MEM_LVL_HIT));
  data_access_source.is_uncachable_memory(perf_lvl & PERF_MEM_LVL_UNC);
#endif

  /// Remote.
#ifndef PERFCPP_NO_MEM_REMOTE // Remote field is supported since Linux 4.14
  const auto remote = perf_mem_data_src{ source }.mem_remote;
  data_access_source.is_remote(remote & PERF_MEM_REMOTE_REMOTE);
#else                         /// Use lvl before Linux 4.14
#ifndef PERFCPP_NO_MEM_LVLNUM /// If PERFCPP_NO_MEM_LVLNUM is defined, we do not need to create another perf_lvl object.
  const auto perf_lvl = perf_mem_data_src{ source }.mem_lvl;
#endif
  data_access_source.is_remote((perf_lvl & PERF_MEM_LVL_REM_RAM1) || (perf_lvl & PERF_MEM_LVL_REM_RAM2) ||
                               (perf_lvl & PERF_MEM_LVL_REM_CCE1) || (perf_lvl & PERF_MEM_LVL_REM_CCE2));
#endif

  /// Remote hops.
  if (data_access_source.is_remote()) {
    auto hops = std::optional<std::uint8_t>{ std::nullopt };
#ifndef PERFCPP_NO_MEM_HOPS_0 /// Remote Hops were introduced in Linux 5.16
    const auto perf_hops = perf_mem_data_src{ source }.mem_hops;
    if (perf_hops == PERF_MEM_HOPS_0) {
      hops = 0U;
    }

#ifndef PERFCPP_NO_MEM_HOPS_1_3 /// Remote Hops 1-3 were introduced in Linux 5.17
    if (perf_hops == PERF_MEM_HOPS_1) {
      hops = 1U;
    } else if (perf_hops == PERF_MEM_HOPS_2) {
      hops = 2U;
    } else if (perf_hops == PERF_MEM_HOPS_3) {
      hops = 3U;
    }
#else /// Use LVL_REM before 5.17
    const auto perf_lvl = perf_mem_data_src{ source }.mem_lvl;
    if ((perf_lvl & PERF_MEM_LVL_REM_RAM1) || (perf_lvl & PERF_MEM_LVL_REM_CCE1)) {
      hops = 1U;
    } else if ((perf_lvl & PERF_MEM_LVL_REM_RAM2) || (perf_lvl & PERF_MEM_LVL_REM_CCE2)) {
      hops = 2U;
    }
#endif
#else /// Use LVL_REM before 5.16
    const auto perf_lvl = perf_mem_data_src{ source }.mem_lvl;
    if ((perf_lvl & PERF_MEM_LVL_REM_RAM1) || (perf_lvl & PERF_MEM_LVL_REM_CCE1)) {
      hops = 1U;
    } else if ((perf_lvl & PERF_MEM_LVL_REM_RAM2) || (perf_lvl & PERF_MEM_LVL_REM_CCE2)) {
      hops = 2U;
    }
#endif

    if (hops.has_value()) {
      data_access_source.remote_hops(hops.value());
    }
  }

  /// TLB.
  const auto perf_tlb = perf_mem_data_src{ source }.mem_dtlb;
  auto tlb = std::optional<std::pair<bool, bool>>{ std::nullopt };
  if (!(perf_tlb & PERF_MEM_TLB_NA)) {
    const auto is_l1_tbl_hit = (perf_tlb & PERF_MEM_TLB_L1) && (perf_tlb & PERF_MEM_TLB_HIT);
    const auto is_l2_tbl_hit = (perf_tlb & PERF_MEM_TLB_L2) && (perf_tlb & PERF_MEM_TLB_HIT);
    tlb = std::make_pair(is_l1_tbl_hit, is_l2_tbl_hit);
  }

  /// Locked.
  const auto perf_lock = perf_mem_data_src{ source }.mem_lock;
  auto is_locked = std::optional<bool>{ std::nullopt };
  if (!(perf_lock & PERF_MEM_LOCK_NA)) {
    is_locked = perf_lock & PERF_MEM_LOCK_LOCKED;
  }

  return std::make_tuple(memory_instruction_type, data_access_source, tlb, is_locked);
}

perf::InstructionExecution::HardwareTransactionAbort
perf::Sampler::read_hardware_transaction_abort(const std::uint64_t abort)
{
  /// Translate into the abort object.
  auto hardware_transaction_abort = InstructionExecution::HardwareTransactionAbort{};
  hardware_transaction_abort.is_elision_transaction(abort & PERF_TXN_ELISION);
  hardware_transaction_abort.is_generic_transaction(abort & PERF_TXN_TRANSACTION);
  hardware_transaction_abort.is_synchronous_abort(abort & PERF_TXN_SYNC);
  hardware_transaction_abort.is_retryable(abort & PERF_TXN_RETRY);
  hardware_transaction_abort.is_abort_due_to_memory_conflict(abort & PERF_TXN_CONFLICT);
  hardware_transaction_abort.is_abort_due_to_write_capacity_conflict(abort & PERF_TXN_CAPACITY_WRITE);
  hardware_transaction_abort.is_abort_due_to_read_capacity_conflict(abort & PERF_TXN_CAPACITY_READ);
  hardware_transaction_abort.user_specified_code((abort >> PERF_TXN_ABORT_SHIFT) & PERF_TXN_ABORT_MASK);

  return hardware_transaction_abort;
}

void
perf::Sampler::enrich_ibs_sample_from_raw_data(const bool is_ibs_fetch, perf::Sample& sample)
{
  /// Fetch events...
  if (is_ibs_fetch) {
    auto fetch_parser = IBSFetchParser{ sample.raw().value() };

    /// Fetch latency.
    sample.instruction_execution().latency().fetch(fetch_parser.latency());

    /// Fetch information.
    sample.instruction_execution().fetch(
      InstructionExecution::Fetch{ fetch_parser.is_valid(), fetch_parser.is_complete() });

    /// Instruction cache.
    sample.instruction_execution().cache(InstructionExecution::Cache{
      fetch_parser.is_instruction_cache_miss(), fetch_parser.is_l2_miss(), fetch_parser.is_l3_miss() });

    /// Instruction TLB.
    auto l1_tlb_size = std::optional<std::uint64_t>{ std::nullopt };
    if (fetch_parser.is_physical_instruction_address_valid()) {
      if (fetch_parser.l1_tlb_page_size() == 0U) {
        l1_tlb_size = 4ULL * 1024ULL;
      } else if (fetch_parser.l1_tlb_page_size() == 1U) {
        l1_tlb_size = 2ULL * 1024ULL * 1024ULL;
      } else if (fetch_parser.l1_tlb_page_size() == 2U) {
        l1_tlb_size = 1024ULL * 1024ULL * 1024ULL;
      }
    }
    sample.instruction_execution().tlb(
      InstructionExecution::TLB{ fetch_parser.is_l1_tlb_miss(), l1_tlb_size, fetch_parser.is_l2_tlb_miss() });

    /// Physical instruction address.
    sample.instruction_execution().physical_instruction_pointer(fetch_parser.physical_instruction_address());
  }

  /// .. or execution events.
  else {
    auto execution_parser = IBSExecutionParser{ sample.raw().value() };

    /// Execution latency.
    sample.instruction_execution().latency().uop_completion_to_retirement(
      execution_parser.completion_to_retire_latency());
    sample.instruction_execution().latency().uop_tag_to_retirement(execution_parser.tag_to_retire_latency());

    /// TLB latency.
    sample.data_access().latency().dtlb_refill(execution_parser.tlb_refill_latency());

    /// TLB page size.
    if (!execution_parser.is_l1_data_tlb_miss()) {
      if (execution_parser.is_l1_data_tlb_hit_1g()) {
        sample.data_access().tlb().l1_page_size(1024ULL * 1024ULL * 1024ULL);
      } else if (execution_parser.is_l1_data_tlb_hit_2m()) {
        sample.data_access().tlb().l1_page_size(1024ULL * 1024ULL * 2ULL);
      } else {
        sample.data_access().tlb().l1_page_size(1024ULL * 4ULL);
      }
    }
    if (!execution_parser.is_l2_data_tlb_miss()) {
      if (execution_parser.is_l2_data_tlb_hit_1g()) {
        sample.data_access().tlb().l2_page_size(1024ULL * 1024ULL * 1024ULL);
      } else if (execution_parser.is_l2_data_tlb_hit_2m()) {
        sample.data_access().tlb().l2_page_size(1024ULL * 1024ULL * 2ULL);
      } else {
        sample.data_access().tlb().l2_page_size(1024ULL * 4ULL);
      }
    }

    /// Type of the instruction (prefetch, return, or branch) and type of the branch–if it is one.
    if (!sample.instruction_execution().type().has_value()) {
      if (execution_parser.is_software_prefetch()) {
        sample.instruction_execution().type(InstructionExecution::InstructionType::SoftwarePrefetch);
      } else if (execution_parser.is_return_operation()) {
        sample.instruction_execution().type(InstructionExecution::InstructionType::Return);
      } else if (execution_parser.is_branch_taken_operation() || execution_parser.is_branch_mispredicted_operation() ||
                 execution_parser.is_branch_retired_operation() || execution_parser.is_branch_fuse()) {
        sample.instruction_execution().type(InstructionExecution::InstructionType::Branch);

        /// If the instruction is a branch, set the branch type.
        if (execution_parser.is_branch_taken_operation()) {
          sample.instruction_execution().branch_type(InstructionExecution::BranchType::Taken);
        } else if (execution_parser.is_branch_mispredicted_operation()) {
          sample.instruction_execution().branch_type(InstructionExecution::BranchType::Mispredicted);
        } else if (execution_parser.is_branch_retired_operation()) {
          sample.instruction_execution().branch_type(InstructionExecution::BranchType::Retired);
        } else if (execution_parser.is_branch_fuse()) {
          sample.instruction_execution().branch_type(InstructionExecution::BranchType::Fuse);
        }
      }
    }

    /// Source information.
    if (sample.data_access().source().has_value()) {
      sample.data_access().source()->num_mhb_slots_allocated(execution_parser.num_open_mem_requests());
      sample.data_access().source()->is_mhb_hit(execution_parser.is_data_cache_miss_no_mab_allocation());
      sample.data_access().source()->is_write_combine_memory(execution_parser.is_data_cache_write_combine_access());
      sample.data_access().is_misalign_penalty(execution_parser.is_data_cache_misaligned_access());

      /// Translate memory width into number of bytes.
      if (const auto access_width = execution_parser.access_mem_width(); access_width > 0U && access_width <= 7U) {
        sample.data_access().access_width(std::uint8_t(1U << (access_width - 1U)));
      }
    }
  }
}

perf::Sample
perf::Sampler::read_loss_event(perf::SampleBuffer::Entry&& entry) const noexcept
{
  auto sample = Sample{};
  sample.metadata().mode(entry.mode());

  /// Read the loss.
  sample.count_loss(entry.read<std::uint64_t>());

  /// Read sample_id.
  this->read_sample_id_all(entry, sample);

  return sample;
}

perf::Sample
perf::Sampler::read_context_switch_event(perf::SampleBuffer::Entry&& entry) const noexcept
{
  auto sample = Sample{};
  sample.metadata().mode(entry.mode());

  const auto is_switch_out = entry.is_context_switch_out();
  const auto is_switch_out_preempt = entry.is_context_switch_out_preempt();

  std::optional<std::uint32_t> process_id{ std::nullopt };
  std::optional<std::uint32_t> thread_id{ std::nullopt };

  /// CPU wide context switches contain the process and thread ids.
  if (entry.is_context_switch_cpu_wide()) {
    process_id = entry.read<std::uint32_t>();
    thread_id = entry.read<std::uint32_t>();
  }

  /// Read sample_id.
  this->read_sample_id_all(entry, sample);

  sample.context_switch(ContextSwitch{ is_switch_out, is_switch_out_preempt, process_id, thread_id });

  return sample;
}

perf::Sample
perf::Sampler::read_cgroup_event(perf::SampleBuffer::Entry&& entry)
{
  auto sample = Sample{};
  sample.metadata().mode(entry.mode());

  const auto cgroup_id = entry.read<std::uint64_t>();
  auto* path = entry.as<const char*>();

  sample.cgroup(CGroup{ cgroup_id, std::string{ path } });

  return sample;
}

perf::Sample
perf::Sampler::read_throttle_event(perf::SampleBuffer::Entry&& entry) const noexcept
{
  auto sample = Sample{};
  sample.metadata().mode(entry.mode());

  if (this->_values.is_set(PERF_SAMPLE_TIME)) {
    sample.metadata().timestamp(entry.read<std::uint64_t>());
  }

  if (this->_values.is_set(PERF_SAMPLE_STREAM_ID)) {
    sample.metadata().stream_id(entry.read<std::uint64_t>());
  }

  /// Read sample_id.
  this->read_sample_id_all(entry, sample);

  sample.throttle(Throttle{ entry.is_throttle() });

  return sample;
}

perf::Sampler::SampleCounter::~SampleCounter()
{
  /// Close the group.
  this->_group.close();
}

std::vector<perf::Sample>
perf::MultiSamplerBase::result(const std::vector<Sampler>& samplers, const bool is_sort_by_time)
{
  if (!samplers.empty()) {
    auto result = samplers.front().result();

    /// Merge the results from all samplers (the result of the first sampler is the start point).
    for (auto i = 1U; i < samplers.size(); ++i) {
      auto sampler_result = samplers[i].result();
      std::move(sampler_result.begin(), sampler_result.end(), std::back_inserter(result));
    }

    /// Sort, if requested and supported by all samplers.
    if (is_sort_by_time) {
      /// Verify that all samplers recorded the timestamp that is needed to sort by time.
      const auto is_time_provided = std::all_of(
        samplers.begin(), samplers.end(), [](const auto& sampler) { return sampler._values.is_set(PERF_SAMPLE_TIME); });

      if (is_time_provided) {
        std::sort(result.begin(), result.end(), SampleTimestampComparator{});
      }
    }

    return result;
  }

  return std::vector<perf::Sample>{};
}

void
perf::MultiSamplerBase::trigger(std::vector<Sampler>& samplers, std::vector<std::vector<std::string>>&& trigger_names)
{
  for (auto sampler_id = 0U; sampler_id < samplers.size(); ++sampler_id) {
    if (sampler_id < samplers.size() - 1U) {
      samplers[sampler_id].trigger(std::vector<std::vector<std::string>>{ trigger_names });
    } else {
      samplers[sampler_id].trigger(std::move(trigger_names));
    }
  }
}

void
perf::MultiSamplerBase::trigger(std::vector<Sampler>& samplers, std::vector<std::vector<Sampler::Trigger>>&& triggers)
{
  for (auto sampler_id = 0U; sampler_id < samplers.size(); ++sampler_id) {
    if (sampler_id < samplers.size() - 1U) {
      samplers[sampler_id].trigger(std::vector<std::vector<Sampler::Trigger>>{ triggers });
    } else {
      samplers[sampler_id].trigger(std::move(triggers));
    }
  }
}

void
perf::MultiSamplerBase::open(perf::Sampler& sampler, const perf::SampleConfig config)
{
  sampler._values = _values;
  sampler._config = config;

  sampler.open();
}

void
perf::MultiSamplerBase::start(perf::Sampler& sampler, const perf::SampleConfig config)
{
  sampler._values = _values;
  sampler._config = config;

  std::ignore = sampler.start();
}

perf::MultiThreadSampler::MultiThreadSampler(const perf::CounterDefinition& counter_list,
                                             const std::uint16_t num_threads,
                                             const perf::SampleConfig config)
  : MultiSamplerBase(config)
{
  /// Create thread-local samplers without config (will be set when starting).
  for (auto thread_id = 0U; thread_id < num_threads; ++thread_id) {
    this->_thread_local_samplers.emplace_back(counter_list);
  }
}

perf::MultiCoreSampler::MultiCoreSampler(const perf::CounterDefinition& counter_list,
                                         std::vector<std::uint16_t>&& core_ids,
                                         perf::SampleConfig config)
  : MultiSamplerBase(config)
  , _core_ids(std::move(core_ids))
{
  /// Record all processes on the CPUs.
  _config.process_id(-1);

  /// Create thread-local samplers without config (will be set when starting).
  for (auto core_id = 0U; core_id < this->_core_ids.size(); ++core_id) {
    this->_core_local_samplers.emplace_back(counter_list);
  }
}

void
perf::MultiCoreSampler::open()
{
  for (auto sampler_id = 0U; sampler_id < this->_core_ids.size(); ++sampler_id) {
    auto config = this->_config;
    config.cpu_id(this->_core_ids[sampler_id]);
    MultiSamplerBase::open(this->_core_local_samplers[sampler_id], config);
  }
}

bool
perf::MultiCoreSampler::start()
{
  for (auto sampler_id = 0U; sampler_id < this->_core_ids.size(); ++sampler_id) {
    auto config = this->_config;
    config.cpu_id(this->_core_ids[sampler_id]);
    MultiSamplerBase::start(this->_core_local_samplers[sampler_id], config);
  }

  return true;
}
