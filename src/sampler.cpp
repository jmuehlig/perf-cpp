#include <algorithm>
#include <asm/unistd.h>
#include <cstring>
#include <iostream>
#include <perfcpp/sampler.h>
#include <stdexcept>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <utility>

perf::Sampler::Sampler(const perf::CounterDefinition& counter_list,
                       std::vector<std::string>&& counter_names,
                       const std::uint64_t type,
                       perf::SampleConfig config)
  : _counter_definitions(counter_list)
  , _config(config)
{
  _values.instruction_pointer(static_cast<bool>(type & PERF_SAMPLE_IP))
    .thread_id(static_cast<bool>(type & PERF_SAMPLE_TID))
    .time(static_cast<bool>(type & PERF_SAMPLE_TIME))
    .logical_memory_address(static_cast<bool>(type & PERF_SAMPLE_ADDR))
    .callchain(static_cast<bool>(type & PERF_SAMPLE_CALLCHAIN))
    .cpu_id(static_cast<bool>(type & PERF_SAMPLE_CPU))
    .weight(static_cast<bool>(type & PERF_SAMPLE_WEIGHT))
    .data_src(static_cast<bool>(type & PERF_SAMPLE_DATA_SRC))
    .identifier(static_cast<bool>(type & PERF_SAMPLE_IDENTIFIER))
#ifndef PERFCPP_NO_SAMPLE_PHYS_ADDR
    .physical_memory_address(static_cast<bool>(type & PERF_SAMPLE_PHYS_ADDR))
#endif
    .data_page_size(static_cast<bool>(type & Type::DataPageSize))
    .code_page_size(static_cast<bool>(type & Type::CodePageSize))
    .weight_struct(static_cast<bool>(type & Type::WeightStruct));

  if (_values.is_set(PERF_SAMPLE_CALLCHAIN)) {
    _values.callchain(this->_config.max_stack());
  }

  if (static_cast<bool>(type & PERF_SAMPLE_BRANCH_STACK)) {
    _values._branch_mask = config.branch_type();
    _values.set(PERF_SAMPLE_BRANCH_STACK, true);
  }

  if (static_cast<bool>(type & PERF_SAMPLE_REGS_USER)) {
    _values.user_registers(config.user_registers());
  }

  if (static_cast<bool>(type & PERF_SAMPLE_REGS_INTR)) {
    _values.kernel_registers(config.kernel_registers());
  }

  /// Extract trigger(s) (the first counter and the second, if the first is an aux counter)
  /// and counter names that will be added to samples.
  bool has_auxiliary = false;
  auto trigger_names = std::vector<std::string>{};
  auto read_counter_names = std::vector<std::string>{};
  for (auto counter_id = 0U; counter_id < counter_names.size(); ++counter_id) {
    auto& counter_name = counter_names[counter_id];
    if (!this->_counter_definitions.is_metric(counter_name)) /// Metrics are not (yet) supported.
    {
      if (auto counter_config = this->_counter_definitions.counter(counter_name); counter_config.has_value()) {
        if (counter_id == 0U && counter_config->second.is_auxiliary()) {
          has_auxiliary = true;
        }
        if ((has_auxiliary && counter_id < 2U) || counter_id < 1U) {
          trigger_names.push_back(std::move(counter_name));
        } else {
          read_counter_names.emplace_back(std::move(counter_name));
        }
      }
    }
  }

  /// Add trigger and counters.
  this->trigger(std::move(trigger_names));
  if (!read_counter_names.empty()) {
    this->_values.counter(std::move(read_counter_names));
  }
}

