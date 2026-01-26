#include <algorithm>
#include <perfcpp/exception.h>
#include <perfcpp/hardware_info.h>
#include <perfcpp/ibs_decoder.h>
#include <perfcpp/record_file_writer.h>
#include <perfcpp/sample_decoder.h>
#include <perfcpp/sampler.h>
#include <stdexcept>
#include <utility>

perf::Sampler::SampleCounter::~SampleCounter()
{
  /// Close the group.
  this->_group.close();
}

perf::Sampler&
perf::Sampler::trigger(std::vector<std::vector<std::string>>&& list_of_triggers)
{
  auto triggers = std::vector<std::vector<Trigger>>{};
  triggers.reserve(list_of_triggers.size());

  /// Turn the list of event names in a list of Sampler::Trigger objects and continue processing (checking if the
  /// trigger is an existing event, not a metric, etc.) there.
  for (auto& trigger_names : list_of_triggers) {
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
perf::Sampler::trigger(std::vector<std::vector<Trigger>>&& list_of_triggers)
{
  /// Deny to modify triggers after the sampler was already opened.
  if (this->_is_opened) {
    throw CannotChangeTriggerWhenSamplerOpenedError{};
  }

  /// Remove all triggers that where added so far.
  this->_triggers.clear();

  /// When no triggers provided, we're done.
  if (list_of_triggers.empty()) {
    return *this;
  }

  /// Process all requested triggers.
  this->_triggers.reserve(list_of_triggers.size());
  for (auto& trigger_group : list_of_triggers) {
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
  /// Measuring any CPU core and any process is invalid, according to the perf subsystem documentation.
  if (this->_config.cpu_core().is_any() && this->_config.process().is_any()) {
    throw InvalidConfigAnyCpuCoreAndAnyProcess{};
  }

  /// Do not open again, if the sampler was already opened.
  /// The is_open flag will be reset on closing the sampler.
  if (std::exchange(this->_is_opened, true)) {
    return;
  }

  /// Build the groups from triggers + events from values.
  for (const auto& trigger_group : this->_triggers) {
    /// Convert the trigger group (list of (event name, configuration attributes)) into "real" sample event, which is
    /// basically a group of hardware events (one ore multiple triggers and to-recorded hardware events, if requested).
    if (!trigger_group.empty()) {
      /// As each event can be available on different, heterogeneous PMUs, we need to check if a trigger is available on
      /// multiple PMUs and add it multiple times – once per PMU.
      const auto event_name = std::get<0>(trigger_group.front());
      for (const auto& hardware_events : this->_counter_definitions.counter(event_name)) {
        auto sample_counter = this->transform_trigger_to_sample_counter(std::get<0>(hardware_events), trigger_group);
        this->_sample_counter.push_back(std::move(sample_counter));
      }
    }
  }

  /// Verify that at least one trigger was configured.
  if (this->_sample_counter.empty()) {
    throw CannotStartEmptySamplerError{};
  }

  /// Open the trigger hardware events.
  for (auto& sample_counter : this->_sample_counter) {
    /// Open the group.
    sample_counter.group().open(
      this->_config,
      sample_counter.has_intel_auxiliary_event(),
      this->_config.buffer_pages(),
      this->_values);
  }
}

bool
perf::Sampler::start()
{
  /// Clear the sample data.
  this->_sample_data.clear();

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
    /// Clear all buffers, groups, and event names
    /// in order to enable opening again.
    this->_sample_counter.clear();

    /// Clear the sample data.
    this->_sample_data.clear();
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

  /// List of event names that should be read later from results.
  auto requested_events = RequestedEventSet{};

  /// Check if the auxiliary event is needed and needs to be added.
  const auto [is_auxiliary_event_needed, is_auxiliary_event_included] =
    this->is_auxiliary_event_needed_and_already_included(pmu_name, trigger_group);

  /// If the auxiliary event is needed but not included, add it.
  if (is_auxiliary_event_needed && !is_auxiliary_event_included) {
    if (auto auxiliary_event = this->_counter_definitions.counter(pmu_name, "mem-loads-aux");
        auxiliary_event.has_value()) {

      /// Read the event config (like event id, etc.).
      auto auxiliary_event_config = std::get<2>(auxiliary_event.value());

      /// The auxiliary event needs constant skid.
      auxiliary_event_config.precision(Precision::MustHaveConstantSkid);

      /// Set the event's period or frequency equal to the first trigger (or fall back to config if not configured).
      auto period_or_frequency = std::get<2>(trigger_group.front());
      auxiliary_event_config.period_or_frequency(period_or_frequency.value_or(this->_config.period_for_frequency()));

      /// Add the event to the group.
      group.add(auxiliary_event_config);
    } else {
      throw AuxiliaryEventForSamplingNotFoundError{};
    }
  }

  /// Add the trigger(s) to the group. For the most time, this will be a single trigger.
  for (const auto& trigger : trigger_group) {
    const auto [event_name, precision, period_or_frequency] = trigger;
    if (auto event_name_and_config = this->_counter_definitions.counter(pmu_name, event_name);
        event_name_and_config.has_value()) {

      /// Read the event config (like event id, etc.).
      auto event_config = std::get<2>(event_name_and_config.value());

      /// Set the event's precision (fall back to config if empty).
      event_config.precision(static_cast<std::uint8_t>(precision.value_or(this->_config.precise_ip())));

      /// Set the event's period or frequency (fall back to config if empty).
      event_config.period_or_frequency(period_or_frequency.value_or(this->_config.period_for_frequency()));

      /// Add the event to the group.
      group.add(event_config);

      /// Notice the event name of the trigger event.
      if (this->_values.is_set(SampleRecordingValues::Field::PerformanceCounter)) {
        requested_events.add(RequestedEvent{ pmu_name, event_name, /* group_id */ 0U, /* position in group */ 0U });
      }
    } else {
      throw CannotFindEventError{ pmu_name, event_name };
    }
  }

  /// Add possible events as value to the sample.
  if (this->_values.is_set(SampleRecordingValues::Field::PerformanceCounter)) {
    for (const auto& event_name : this->_values.counters()) {

      /// Check if the event is a true hardware event – if so, just add it to the list.
      if (auto event_config = this->_counter_definitions.counter(pmu_name, event_name); event_config.has_value()) {
        /// Add the event to the requested event set.
        /// If the request returns true, the event as indeed added and needs to be added to the group.
        /// The group id provided to the event set is 0 since there is only one group.
        const auto is_added = requested_events.add(RequestedEvent{
          pmu_name, std::get<1>(event_config.value()), /* group_id */ 0U, static_cast<std::uint8_t>(group.size()) });
        if (is_added) {
          group.add(std::get<2>(event_config.value()));
        }
      }

      /// Otherwise, check if the event is a metric. In that case, add all depending hardware events (if not already
      /// done).
      else if (const auto metric = this->_counter_definitions.metric(event_name); metric.has_value()) {
        this->add(metric.value(), pmu_name, requested_events, group);
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

void
perf::Sampler::add(const std::pair<std::string_view, Metric&> metric,
                   const std::string_view pmu_name,
                   perf::RequestedEventSet& requested_event_set,
                   perf::Group& group) const
{
  const auto metric_name = std::get<0>(metric);
  /// For metrics, we need to add every hardware event the metric depends on (and check their existence).
  for (const auto& depending_event_name : std::get<1>(metric).required_counter_names()) {
    if (const auto depending_event_config = this->_counter_definitions.counter(pmu_name, depending_event_name);
        depending_event_config.has_value()) {

      /// Add the event to the requested event set.
      /// If the request returns true, the event is indeed added and needs to be added to the group.
      /// The group id provided to the event set is 0 since there is only one group.
      const auto is_added = requested_event_set.add(RequestedEvent{ pmu_name,
                                                                    std::get<1>(depending_event_config.value()),
                                                                    /* group_id */ 0U,
                                                                    static_cast<std::uint8_t>(group.size()) });
      if (is_added) {
        group.add(std::get<2>(depending_event_config.value()));
      }
    } else if (const auto depending_metric = this->_counter_definitions.metric(depending_event_name);
               depending_metric.has_value()) {
      this->add(depending_metric.value(), pmu_name, requested_event_set, group);
    } else if (this->_counter_definitions.is_time_event(depending_event_name)) {
      throw TimeEventNotSupportedForSamplingError{ depending_event_name };
    } else {
      throw CannotFindEventForMetricError{ depending_event_name, metric_name };
    }
  }

  /// Add the metric to the list of scheduled events.
  requested_event_set.add(RequestedEvent{ metric_name, true, RequestedEvent::Type::Metric });
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

            /// If there is a mem-loads event we need the auxiliary event.
            if (const auto is_mem_loads_event =
                  std::get<2>(trigger_event.value()) == std::get<2>(mem_loads_event.value());
                is_mem_loads_event) {

              /// Check if the first event in the group is already the mem-loads-aux event.
              if (const auto leading_trigger_event =
                    this->_counter_definitions.counter(pmu_name, std::get<0>(trigger_group.front()));
                  leading_trigger_event.has_value()) {
                const auto has_mem_loads_aux_event =
                  std::get<2>(leading_trigger_event.value()) == std::get<2>(mem_loads_aux_event.value());
                return std::make_pair(true, has_mem_loads_aux_event);
              }

              /// The auxiliary event is needed, but not included.
              return std::make_pair(true, false);
            }
          }
        }
      }
    }
  }

  return std::make_pair(false, false);
}

perf::SampleResult
perf::Sampler::result(const bool sort_by_time)
{
  /// Consume the sample data, when not already consumed.
  const auto& sample_data = this->consume_sample_data();

  if (this->_sample_counter.size() != sample_data.size()) {
    return SampleResult{};
  }

  auto result = std::vector<Sample>{};

  auto sample_decoder = SampleDecoder{ this->_counter_definitions, this->_values };
  for (auto sample_counter_id = 0U; sample_counter_id < this->_sample_counter.size(); ++sample_counter_id) {
    const auto& sample_counter = this->_sample_counter[sample_counter_id];
    const auto& counter_sample_data = sample_data[sample_counter_id];

    /// Decode all samples from the buffers.
    auto samples = sample_decoder.decode(counter_sample_data,
                                         sample_counter.has_amd_op_pmu_counter(),
                                         sample_counter.has_amd_fetch_pmu_counter(),
                                         sample_counter.requested_events(),
                                         sample_counter.group());

    /// Append samples to the entire result.
    std::move(samples.begin(), samples.end(), std::back_inserter(result));
  }

  /// Sort the samples if requested and we can sort by time.
  if (this->_values.is_set(SampleRecordingValues::Field::Timestamp) && sort_by_time) {
    std::sort(result.begin(), result.end(), SampleTimestampComparator{});
  }

  return SampleResult{ this->_values, std::move(result) };
}

void
perf::Sampler::to_perf_file(const std::string_view output_file_name)
{
  RecordFileWriter::write(this->_values, this->_sample_counter, this->consume_sample_data(), output_file_name);
}

std::vector<std::vector<std::vector<std::byte>>>&
perf::Sampler::consume_sample_data()
{
  /// Check if the sample data is not yet consumed (i.e., we store a vector of data equal to the size of the sample
  /// counters).
  if (this->_sample_data.size() != this->_sample_counter.size()) {
    this->_sample_data.reserve(this->_sample_counter.size());
    for (auto& sample_counter : this->_sample_counter) {
      this->_sample_data.push_back(sample_counter.consume_samples());
    }
  }

  return this->_sample_data;
}

std::vector<std::vector<std::byte>>
perf::Sampler::SampleCounter::consume_samples()
{
  /// Normally, the first member will control the sample buffer; however, on some Intel
  /// architectures, an auxiliary event is needed before the "real" event – the "real" event controlling the
  /// buffer is the second one.
  const auto event_index = 0U + static_cast<std::uint8_t>(this->_has_intel_auxiliary_event);
  if (auto& members = this->group().members();
      members.size() > event_index && members[event_index].mmap_buffer() != nullptr) {
    return members[event_index].mmap_buffer()->consume_data();
  }

  return {};
}

perf::SampleResult
perf::MultiSamplerBase::result(std::vector<Sampler>& samplers, const bool is_sort_by_time)
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
        samplers.begin(), samplers.end(), [](const auto& sampler) { return sampler._values.is_set(SampleRecordingValues::Field::Timestamp); });

      /// Finally, sort if requested and the samples contain a timestamp.
      if (is_time_provided) {
        std::sort(result.begin(), result.end(), SampleTimestampComparator{});
      }
    }

    return result;
  }

  return SampleResult{};
}

