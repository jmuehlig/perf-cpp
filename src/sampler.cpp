#include <algorithm>
#include <perfcpp/sampler.h>
#include <stdexcept>
#include <sys/mman.h>
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
  this->_triggers.reserve(triggers.size());

  for (auto& trigger_group : triggers) {
    auto trigger_group_references =
      std::vector<std::tuple<std::string_view, std::optional<Precision>, std::optional<PeriodOrFrequency>>>{};
    trigger_group_references.reserve(trigger_group.size());

    for (auto& trigger : trigger_group) {
      /// Reject metrics as trigger events as metrics consist of multiple events.
      if (this->_counter_definitions.is_metric(trigger.name())) {
        throw std::runtime_error{ std::string{ "Counter '" }
                                    .append(trigger.name())
                                    .append("' seems to be a metric. Metrics are not supported as triggers.") };
      }

      /// Read the config (like event id etc.) from every trigger name and verify that the trigger event exists in the
      /// CounterDefinition.
      if (auto counter_config = this->_counter_definitions.counter(trigger.name()); counter_config.has_value()) {
        trigger_group_references.emplace_back(
          std::get<0>(counter_config.value()), trigger.precision(), trigger.period_or_frequency());
      } else {
        throw std::runtime_error{ std::string{ "Cannot find counter '" }.append(trigger.name()).append("'.") };
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
  for (const auto& trigger : this->_triggers) {
    /// Convert the trigger (event name, configuration attributes) into a "real" sample counter, which is basically a group of hardware events (one ore multiple triggers and to-recorded hardware events, if requested).
    auto sample_counter = this->transform_trigger_to_sample_counter(trigger);
    this->_sample_counter.push_back(std::move(sample_counter));
  }

  /// Verify that at least one trigger was configured.
  if (this->_sample_counter.empty()) {
    throw std::runtime_error{ "No trigger for sampling specified." };
  }

  /// Open the trigger hardware events.
  for (auto& sample_counter : this->_sample_counter) {
    /// Detect, if the leader is an auxiliary (specifically for Sapphire Rapids).
    const auto is_leader_auxiliary_counter = sample_counter.group().member(0U).is_auxiliary();

    auto group_leader_file_descriptor = -1LL;

    /// Open the conunters.
    for (auto counter_index = 0U; counter_index < sample_counter.group().size(); ++counter_index) {
      auto& counter = sample_counter.group().member(counter_index);

      /// The first counter in the group has a "special" role, others will use its file descriptor.
      const auto is_leader = counter_index == 0U;

      /// For Intel's Sapphire Rapids architecture, sampling for memory requires a dummy as first counter.
      /// Only the second counter is the "real" sampling counter.
      const auto is_secret_leader = is_leader_auxiliary_counter && counter_index == 1U;

#ifndef PERFCPP_NO_RECORD_CGROUP
      const auto is_include_cgroup = this->_values.is_set(PERF_SAMPLE_CGROUP);
#else
      const auto is_include_cgroup = false;
#endif

      /// Open the hardware counter. If this fails, the counter.open() method fill throw an exception.
      counter.open(
        this->_config.is_debug(),
        is_leader,
        is_secret_leader,
        group_leader_file_descriptor,
        this->_config.cpu_id(),
        this->_config.process_id(),
        this->_config.is_include_child_threads(),
        this->_config.is_include_kernel(),
        this->_config.is_include_user(),
        this->_config.is_include_hypervisor(),
        this->_config.is_include_idle(),
        this->_config.is_include_guest(),
        this->_values.is_set(PERF_SAMPLE_READ),
        this->_values.get(),
        this->_values.is_set(PERF_SAMPLE_BRANCH_STACK) ? std::make_optional(this->_values.branch_mask()) : std::nullopt,
        this->_values.is_set(PERF_SAMPLE_REGS_USER) ? std::make_optional(this->_values.user_registers().mask())
                                                    : std::nullopt,
        this->_values.is_set(PERF_SAMPLE_REGS_INTR) ? std::make_optional(this->_values.kernel_registers().mask())
                                                    : std::nullopt,
        this->_values.is_set(PERF_SAMPLE_CALLCHAIN) ? std::make_optional(this->_values.max_call_stack()) : std::nullopt,
        this->_values._is_include_context_switch,
        is_include_cgroup);

      /// Set the group leader file descriptor.
      if (is_leader) {
        group_leader_file_descriptor = counter.file_descriptor();
      }
    }

    /// Get the file descriptor for opening the user-level buffer used for storing samples.
    /// For the most times, this will be the group leader's file descriptor.
    /// However, some architectures (e.g., Intel's Sapphire Rapids) need an "auxiliary" counter.
    /// If this is the case, use the file descriptor of the second counter (the "real" counter) instead.
    auto buffer_file_descriptor = group_leader_file_descriptor;
    if (is_leader_auxiliary_counter && sample_counter.group().size() > 1U) {
      buffer_file_descriptor = sample_counter.group().member(1U).file_descriptor();
    }

    /// Open the mapped buffer.
    auto* buffer = ::mmap(nullptr,
                          this->_config.buffer_pages() * 4096U,
                          PROT_READ,
                          MAP_SHARED,
                          static_cast<std::int32_t>(buffer_file_descriptor),
                          0);

    /// Verify the buffer was opened.
    if (buffer == MAP_FAILED) {
      throw std::runtime_error{ "Creating buffer via mmap() failed." };
    } else if (buffer == nullptr) {
      throw std::runtime_error{ "Created buffer via mmap() is null." };
    }

    sample_counter.buffer(buffer, this->_config.buffer_pages());
  }
}

bool
perf::Sampler::start()
{
  /// Open the groups.
  this->open();

  /// Enable the counters.
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
perf::Sampler::close()
{
  if (std::exchange(this->_is_opened, false)) {
    /// Clear all buffers, groups, and counter names
    /// in order to enable opening again.
    this->_sample_counter.clear();
  }
}

perf::Sampler::SampleCounter
perf::Sampler::transform_trigger_to_sample_counter(const std::vector<std::tuple<std::string_view, std::optional<Precision>, std::optional<PeriodOrFrequency>>>& triggers) const
{
  /// Group of hardware events.
  auto group = Group{};

  /// List of counter names that should be read later from results.
  auto counter_names = std::vector<std::string_view>{};

  /// Add the trigger(s) to the group. For the most time, this will be a single trigger. Some architectures need specific auxiliary counters.
  for (const auto trigger : triggers) {
    if (auto counter_name_and_config = this->_counter_definitions.counter(std::get<0>(trigger));
        counter_name_and_config.has_value()) {

      /// Read the counter config (like event id, etc.).
      auto counter_config = std::get<1>(counter_name_and_config.value());

      /// Set the counters precise_ip (fall back to config if empty).
      const auto precision = std::get<1>(trigger).value_or(this->_config.precise_ip());
      counter_config.precise_ip(static_cast<std::uint8_t>(precision));

      /// Set the counters period or frequency (fall back to config if empty).
      const auto period_or_frequency = std::get<2>(trigger).value_or(this->_config.period_for_frequency());
      std::visit(
        [&counter_config](const auto period_or_frequency) {
          using T = std::decay_t<decltype(period_or_frequency)>;
          if constexpr (std::is_same_v<T, class Period>) {
            counter_config.period(period_or_frequency.get());
          } else if constexpr (std::is_same_v<T, class Frequency>) {
            counter_config.frequency(period_or_frequency.get());
          }
        },
        period_or_frequency);

      /// Add the counter to the group.
      group.add(counter_config);

      /// Notice the counter name of the trigger event.
      if (this->_values.is_set(PERF_SAMPLE_READ)) {
        counter_names.push_back(std::get<0>(trigger));
      }
    } else {
      throw std::runtime_error{std::string{"Cannot find trigger '"}.append(std::get<0>(trigger)).append("'.")};
    }
  }

  /// Add possible counters as value to the sample.
  if (this->_values.is_set(PERF_SAMPLE_READ)) {
    for (const auto& counter_name : this->_values.counters()) {

      /// Verify the counter is not a metric.
      if (this->_counter_definitions.is_metric(counter_name)) {
        throw std::runtime_error{ std::string{ "Counter '" }
                                    .append(counter_name)
                                    .append("' seems to be a metric. Metrics are not supported for sampling.") };
      }

      /// Find the counter.
      if (auto counter_config = this->_counter_definitions.counter(counter_name); counter_config.has_value()) {
        /// Add the counter to the group and to the list of counters.
        counter_names.push_back(std::get<0>(counter_config.value()));
        group.add(std::get<1>(counter_config.value()));
      } else {
        throw std::runtime_error{ std::string{ "Cannot find counter '" }.append(counter_name).append("'.") };
      }
    }

    if (!counter_names.empty()) {
      return SampleCounter{std::move(group), std::move(counter_names)};
    }
  }

  return SampleCounter{std::move(group)};
}

std::vector<perf::Sample>
perf::Sampler::result(const bool sort_by_time) const
{
  auto result = std::vector<Sample>{};
  result.reserve(2048U);

  for (const auto& sample_counter : this->_sample_counter) {
    if (sample_counter.buffer() == nullptr) {
      continue;
    }

    auto* mmap_page = reinterpret_cast<perf_event_mmap_page*>(sample_counter.buffer());

    /// When the ringbuffer is empty or already read, there is nothing to do.
    if (mmap_page->data_tail >= mmap_page->data_head) {
      return result;
    }

    /// The buffer starts at page 1 (from 0).
    auto iterator = std::uintptr_t(sample_counter.buffer()) + 4096U;

    /// data_head is the size (in bytes) of the samples.
    const auto end = iterator + mmap_page->data_head;

    while (iterator < end) {
      auto* event_header = reinterpret_cast<perf_event_header*>(iterator);
      auto entry = UserLevelBufferEntry{ event_header };

      if (entry.is_sample_event()) { /// Read "normal" samples.
        result.push_back(this->read_sample_event(entry, sample_counter));
      } else if (entry.is_loss_event()) { /// Read lost samples.
        result.push_back(this->read_loss_event(entry));
      } else if (entry.is_context_switch_event()) { /// Read context switch.
        result.push_back(this->read_context_switch_event(entry));
      } else if (entry.is_cgroup_event()) { /// Read cgroup samples.
        result.push_back(Sampler::read_cgroup_event(entry));
      } else if (entry.is_throttle_event() && this->_values._is_include_throttle) { /// Read (un-) throttle samples.
        result.push_back(this->read_throttle_event(entry));
      }

      /// Go to the next sample.
      iterator += event_header->size;
    }
  }

  /// Sort the samples if requested and we can sort by time.
  if (this->_values.is_set(PERF_SAMPLE_TIME) && sort_by_time) {
    std::sort(result.begin(), result.end(), SampleTimestampComparator{});
  }

  return result;
}

void
perf::Sampler::read_sample_id(perf::Sampler::UserLevelBufferEntry& entry, perf::Sample& sample) const noexcept
{
  if (this->_values.is_set(PERF_SAMPLE_TID)) {
    sample.process_id(entry.read<std::uint32_t>());
    sample.thread_id(entry.read<std::uint32_t>());
  }

  if (this->_values.is_set(PERF_SAMPLE_TIME)) {
    sample.timestamp(entry.read<std::uint64_t>());
  }

  if (this->_values.is_set(PERF_SAMPLE_STREAM_ID)) {
    sample.stream_id(entry.read<std::uint64_t>());
  }

  if (this->_values.is_set(PERF_SAMPLE_CPU)) {
    sample.cpu_id(entry.read<std::uint32_t>());
    entry.skip<std::uint32_t>(); /// Skip "res".
  }

  if (this->_values.is_set(PERF_SAMPLE_IDENTIFIER)) {
    sample.id(entry.read<std::uint64_t>());
  }
}

perf::Sample::Mode
perf::Sampler::UserLevelBufferEntry::mode() const noexcept
{
  if (static_cast<bool>(this->_misc & PERF_RECORD_MISC_KERNEL)) {
    return Sample::Mode::Kernel;
  } else if (static_cast<bool>(this->_misc & PERF_RECORD_MISC_USER)) {
    return Sample::Mode::User;
  } else if (static_cast<bool>(this->_misc & PERF_RECORD_MISC_HYPERVISOR)) {
    return Sample::Mode::Hypervisor;
  } else if (static_cast<bool>(this->_misc & PERF_RECORD_MISC_GUEST_KERNEL)) {
    return Sample::Mode::GuestKernel;
  } else if (static_cast<bool>(this->_misc & PERF_RECORD_MISC_GUEST_USER)) {
    return Sample::Mode::GuestUser;
  }

  return Sample::Mode::Unknown;
}

perf::Sample
perf::Sampler::read_sample_event(perf::Sampler::UserLevelBufferEntry entry, const SampleCounter& sample_counter) const
{
  auto sample = Sample{ entry.mode() };

  sample.is_exact_ip(entry.is_exact_ip());

  if (this->_values.is_set(PERF_SAMPLE_IDENTIFIER)) {
    sample.sample_id(entry.read<std::uint64_t>());
  }

  if (this->_values.is_set(PERF_SAMPLE_IP)) {
    sample.instruction_pointer(entry.read<std::uintptr_t>());
  }

  if (this->_values.is_set(PERF_SAMPLE_TID)) {
    sample.process_id(entry.read<std::uint32_t>());
    sample.thread_id(entry.read<std::uint32_t>());
  }

  if (this->_values.is_set(PERF_SAMPLE_TIME)) {
    sample.timestamp(entry.read<std::uint64_t>());
  }

  if (this->_values.is_set(PERF_SAMPLE_STREAM_ID)) {
    sample.stream_id(entry.read<std::uint64_t>());
  }

  if (this->_values.is_set(PERF_SAMPLE_ADDR)) {
    sample.logical_memory_address(entry.read<std::uint64_t>());
  }

  if (this->_values.is_set(PERF_SAMPLE_CPU)) {
    sample.cpu_id(entry.read<std::uint32_t>());
    entry.skip<std::uint32_t>(); /// Skip "res".
  }

  if (this->_values.is_set(PERF_SAMPLE_PERIOD)) {
    sample.period(entry.read<std::uint64_t>());
  }

  if (this->_values.is_set(PERF_SAMPLE_READ)) {
    /// Read the number of counters.
    const auto count_counter_values = entry.read<decltype(CounterReadFormat<Group::MAX_MEMBERS>::count_members)>();

    /// Time enabled and running for correction.
    const auto time_enabled = entry.read<decltype(CounterReadFormat<Group::MAX_MEMBERS>::time_enabled)>();
    const auto time_running = entry.read<decltype(CounterReadFormat<Group::MAX_MEMBERS>::time_running)>();
    const auto multiplexing_correction = double(time_enabled) / double(time_running);

    /// Read the counters (if the number matches the number of specified counters).
    auto* counter_values = entry.read<CounterReadFormat<Group::MAX_MEMBERS>::value>(count_counter_values);
    if (count_counter_values == sample_counter.group().size()) {
      auto counter_results = std::vector<std::pair<std::string_view, double>>{};

      /// Add each counter and its value to the result set of the sample.
      for (auto counter_id = 0U; counter_id < sample_counter.group().size(); ++counter_id) {
        const auto counter_name = sample_counter.counter_names()[counter_id];

        /// Counter value (corrected).
        const auto counter_result = double(counter_values[counter_id].value) * multiplexing_correction;

        counter_results.emplace_back(counter_name, counter_result);
      }
      sample.counter_result(CounterResult{ std::move(counter_results) });
    }
  }

  if (this->_values.is_set(PERF_SAMPLE_CALLCHAIN)) {
    /// Read the size of the callchain.
    const auto callchain_size = entry.read<std::uint64_t>();

    if (callchain_size > 0U) {
      auto callchain = std::vector<std::uintptr_t>{};
      callchain.reserve(callchain_size);

      /// Read the callchain entries.
      auto* instruction_pointers = entry.read<std::uint64_t>(callchain_size);
      for (auto index = 0U; index < callchain_size; ++index) {
        callchain.push_back(std::uintptr_t{ instruction_pointers[index] });
      }

      sample.callchain(std::move(callchain));
    }
  }

  if (this->_values.is_set(PERF_SAMPLE_RAW)) {
    /// Read the size of the raw sample.
    const auto raw_data_size = entry.read<std::uint32_t>();

    /// Read the raw data.
    const auto* raw_sample_data = entry.read<char>(raw_data_size);
    auto raw_data = std::vector<char>(std::size_t{ raw_data_size }, '\0');
    for (auto i = 0U; i < raw_data_size; ++i) {
      raw_data[i] = raw_sample_data[i];
    }

    sample.raw(std::move(raw_data));
  }

  if (this->_values.is_set(PERF_SAMPLE_BRANCH_STACK)) {
    /// Read the size of the branch stack.
    const auto count_branches = entry.read<std::uint64_t>();

    if (count_branches > 0U) {
      auto branches = std::vector<Branch>{};
      branches.reserve(count_branches);

      /// Read the branch stack entries.
      auto* sampled_branches = entry.read<perf_branch_entry>(count_branches);
      for (auto i = 0U; i < count_branches; ++i) {
        const auto& branch = sampled_branches[i];
        branches.emplace_back(
          branch.from, branch.to, branch.mispred, branch.predicted, branch.in_tx, branch.abort, branch.cycles);
      }

      sample.branches(std::move(branches));
    }
  }

  if (this->_values.is_set(PERF_SAMPLE_REGS_USER)) {
    /// Read the register ABI.
    sample.user_registers_abi(entry.read<std::uint64_t>());

    /// Read the number of registers.
    const auto count_user_registers = this->_values.user_registers().size();

    if (count_user_registers > 0U) {
      auto user_registers = std::vector<std::uint64_t>{};
      user_registers.reserve(count_user_registers);

      /// Read the register values.
      const auto* perf_user_registers = entry.read<std::uint64_t>(count_user_registers);
      for (auto register_id = 0U; register_id < count_user_registers; ++register_id) {
        user_registers.push_back(perf_user_registers[register_id]);
      }

      sample.user_registers(std::move(user_registers));
    }
  }

  if (this->_values.is_set(PERF_SAMPLE_WEIGHT)) {
    sample.weight(perf::Weight{ static_cast<std::uint32_t>(entry.read<std::uint64_t>()) });
  }

#ifndef PERFCPP_NO_SAMPLE_WEIGHT_STRUCT
  else if (this->_values.is_set(PERF_SAMPLE_WEIGHT_STRUCT)) {
    const auto weight_struct = entry.read<perf_sample_weight>();
    sample.weight(perf::Weight{ weight_struct.var1_dw, weight_struct.var2_w, weight_struct.var3_w });
  }
#endif

  if (this->_values.is_set(PERF_SAMPLE_DATA_SRC)) {
    sample.data_src(perf::DataSource{ entry.read<std::uint64_t>() });
  }

  if (this->_values.is_set(PERF_SAMPLE_TRANSACTION)) {
    sample.transaction_abort(TransactionAbort{ entry.read<std::uint64_t>() });
  }

  if (this->_values.is_set(PERF_SAMPLE_REGS_INTR)) {
    /// Read the register ABI.
    sample.kernel_registers_abi(entry.read<std::uint64_t>());

    /// Read the number of registers.
    const auto count_kernel_registers = this->_values.kernel_registers().size();

    if (count_kernel_registers > 0U) {
      auto kernel_registers = std::vector<std::uint64_t>{};
      kernel_registers.reserve(count_kernel_registers);

      /// Read the register values.
      const auto* perf_kernel_registers = entry.read<std::uint64_t>(count_kernel_registers);
      for (auto register_id = 0U; register_id < count_kernel_registers; ++register_id) {
        kernel_registers.push_back(perf_kernel_registers[register_id]);
      }

      sample.kernel_registers(std::move(kernel_registers));
    }
  }

#ifndef PERFCPP_NO_SAMPLE_PHYS_ADDR
  if (this->_values.is_set(PERF_SAMPLE_PHYS_ADDR)) {
    sample.physical_memory_address(entry.read<std::uint64_t>());
  }
#endif

  if (this->_values.is_set(PERF_SAMPLE_CGROUP)) {
    sample.cgroup_id(entry.read<std::uint64_t>());
  }

#ifndef PERFCPP_NO_SAMPLE_DATA_PAGE_SIZE
  if (this->_values.is_set(PERF_SAMPLE_DATA_PAGE_SIZE)) {
    sample.data_page_size(entry.read<std::uint64_t>());
  }
#endif

#ifndef PERFCPP_NO_SAMPLE_CODE_PAGE_SIZE
  if (this->_values.is_set(PERF_SAMPLE_CODE_PAGE_SIZE)) {
    sample.code_page_size(entry.read<std::uint64_t>());
  }
#endif

  return sample;
}

perf::Sample
perf::Sampler::read_loss_event(perf::Sampler::UserLevelBufferEntry entry) const
{
  auto sample = Sample{ entry.mode() };

  sample.count_loss(entry.read<std::uint64_t>());

  /// Read sample_id.
  this->read_sample_id(entry, sample);

  return sample;
}

perf::Sample
perf::Sampler::read_context_switch_event(perf::Sampler::UserLevelBufferEntry entry) const
{
  auto sample = Sample{ entry.mode() };

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
  this->read_sample_id(entry, sample);

  sample.context_switch(ContextSwitch{ is_switch_out, is_switch_out_preempt, process_id, thread_id });

  return sample;
}

perf::Sample
perf::Sampler::read_cgroup_event(perf::Sampler::UserLevelBufferEntry entry)
{
  auto sample = Sample{ entry.mode() };

  const auto cgroup_id = entry.read<std::uint64_t>();
  auto* path = entry.as<const char*>();

  sample.cgroup(CGroup{ cgroup_id, std::string{ path } });

  return sample;
}

perf::Sample
perf::Sampler::read_throttle_event(perf::Sampler::UserLevelBufferEntry entry) const
{
  auto sample = Sample{ entry.mode() };

  if (this->_values.is_set(PERF_SAMPLE_TIME)) {
    sample.timestamp(entry.read<std::uint64_t>());
  }

  if (this->_values.is_set(PERF_SAMPLE_STREAM_ID)) {
    sample.stream_id(entry.read<std::uint64_t>());
  }

  /// Read sample_id.
  this->read_sample_id(entry, sample);

  sample.throttle(Throttle{ entry.is_throttle() });

  return sample;
}

perf::Sampler::SampleCounter::~SampleCounter()
{
  /// Free the buffer (if mmap-ed).
  if (this->_buffer != nullptr) {
    ::munmap(this->_buffer, this->_buffer_pages * 4096U);
  }

  /// Close the group.
  if (this->_group.leader_file_descriptor() > -1) {
    this->_group.close();
  }
}

std::vector<perf::Sample>
perf::MultiSamplerBase::result(const std::vector<Sampler>& sampler, bool sort_by_time)
{
  if (!sampler.empty()) {
    auto result = sampler.front().result();

    sort_by_time &= sampler.front()._values.is_set(PERF_SAMPLE_TIME);

    for (auto i = 1U; i < sampler.size(); ++i) {
      /// Only sort if all samplers recorded the timestamp.
      sort_by_time &= sampler[i]._values.is_set(PERF_SAMPLE_TIME);

      auto temp_result = sampler[i].result();
      std::move(temp_result.begin(), temp_result.end(), std::back_inserter(result));
    }

    if (sort_by_time) {
      std::sort(result.begin(), result.end(), SampleTimestampComparator{});
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
  for (const auto _ : this->_core_ids) {
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