perf::Sampler&
perf::Sampler::trigger(std::vector<std::vector<std::string>>&& list_of_trigger_names)
{
  auto triggers = std::vector<std::vector<Trigger>>{};
  triggers.reserve(triggers.size());

  for (auto& trigger_names : list_of_trigger_names) {
    auto trigger_with_precision = std::vector<Trigger>{};

    trigger_with_precision.reserve(trigger_names.size());
    for (auto& trigger_name : trigger_names) {
      trigger_with_precision.emplace_back(std::move(trigger_name));
    }

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
    for (auto& trigger : trigger_group) {
      if (!this->_counter_definitions.is_metric(trigger.name())) {
        if (auto counter_config = this->_counter_definitions.counter(trigger.name()); counter_config.has_value()) {
          trigger_group_references.emplace_back(
            std::get<0>(counter_config.value()), trigger.precision(), trigger.period_or_frequency());
        } else {
          throw std::runtime_error{ std::string{ "Cannot find counter '" }.append(trigger.name()).append("'.") };
        }
      } else {
        throw std::runtime_error{ std::string{ "Counter '" }
                                    .append(trigger.name())
                                    .append("' seems to be a metric. Metrics are not supported as triggers.") };
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
  if (std::exchange(this->_is_opened, true)) {
    return;
  }

  /// Build the groups from triggers + counters from values.
  for (const auto& trigger_group : this->_triggers) {
    auto group = Group{};
    auto counter_names = std::vector<std::string_view>{};

    /// Add the trigger(s) to the group.
    for (const auto trigger : trigger_group) {
      if (auto counter_name_and_config = this->_counter_definitions.counter(std::get<0>(trigger));
          counter_name_and_config.has_value()) {
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

        if (this->_values.is_set(PERF_SAMPLE_READ)) {
          counter_names.push_back(std::get<0>(trigger));
        }
      }
    }

    if (!group.empty()) {
      /// Add possible counters as value to the sample.
      if (this->_values.is_set(PERF_SAMPLE_READ)) {

        for (const auto& counter_name : this->_values.counters()) {

          /// Validate the counter is not a metric.
          if (!this->_counter_definitions.is_metric(counter_name)) {

            /// Find the counter.
            if (auto counter_config = this->_counter_definitions.counter(counter_name); counter_config.has_value()) {
              /// Add the counter to the group and to the list of counters.
              counter_names.push_back(std::get<0>(counter_config.value()));
              group.add(std::get<1>(counter_config.value()));
            } else {
              throw std::runtime_error{ std::string{ "Cannot find counter '" }.append(counter_name).append("'.") };
            }
          } else {
            throw std::runtime_error{ std::string{ "Counter '" }
                                        .append(counter_name)
                                        .append("' seems to be a metric. Metrics are not supported for sampling.") };
          }
        }
      }

      if (!counter_names.empty()) {
        this->_sample_counter.emplace_back(std::move(group), std::move(counter_names));
      } else {
        this->_sample_counter.emplace_back(std::move(group));
      }
    }
  }

  /// Open the groups.
  if (this->_sample_counter.empty()) {
    throw std::runtime_error{ "No trigger for sampling specified." };
  }

  for (auto& sample_counter : this->_sample_counter) {
    /// Detect, if the leader is an auxiliary (specifically for Sapphire Rapids).
    const auto is_leader_auxiliary_counter = sample_counter.group().member(0U).is_auxiliary();

    for (auto counter_index = 0U; counter_index < sample_counter.group().size(); ++counter_index) {
      auto& counter = sample_counter.group().member(counter_index);

      /// The first counter in the group has a "special" role, others will use its file descriptor.
      const auto is_leader = counter_index == 0U;

      /// For Intel's Sapphire Rapids architecture, sampling for memory requires a dummy as first counter.
      /// Only the second counter is the "real" sampling counter.
      const auto is_secret_leader = is_leader_auxiliary_counter && counter_index == 1U;

      auto& perf_event = counter.event_attribute();
      std::memset(&perf_event, 0, sizeof(perf_event_attr));
      perf_event.type = counter.type();
      perf_event.size = sizeof(perf_event_attr);
      perf_event.config = counter.event_id();
      perf_event.config1 = counter.event_id_extension()[0U];
      perf_event.config2 = counter.event_id_extension()[1U];
      perf_event.disabled = is_leader;

      perf_event.inherit = static_cast<std::int32_t>(this->_config.is_include_child_threads());
      perf_event.exclude_kernel = static_cast<std::int32_t>(!this->_config.is_include_kernel());
      perf_event.exclude_user = static_cast<std::int32_t>(!this->_config.is_include_user());
      perf_event.exclude_hv = static_cast<std::int32_t>(!this->_config.is_include_hypervisor());
      perf_event.exclude_idle = static_cast<std::int32_t>(!this->_config.is_include_idle());
      perf_event.exclude_guest = static_cast<std::int32_t>(!this->_config.is_include_guest());

      if (is_leader || is_secret_leader) {
        perf_event.sample_type = this->_values.get();
        perf_event.sample_id_all = 1U;

        /// Set period of frequency.
        perf_event.freq = static_cast<std::uint64_t>(counter.is_frequency());
        perf_event.sample_freq = counter.period_or_frequency();

        if (this->_values.is_set(PERF_SAMPLE_BRANCH_STACK)) {
          perf_event.branch_sample_type = this->_values.branch_mask();
        }

#ifndef PERFCPP_NO_SAMPLE_MAX_STACK
        if (this->_values.is_set(PERF_SAMPLE_CALLCHAIN)) {
          perf_event.sample_max_stack = this->_values.max_call_stack();
        }
#endif

        if (this->_values.is_set(PERF_SAMPLE_REGS_USER)) {
          perf_event.sample_regs_user = this->_values.user_registers().mask();
        }

        if (this->_values.is_set(PERF_SAMPLE_REGS_INTR)) {
          perf_event.sample_regs_intr = this->_values.kernel_registers().mask();
        }

        perf_event.context_switch = static_cast<std::uint8_t>(this->_values._is_include_context_switch);
        perf_event.cgroup = this->_values.is_set(PERF_SAMPLE_CGROUP) ? 1U : 0U;
      }

      if (this->_values.is_set(PERF_SAMPLE_READ)) {
        perf_event.read_format = PERF_FORMAT_GROUP | PERF_FORMAT_ID;
      }

      const std::int32_t cpu_id =
        this->_config.cpu_id().has_value() ? std::int32_t{ this->_config.cpu_id().value() } : -1;

      /// Open the counter. Try to decrease the precise_ip if the file syscall was not successful, reporting an invalid
      /// argument.
      std::int64_t file_descriptor;
      for (auto precise_ip = std::int32_t{ counter.precise_ip() }; precise_ip > -1; --precise_ip) {
        perf_event.precise_ip = std::uint64_t(precise_ip);

        counter.precise_ip(
          static_cast<std::uint8_t>(precise_ip)); /// Set back to counter in case the counter is debugged and the user
                                                  /// wants to see the precise_ip that worked.

        file_descriptor = ::syscall(__NR_perf_event_open,
                                    &perf_event,
                                    this->_config.process_id(),
                                    cpu_id,
                                    sample_counter.group().leader_file_descriptor(),
                                    0);

        /// If opening the file descriptor was successfully, we can start the counter.
        /// Otherwise, we will try to decrease the precision and try again.
        if (file_descriptor > -1 || (errno != EINVAL && errno != EOPNOTSUPP)) {
          break;
        }
      }
      counter.file_descriptor(file_descriptor);

      /// Print debug output, if requested.
      if (this->_config.is_debug()) {
        std::cout << counter.to_string() << std::flush;
      }

      /// Check if the counter could be opened successfully.
      if (file_descriptor < 0) {
        this->_last_error = errno;
        throw std::runtime_error{ std::string{ "Cannot create file descriptor for sampling counter (error no: " }
                                    .append(std::to_string(errno))
                                    .append(").") };
      }
    }

    /// Open the mapped buffer.
    /// If the leader is an "auxiliary" counter (like on Sapphire Rapid), use the second counter instead.
    const auto file_descriptor = is_leader_auxiliary_counter && sample_counter.group().size() > 1U
                                   ? sample_counter.group().member(1U).file_descriptor()
                                   : sample_counter.group().leader_file_descriptor();
    auto* buffer = ::mmap(nullptr,
                          this->_config.buffer_pages() * 4096U,
                          PROT_READ,
                          MAP_SHARED,
                          static_cast<std::int32_t>(file_descriptor),
                          0);
    if (buffer == MAP_FAILED) {
      this->_last_error = errno;
      throw std::runtime_error{ "Creating buffer via mmap() failed." };
    }

    if (buffer == nullptr) {
      throw std::runtime_error{ "Created buffer via mmap() is null." };
    }

    sample_counter.buffer(buffer);
  }
}

bool
perf::Sampler::start()
{
  /// Open the groups.
  this->open();

  /// Start the counters.
  for (const auto& sample_counter : this->_sample_counter) {
    ::ioctl(sample_counter.group().leader_file_descriptor(), PERF_EVENT_IOC_RESET, 0);
    ::ioctl(sample_counter.group().leader_file_descriptor(), PERF_EVENT_IOC_ENABLE, 0);
  }

  return true;
}

void
perf::Sampler::stop()
{
  for (const auto& sample_counter : this->_sample_counter) {
    ::ioctl(sample_counter.group().leader_file_descriptor(), PERF_EVENT_IOC_DISABLE, 0);
  }
}

void
perf::Sampler::close()
{
  /// Free all buffers and close all groups.
  for (auto& sample_counter : this->_sample_counter) {
    if (sample_counter.buffer()) {
      ::munmap(sample_counter.buffer(), this->_config.buffer_pages() * 4096U);
    }

    if (sample_counter.group().leader_file_descriptor() > -1) {
      sample_counter.group().close();
    }
  }

  /// Clear all buffers, groups, and counter names
  /// in order to enable opening again.
  this->_sample_counter.clear();
  this->_is_opened = false;
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
        result.push_back(this->read_cgroup_event(entry));
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
    const auto count_counter_values = entry.read<typeof(Sampler::read_format::count_members)>();

    /// Read the counters (if the number matches the number of specified counters).
    auto* counter_values = entry.read<read_format::value>(count_counter_values);
    if (count_counter_values == sample_counter.group().size()) {
      auto counter_results = std::vector<std::pair<std::string_view, double>>{};

      /// Add each counter and its value to the result set of the sample.
      for (auto counter_id = 0U; counter_id < sample_counter.group().size(); ++counter_id) {
        const auto counter_name = sample_counter.counter_names()[counter_id];
        const auto counter_result = double(counter_values[counter_id].value);
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

      sample.user_registers(std::move(kernel_registers));
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
perf::Sampler::read_cgroup_event(perf::Sampler::UserLevelBufferEntry entry) const
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
                                             std::vector<std::string>&& counter_names,
                                             const std::uint64_t type,
                                             const std::uint16_t num_threads,
                                             const perf::SampleConfig config)
  : MultiSamplerBase(config)
{
  /// Setup new values field (will be transferred to samplers at start).
  _values.instruction_pointer(static_cast<bool>(type & PERF_SAMPLE_IP))
    .thread_id(static_cast<bool>(type & PERF_SAMPLE_TID))
    .time(static_cast<bool>(type & PERF_SAMPLE_TIME))
    .logical_memory_address(static_cast<bool>(type & PERF_SAMPLE_ADDR))
    .callchain(static_cast<bool>(type & PERF_SAMPLE_CALLCHAIN))
    .cpu_id(static_cast<bool>(type & PERF_SAMPLE_CPU))
    .weight(static_cast<bool>(type & PERF_SAMPLE_WEIGHT))
    .data_src(static_cast<bool>(type & PERF_SAMPLE_DATA_SRC))
    .identifier(static_cast<bool>(type & PERF_SAMPLE_IDENTIFIER))
#ifndef PERFCPP_NO_SAMPLE_PHYS_ADDR
    .physical_memory_address(static_cast<bool>(type & PERF_SAMPLE_PHYS_ADDR))
#endif
    .data_page_size(static_cast<bool>(type & Sampler::Type::DataPageSize))
    .code_page_size(static_cast<bool>(type & Sampler::Type::CodePageSize))
    .weight_struct(static_cast<bool>(type & Sampler::Type::WeightStruct));

  if (static_cast<bool>(type & PERF_SAMPLE_BRANCH_STACK)) {
    _values._branch_mask = config.branch_type();
    _values.set(PERF_SAMPLE_BRANCH_STACK, true);
  }

  if (static_cast<bool>(type & PERF_SAMPLE_REGS_USER)) {
    _values.user_registers(config.user_registers());
  }

  if (static_cast<bool>(type & PERF_SAMPLE_REGS_INTR)) {
    _values.kernel_registers(config.kernel_registers());
  }

  /// Extract trigger(s) (the first counter and the second, if the first is an aux counter)
  /// and counter names that will be added to samples.
  bool has_auxiliary = false;
  auto trigger_names = std::vector<std::string>{};
  auto read_counter_names = std::vector<std::string>{};
  for (auto counter_id = 0U; counter_id < counter_names.size(); ++counter_id) {
    auto& counter_name = counter_names[counter_id];
    if (!counter_list.is_metric(counter_name)) /// Metrics are not (yet) supported.
    {
      if (auto counter_config = counter_list.counter(counter_name); counter_config.has_value()) {
        if (counter_id == 0U && counter_config->second.is_auxiliary()) {
          has_auxiliary = true;
        }
        if ((has_auxiliary && counter_id < 2U) || counter_id < 1U) {
          trigger_names.push_back(std::move(counter_name));
        } else {
          read_counter_names.emplace_back(std::move(counter_name));
        }
      }
    }
  }

  /// Add trigger and counters.
  if (!read_counter_names.empty()) {
    this->_values.counter(std::move(read_counter_names));
  }

  /// Create thread-local samplers without config (will be set when starting).
  for (auto thread_id = 0U; thread_id < num_threads; ++thread_id) {
    auto& thread_sampler = this->_thread_local_samplers.emplace_back(counter_list);
    if (!trigger_names.empty()) {
      thread_sampler.trigger(std::vector<std::vector<std::string>>{ trigger_names });
    }
  }
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
                                         std::vector<std::string>&& counter_names,
                                         const std::uint64_t type,
                                         std::vector<std::uint16_t>&& core_ids,
                                         perf::SampleConfig config)
  : MultiSamplerBase(config)
  , _core_ids(std::move(core_ids))
{
  /// Record all processes on the CPUs.
  _config.process_id(-1);

  /// Setup new values field (will be transferred to samplers at start).
  _values.instruction_pointer(static_cast<bool>(type & PERF_SAMPLE_IP))
    .thread_id(static_cast<bool>(type & PERF_SAMPLE_TID))
    .time(static_cast<bool>(type & PERF_SAMPLE_TIME))
    .logical_memory_address(static_cast<bool>(type & PERF_SAMPLE_ADDR))
    .callchain(static_cast<bool>(type & PERF_SAMPLE_CALLCHAIN))
    .cpu_id(static_cast<bool>(type & PERF_SAMPLE_CPU))
    .weight(static_cast<bool>(type & PERF_SAMPLE_WEIGHT))
    .data_src(static_cast<bool>(type & PERF_SAMPLE_DATA_SRC))
    .identifier(static_cast<bool>(type & PERF_SAMPLE_IDENTIFIER))
#ifndef PERFCPP_NO_SAMPLE_PHYS_ADDR
    .physical_memory_address(static_cast<bool>(type & PERF_SAMPLE_PHYS_ADDR))
#endif
    .data_page_size(static_cast<bool>(type & Sampler::Type::DataPageSize))
    .code_page_size(static_cast<bool>(type & Sampler::Type::CodePageSize))
    .weight_struct(static_cast<bool>(type & Sampler::Type::WeightStruct));

  if (static_cast<bool>(type & PERF_SAMPLE_BRANCH_STACK)) {
    _values._branch_mask = config.branch_type();
    _values.set(PERF_SAMPLE_BRANCH_STACK, true);
  }

  if (static_cast<bool>(type & PERF_SAMPLE_REGS_USER)) {
    _values.user_registers(config.user_registers());
  }

  if (static_cast<bool>(type & PERF_SAMPLE_REGS_INTR)) {
    _values.kernel_registers(config.kernel_registers());
  }

  /// Extract trigger(s) (the first counter and the second, if the first is an aux counter)
  /// and counter names that will be added to samples.
  bool has_auxiliary = false;
  auto trigger_names = std::vector<std::string>{};
  auto read_counter_names = std::vector<std::string>{};
  for (auto counter_id = 0U; counter_id < counter_names.size(); ++counter_id) {
    auto& counter_name = counter_names[counter_id];
    if (!counter_list.is_metric(counter_name)) /// Metrics are not (yet) supported.
    {
      if (auto counter_config = counter_list.counter(counter_name); counter_config.has_value()) {
        if (counter_id == 0U && counter_config->second.is_auxiliary()) {
          has_auxiliary = true;
        }
        if ((has_auxiliary && counter_id < 2U) || counter_id < 1U) {
          trigger_names.push_back(std::move(counter_name));
        } else {
          read_counter_names.emplace_back(std::move(counter_name));
        }
      }
    }
  }

  /// Add trigger and counters.
  if (!read_counter_names.empty()) {
    this->_values.counter(std::move(read_counter_names));
  }

  /// Create thread-local samplers without config (will be set when starting).
  for (const auto _ : this->_core_ids) {
    auto& cpu_sampler = this->_core_local_samplers.emplace_back(counter_list);
    if (!trigger_names.empty()) {
      cpu_sampler.trigger(std::vector<std::vector<std::string>>{ trigger_names });
    }
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