void
perf::MultiSamplerBase::to_perf_file(std::vector<Sampler>& samplers, std::string_view output_file_name)
{
  if (!samplers.empty()) {
    /// Since we cannot modify any samplers data, we copy every sample into a new set.
    auto accumulated_sample_data = samplers.front().consume_sample_data();

    /// Merge the data from all samplers (the result of the first sampler is the start point).
    for (auto i = 1U; i < samplers.size(); ++i) {
      const auto& sample_data = samplers[i].consume_sample_data();

      /// Verify that both samples contain the same number of counters.
      if (accumulated_sample_data.size() == sample_data.size()) {
        for (auto counter_id = 0U; counter_id < sample_data.size(); ++counter_id) {
          const auto& counter_sample_data = sample_data[counter_id];

          /// Append the data for every counter as different counters will have different sample data.
          accumulated_sample_data[counter_id].insert(
            accumulated_sample_data[counter_id].end(), counter_sample_data.begin(), counter_sample_data.end());
        }
      }
    }

    /// Write the result using the first sampler as a template.
    RecordFileWriter::write(
      samplers.front()._values, samplers.front()._sample_counter, accumulated_sample_data, output_file_name);
  }
}

void
perf::MultiSamplerBase::trigger(std::vector<Sampler>& samplers, std::vector<std::vector<std::string>>&& trigger_names)
{
  for (auto sampler_id = 0U; sampler_id < samplers.size() - 1U; ++sampler_id) {
    samplers[sampler_id].trigger(std::vector<std::vector<std::string>>{ trigger_names });
  }

  samplers.back().trigger(std::move(trigger_names));
}

void
perf::MultiSamplerBase::trigger(std::vector<Sampler>& samplers, std::vector<std::vector<Sampler::Trigger>>&& triggers)
{
  for (auto sampler_id = 0U; sampler_id < samplers.size() - 1U; ++sampler_id) {
    samplers[sampler_id].trigger(std::vector<std::vector<Sampler::Trigger>>{ triggers });
  }

  samplers.back().trigger(std::move(triggers));
}

void
perf::MultiSamplerBase::open(perf::Sampler& sampler, const perf::SampleConfig config) const
{
  sampler._values = _values;
  sampler._config = config;

  sampler.open();
}

void
perf::MultiSamplerBase::start(perf::Sampler& sampler, const perf::SampleConfig config) const
{
  sampler._values = _values;
  sampler._config = config;

  std::ignore = sampler.start();
}

perf::MultiThreadSampler::MultiThreadSampler(const perf::CounterDefinition& counter_definition,
                                             const std::uint16_t num_threads,
                                             const perf::SampleConfig config)
  : MultiSamplerBase(config)
{
  /// Create thread-local samplers without config (will be set when starting).
  for (auto thread_id = 0U; thread_id < num_threads; ++thread_id) {
    this->_thread_local_samplers.emplace_back(counter_definition);
  }
}

perf::MultiCoreSampler::MultiCoreSampler(const perf::CounterDefinition& counter_definition,
                                         std::vector<std::uint16_t>&& core_ids,
                                         perf::SampleConfig config)
  : MultiSamplerBase(config)
  , _core_ids(std::move(core_ids))
{
  /// Record all processes on the CPUs.
  _config.process(Process::Any);

  /// Create thread-local samplers without config (will be set when starting).
  for (auto core_id = 0U; core_id < this->_core_ids.size(); ++core_id) {
    this->_core_local_samplers.emplace_back(counter_definition);
  }
}

void
perf::MultiCoreSampler::open()
{
  for (auto sampler_id = 0U; sampler_id < this->_core_ids.size(); ++sampler_id) {
    auto config = this->_config;
    config.cpu_core(CpuCore{ this->_core_ids[sampler_id] });
    MultiSamplerBase::open(this->_core_local_samplers[sampler_id], config);
  }
}

bool
perf::MultiCoreSampler::start()
{
  for (auto sampler_id = 0U; sampler_id < this->_core_ids.size(); ++sampler_id) {
    auto config = this->_config;
    config.cpu_core(CpuCore{ this->_core_ids[sampler_id] });
    MultiSamplerBase::start(this->_core_local_samplers[sampler_id], config);
  }

  return true;
}
